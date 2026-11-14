/* Copyright (C) 2018-2026 Free Software Foundation, Inc.
   Contributed by Nicolas Koenig

   This file is part of the GNU Fortran runtime library (libgfortran).

   Libgfortran is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgfortran is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include "libgfortran.h"

#define _GTHREAD_USE_COND_INIT_FUNC
#include "../../libgcc/gthr.h"
#include "io.h"
#include "fbuf.h"
#include "format.h"
#include "unix.h"
#include <string.h>
#include <assert.h>

#include <sys/types.h>

#include "async.h"
#if ASYNC_IO

#if defined(__linux__) && defined(_GNU_SOURCE)
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#define HAVE_ASYNC_IO_AFFINITY 1
#endif

/* Apply GFORTRAN_ASYNC_IO_CPUS to the calling (I/O) thread.  Malformed
   lists are ignored.  */

static void
apply_io_thread_affinity (void)
{
#ifdef HAVE_ASYNC_IO_AFFINITY
  const char *p = options.async_io_cpus;
  cpu_set_t set;
  long ncpu;
  int any = 0;

  if (p == NULL)
    return;
  ncpu = sysconf (_SC_NPROCESSORS_CONF);
  if (ncpu <= 0 || ncpu > CPU_SETSIZE)
    ncpu = CPU_SETSIZE;
  CPU_ZERO (&set);
  if (strcmp (p, "all") == 0)
    {
      for (long c = 0; c < ncpu; c++)
	CPU_SET (c, &set);
      any = 1;
    }
  else
    while (*p)
      {
	char *end;
	long a = strtol (p, &end, 10), b;
	if (end == p)
	  return;
	b = a;
	p = end;
	if (*p == '-')
	  {
	    b = strtol (p + 1, &end, 10);
	    if (end == p + 1)
	      return;
	    p = end;
	  }
	for (long c = a; c <= b; c++)
	  if (c >= 0 && c < ncpu)
	    {
	      CPU_SET (c, &set);
	      any = 1;
	    }
	if (*p == ',')
	  p++;
	else if (*p)
	  return;
      }
  if (any)
    pthread_setaffinity_np (pthread_self (), sizeof (set), &set);
#endif
}

DEBUG_LINE (__thread const char *aio_prefix = MPREFIX);

DEBUG_LINE (__gthread_mutex_t debug_queue_lock = __GTHREAD_MUTEX_INIT;)
DEBUG_LINE (aio_lock_debug *aio_debug_head = NULL;)
#ifdef __GTHREAD_RWLOCK_INIT
DEBUG_LINE (aio_rwlock_debug *aio_rwlock_debug_head = NULL;)
DEBUG_LINE (__gthread_rwlock_t debug_queue_rwlock = __GTHREAD_RWLOCK_INIT;)
#endif

/* Current unit for asynchronous I/O.  Needed for error reporting.  */

__thread gfc_unit *thread_unit = NULL;

/* Queue entry for the asynchronous I/O entry.  */
typedef struct transfer_queue
{
  enum aio_do type;
  struct transfer_queue *next;
  struct st_parameter_dt *new_pdt;
  transfer_args arg;
  _Bool has_id;
  int read_flag;
  /* Buffer holding a copy of the data (GFORTRAN_ASYNC_COPY), freed by the
     I/O thread once the item has been processed, and its size.  */
  void *owned;
  size_t owned_bytes;
  size_t owned_cap;
} transfer_queue;

struct error {
  st_parameter_dt *dtp;
  int id;
};

/* Released copy-on-enqueue buffers kept for reuse, so that repeated
   writes do not pay for page faults on freshly allocated memory each
   time.  The pool is process-wide because programs typically open a new
   file for every restart dump, which would discard a per-unit pool.  A
   buffer is only ever allocated when no pooled one fits, so the pool can
   not grow beyond the peak amount of copies alive at once; in addition
   GFORTRAN_ASYNC_COPY_LIMIT caps it when set.  */

struct async_spare {
  void *buf;
  size_t cap;
  struct async_spare *next;
};

static struct async_spare *spare_pool = NULL;
static size_t spare_pool_bytes = 0;
static __gthread_mutex_t spare_pool_lock = __GTHREAD_MUTEX_INIT;

/* Hand a released buffer to the pool or free it.  */

static void
spare_release (void *buf, size_t cap)
{
  size_t limit = options.async_copy_limit > 0
		 ? (size_t) options.async_copy_limit << 20 : 0;
  LOCK (&spare_pool_lock);
  if (limit == 0 || spare_pool_bytes + cap <= limit)
    {
      struct async_spare *s = xmalloc (sizeof (struct async_spare));
      s->buf = buf;
      s->cap = cap;
      s->next = spare_pool;
      spare_pool = s;
      spare_pool_bytes += cap;
      buf = NULL;
    }
  UNLOCK (&spare_pool_lock);
  free (buf);
}

/* Helper function to exchange the old vs. a new PDT.  */

static void
update_pdt (st_parameter_dt **old, st_parameter_dt *new) {
  st_parameter_dt *temp;
  NOTE ("Changing pdts, current_unit = %p", (void *) (new->u.p.current_unit));
  temp = *old;
  *old = new;
  free (temp);
}

/* Destroy an adv_cond structure.  */

static void
destroy_adv_cond (struct adv_cond *ac)
{
  T_ERROR (__gthread_cond_destroy, &ac->signal);
}

/* Function invoked as start routine for a new asynchronous I/O unit.
   Contains the main loop for accepting requests and handling them.  */

static void *
async_io (void *arg)
{
  DEBUG_LINE (aio_prefix = TPREFIX);
  transfer_queue *ctq = NULL, *prev = NULL;
  gfc_unit *u = (gfc_unit *) arg;
  async_unit *au = u->au;
  apply_io_thread_affinity ();
  LOCK (&au->lock);
  thread_unit = u;
  au->thread = __gthread_self ();
  while (true)
    {
      /* Main loop.  At this point, au->lock is always held. */
      WAIT_SIGNAL_MUTEX (&au->work, au->tail != NULL, &au->lock);
      LOCK (&au->lock);
      ctq = au->head;
      prev = NULL;
      /* Loop over the queue entries until they are finished.  */
      while (ctq)
	{
	  free (prev);
	  prev = ctq;
	  if (!au->error.has_error)
	    {
	      UNLOCK (&au->lock);

	      switch (ctq->type)
		{
		case AIO_WRITE_DONE:
		  NOTE ("Finalizing write");
		  st_write_done_worker (au->pdt, false);
		  UNLOCK (&au->io_lock);
		  break;

		case AIO_READ_DONE:
		  NOTE ("Finalizing read");
		  st_read_done_worker (au->pdt, false);
		  UNLOCK (&au->io_lock);
		  break;

		case AIO_DATA_TRANSFER_INIT:
		  NOTE ("Data transfer init");
		  LOCK (&au->io_lock);
		  update_pdt (&au->pdt, ctq->new_pdt);
		  data_transfer_init_worker (au->pdt, ctq->read_flag);
		  break;

		case AIO_TRANSFER_SCALAR:
		  NOTE ("Starting scalar transfer");
		  ctq->arg.scalar.transfer (au->pdt, ctq->arg.scalar.arg_bt,
					    ctq->arg.scalar.data,
					    ctq->arg.scalar.i,
					    ctq->arg.scalar.s1,
					    ctq->arg.scalar.s2);
		  break;

		case AIO_TRANSFER_ARRAY:
		  NOTE ("Starting array transfer");
		  NOTE ("ctq->arg.array.desc = %p",
			(void *) (ctq->arg.array.desc));
		  transfer_array_inner (au->pdt, ctq->arg.array.desc,
					ctq->arg.array.kind,
					ctq->arg.array.charlen);
		  free (ctq->arg.array.desc);
		  break;

		case AIO_CLOSE:
		  NOTE ("Received AIO_CLOSE");
		  LOCK (&au->lock);
		  goto finish_thread;

		default:
		  internal_error (NULL, "Invalid queue type");
		  break;
		}
	      LOCK (&au->lock);
	      if (unlikely (au->error.has_error))
		au->error.last_good_id = au->id.low - 1;
	    }
	  else
	    {
	      if (ctq->type == AIO_WRITE_DONE || ctq->type == AIO_READ_DONE)
		{
		  UNLOCK (&au->io_lock);
		}
	      else if (ctq->type == AIO_CLOSE)
		{
		  NOTE ("Received AIO_CLOSE during error condition");
		  goto finish_thread;
		}
	      else if (ctq->type == AIO_DATA_TRANSFER_INIT)
		{
		  LOCK (&au->io_lock);
		  free (ctq->new_pdt);
		}
	      else if (ctq->type == AIO_TRANSFER_ARRAY)
		free (ctq->arg.array.desc);
	    }

  	  if (ctq->owned)
	    {
	      spare_release (ctq->owned, ctq->owned_cap);
	      au->copy_bytes_outstanding -= ctq->owned_bytes;
	    }
  	  NOTE ("Next ctq, current id: %d", au->id.low);
  	  if (ctq->has_id && au->id.waiting == au->id.low++)
	    SIGNAL (&au->id.done);

	  ctq = ctq->next;
	}
      au->tail = NULL;
      au->head = NULL;
      au->empty = 1;
      SIGNAL (&au->emptysignal);
    }
 finish_thread:
  au->tail = NULL;
  au->head = NULL;
  au->empty = 1;
  SIGNAL (&au->emptysignal);
  free (ctq);
  UNLOCK (&au->lock);
  return NULL;
}

/* Free an asynchronous unit.  */

static void
free_async_unit (async_unit *au)
{
  if (au->tail)
    internal_error (NULL, "Trying to free nonempty asynchronous unit");

  destroy_adv_cond (&au->work);
  destroy_adv_cond (&au->emptysignal);
  destroy_adv_cond (&au->id.done);
  T_ERROR (__gthread_mutex_destroy, &au->lock);
  free (au);
}

/* Initialize an adv_cond structure.  */

static void
init_adv_cond (struct adv_cond *ac)
{
  ac->pending = 0;
  __GTHREAD_COND_INIT_FUNCTION (&ac->signal);
}

/* Initialize an asynchronous unit, returning zero on success,
 nonzero on failure.  It also sets u->au.  */

void
init_async_unit (gfc_unit *u)
{
  async_unit *au;
  if (!__gthread_active_p ())
    {
      u->au = NULL;
      return;
    }
  
  au = (async_unit *) xmalloc (sizeof (async_unit));
  u->au = au;
  init_adv_cond (&au->work);
  init_adv_cond (&au->emptysignal);
  __GTHREAD_MUTEX_INIT_FUNCTION (&au->lock);
  __GTHREAD_MUTEX_INIT_FUNCTION (&au->io_lock);
  LOCK (&au->lock);
  T_ERROR (__gthread_create, &au->thread, &async_io, (void *) u);
  au->pdt = NULL;
  au->head = NULL;
  au->tail = NULL;
  au->copy_bytes_outstanding = 0;
  au->empty = true;
  au->id.waiting = -1;
  au->id.low = 0;
  au->id.high = 0;
  au->error.fatal_error = 0;
  au->error.has_error = 0;
  au->error.last_good_id = 0;
  init_adv_cond (&au->id.done);
  UNLOCK (&au->lock);
}

/* Enqueue a transfer statement.  */

void
enqueue_transfer (async_unit *au, transfer_args *arg, enum aio_do type,
		  void *owned, size_t owned_bytes, size_t owned_cap)
{
  transfer_queue *tq = calloc (1, sizeof (transfer_queue));
  tq->arg = *arg;
  tq->type = type;
  tq->has_id = 0;
  tq->owned = owned;
  tq->owned_bytes = owned_bytes;
  tq->owned_cap = owned_cap;
  LOCK (&au->lock);
  au->copy_bytes_outstanding += owned_bytes;
  if (!au->tail)
    au->head = tq;
  else
    au->tail->next = tq;
  au->tail = tq;
  REVOKE_SIGNAL (&(au->emptysignal));
  au->empty = false;
  SIGNAL (&au->work);
  UNLOCK (&au->lock);
}

/* Enqueue an st_write_done or st_read_done which contains an ID.  */

int
enqueue_done_id (async_unit *au, enum aio_do type)
{
  int ret;
  transfer_queue *tq = calloc (1, sizeof (transfer_queue));

  tq->type = type;
  tq->has_id = 1;
  LOCK (&au->lock);
  if (!au->tail)
    au->head = tq;
  else
    au->tail->next = tq;
  au->tail = tq;
  REVOKE_SIGNAL (&(au->emptysignal));
  au->empty = false;
  ret = au->id.high++;
  NOTE ("Enqueue id: %d", ret);
  SIGNAL (&au->work);
  UNLOCK (&au->lock);
  return ret;
}

/* Enqueue an st_write_done or st_read_done without an ID.  */

void
enqueue_done (async_unit *au, enum aio_do type)
{
  transfer_queue *tq = calloc (1, sizeof (transfer_queue));
  tq->type = type;
  tq->has_id = 0;
  LOCK (&au->lock);
  if (!au->tail)
    au->head = tq;
  else
    au->tail->next = tq;
  au->tail = tq;
  REVOKE_SIGNAL (&(au->emptysignal));
  au->empty = false;
  SIGNAL (&au->work);
  UNLOCK (&au->lock);
}

/* Enqueue a CLOSE statement.  */

void
enqueue_close (async_unit *au)
{
  transfer_queue *tq = calloc (1, sizeof (transfer_queue));

  tq->type = AIO_CLOSE;
  LOCK (&au->lock);
  if (!au->tail)
    au->head = tq;
  else
    au->tail->next = tq;
  au->tail = tq;
  REVOKE_SIGNAL (&(au->emptysignal));
  au->empty = false;
  SIGNAL (&au->work);
  UNLOCK (&au->lock);
}

/* The asynchronous unit keeps the currently active PDT around.
   This function changes that to the current one.  */

void
enqueue_data_transfer_init (async_unit *au, st_parameter_dt *dt, int read_flag)
{
  st_parameter_dt *new = xmalloc (sizeof (st_parameter_dt));
  transfer_queue *tq = xmalloc (sizeof (transfer_queue));

  memcpy ((void *) new, (void *) dt, sizeof (st_parameter_dt));

  NOTE ("dt->internal_unit_desc = %p", dt->internal_unit_desc);
  NOTE ("common.flags & mask = %d", dt->common.flags & IOPARM_LIBRETURN_MASK);
  tq->next = NULL;
  tq->type = AIO_DATA_TRANSFER_INIT;
  tq->read_flag = read_flag;
  tq->has_id = 0;
  tq->new_pdt = new;
  tq->owned = NULL;
  tq->owned_bytes = 0;
  LOCK (&au->lock);

  if (!au->tail)
    au->head = tq;
  else
    au->tail->next = tq;
  au->tail = tq;
  REVOKE_SIGNAL (&(au->emptysignal));
  au->empty = false;
  SIGNAL (&au->work);
  UNLOCK (&au->lock);
}

/* Collect the errors that may have happened asynchronously.  Return true if
   an error has been encountered.  */

bool
collect_async_errors (st_parameter_common *cmp, async_unit *au)
{
  bool has_error = au->error.has_error;

  if (has_error)
    {
      if (generate_error_common (cmp, au->error.family, au->error.message))
	{
	  au->error.has_error = 0;
	  au->error.cmp = NULL;
	}
      else
	{
	  /* The program will exit later.  */
	  au->error.fatal_error = true;
	}
    }
  return has_error;
}

/* Perform a wait operation on an asynchronous unit with an ID specified,
   which means collecting the errors that may have happened asynchronously.
   Return true if an error has been encountered.  */

bool
async_wait_id (st_parameter_common *cmp, async_unit *au, int i)
{
  bool ret;

  if (au == NULL)
    return false;

  if (cmp == NULL)
    cmp = au->error.cmp;

  if (au->error.has_error)
    {
      if (i <= au->error.last_good_id)
	return false;

      return collect_async_errors (cmp, au);
    }

  LOCK (&au->lock);
  if (i < 0 || i >= au->id.high)
    {
      generate_error_common (cmp, LIBERROR_BAD_WAIT_ID, NULL);
      UNLOCK (&au->lock);
      return true;
    }

  NOTE ("Waiting for id %d", i);
  if (au->id.waiting < i)
    au->id.waiting = i;
  SIGNAL (&(au->work));
  REVOKE_SIGNAL (&(au->id.done));
  WAIT_SIGNAL_MUTEX (&(au->id.done),
		     (au->id.low > au->id.waiting || au->empty), &au->lock);
  LOCK (&au->lock);
  ret = collect_async_errors (cmp, au);
  UNLOCK (&au->lock);
  return ret;
}

/* Return a buffer of at least NBYTES for a copy-on-enqueue transfer,
   reusing the unit's spare buffer when it is large enough.  The capacity
   of the returned buffer is stored in *CAP.  */

void *
async_copy_buffer (async_unit *au, size_t nbytes, size_t *cap)
{
  void *buf = NULL;
  struct async_spare **best = NULL, **pp;
  LOCK (&spare_pool_lock);
  /* Best fit: the smallest pooled buffer that is large enough.  */
  for (pp = &spare_pool; *pp; pp = &(*pp)->next)
    if ((*pp)->cap >= nbytes && (best == NULL || (*pp)->cap < (*best)->cap))
      best = pp;
  if (best)
    {
      struct async_spare *s = *best;
      *best = s->next;
      buf = s->buf;
      *cap = s->cap;
      spare_pool_bytes -= s->cap;
      free (s);
    }
  UNLOCK (&spare_pool_lock);
  if (buf)
    return buf;
  *cap = nbytes;
  return xmalloc (nbytes);
}

/* Parallel memcpy for copy-on-enqueue.  Items of at least
   ASYNC_COPY_PAR_MIN bytes are split across GFORTRAN_ASYNC_COPY_THREADS
   threads, one chunk being copied by the calling thread.  The helper
   threads are created per call; at this size their start-up cost is
   negligible against the copy itself.  */

#define ASYNC_COPY_PAR_MIN (8u << 20)
#define ASYNC_COPY_MAX_THREADS 64

struct copy_job
{
  void *dst;
  const void *src;
  size_t n;
};

static void *
copy_job_run (void *arg)
{
  struct copy_job *j = arg;
  memcpy (j->dst, j->src, j->n);
  return NULL;
}

void
async_copy_mem (void *dst, const void *src, size_t nbytes)
{
  int nthreads = options.async_copy_threads;
  __gthread_t tid[ASYNC_COPY_MAX_THREADS];
  struct copy_job job[ASYNC_COPY_MAX_THREADS];
  size_t chunk, off;
  int i, started;

  if (nthreads > ASYNC_COPY_MAX_THREADS)
    nthreads = ASYNC_COPY_MAX_THREADS;
  if (nthreads < 2 || nbytes < ASYNC_COPY_PAR_MIN || !__gthread_active_p ())
    {
      memcpy (dst, src, nbytes);
      return;
    }

  /* Chunks aligned to 4 KiB so that threads do not share pages.  */
  chunk = (nbytes / nthreads + 4095) & ~(size_t) 4095;
  started = 0;
  off = 0;
  for (i = 0; i < nthreads - 1 && off + chunk < nbytes; i++)
    {
      job[i].dst = (char *) dst + off;
      job[i].src = (const char *) src + off;
      job[i].n = chunk;
      if (__gthread_create (&tid[i], copy_job_run, &job[i]) != 0)
	break;
      started++;
      off += chunk;
    }
  /* The calling thread copies the remainder, then joins the helpers.  */
  memcpy ((char *) dst + off, (const char *) src + off, nbytes - off);
  for (i = 0; i < started; i++)
    __gthread_join (tid[i], NULL);
}

/* Assign an ID to a statement on an asynchronous unit that was
   completed synchronously.  */

int
async_complete_id (async_unit *au)
{
  int ret;
  LOCK (&au->lock);
  ret = au->id.high++;
  au->id.low++;
  UNLOCK (&au->lock);
  return ret;
}

/* Perform a wait operation an an asynchronous unit without an ID.  */

bool
async_wait (st_parameter_common *cmp, async_unit *au)
{
  bool ret;

  if (au == NULL)
    return false;

  if (cmp == NULL)
    cmp = au->error.cmp;

  LOCK (&(au->lock));
  SIGNAL (&(au->work));

  if (au->empty)
    {
      ret = collect_async_errors (cmp, au);
      UNLOCK (&au->lock);
      return ret;
    }

  WAIT_SIGNAL_MUTEX (&(au->emptysignal), (au->empty), &au->lock);
  ret = collect_async_errors (cmp, au);
  return ret;
}

/* Close an asynchronous unit.  */

void
async_close (async_unit *au)
{
  if (au == NULL)
    return;

  NOTE ("Closing async unit");
  enqueue_close (au);
  T_ERROR (__gthread_join, au->thread, NULL);
  free_async_unit (au);
}

#else

/* Only set u->au to NULL so no async I/O will happen.  */

void
init_async_unit (gfc_unit *u)
{
  u->au = NULL;
  return;
}

/* Do-nothing function, which will not be called.  */

void
enqueue_transfer (async_unit *au, transfer_args *arg, enum aio_do type,
		  void *owned, size_t owned_bytes, size_t owned_cap)
{
  return;
}

/* Do-nothing function, which will not be called.  */
void *
async_copy_buffer (async_unit *au, size_t nbytes, size_t *cap)
{
  *cap = 0;
  return NULL;
}

/* Plain copy when there is no threading.  */
void
async_copy_mem (void *dst, const void *src, size_t nbytes)
{
  memcpy (dst, src, nbytes);
}

/* Do-nothing function, which will not be called.  */

int
enqueue_done_id (async_unit *au, enum aio_do type)
{
  return 0;
}

/* Do-nothing function, which will not be called.  */

void
enqueue_done (async_unit *au, enum aio_do type)
{
  return;
}

/* Do-nothing function, which will not be called.  */

void
enqueue_close (async_unit *au)
{
  return;
}

/* Do-nothing function, which will not be called.  */

void
enqueue_data_transfer_init (async_unit *au, st_parameter_dt *dt, int read_flag)
{
  return;
}

/* Do-nothing function, which will not be called.  */

bool
collect_async_errors (st_parameter_common *cmp, async_unit *au)
{
  return false;
}

/* Do-nothing function, which will not be called.  */

bool
async_wait_id (st_parameter_common *cmp, async_unit *au, int i)
{
  return false;
}

/* Do-nothing function, which will not be called.  */

int
async_complete_id (async_unit *au)
{
  return 0;
}

/* Do-nothing function, which will not be called.  */

bool
async_wait (st_parameter_common *cmp, async_unit *au)
{
  return false;
}

/* Do-nothing function, which will not be called.  */

void
async_close (async_unit *au)
{
  return;
}

#endif
