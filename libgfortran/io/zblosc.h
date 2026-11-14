/* c-blosc2 entry points used by libgfortran, loaded at run time.
   Copyright (C) 2026 Free Software Foundation, Inc.

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

#ifndef GFOR_ZBLOSC_H
#define GFOR_ZBLOSC_H

#ifdef HAVE_BLOSC2
/* Only types and constants come from these headers; the library is
   opened with dlopen by gfc_blosc2_available (transfer.c).  */
#include <blosc2.h>
#include <b2nd.h>

typedef struct
{
  int state;			/* 0 not tried, 1 loaded, -1 failed.  */
  char err[256];
  void (*init) (void);
  blosc2_context *(*create_cctx) (blosc2_cparams);
  blosc2_context *(*create_dctx) (blosc2_dparams);
  void (*free_ctx) (blosc2_context *);
  int (*compress_ctx) (blosc2_context *, const void *, int32_t, void *,
		       int32_t);
  int (*decompress_ctx) (blosc2_context *, const void *, int32_t, void *,
			 int32_t);
  const char *(*version) (void);
  /* Super-chunks, for compressed stream and direct access.  */
  blosc2_schunk *(*schunk_new) (blosc2_storage *);
  blosc2_schunk *(*schunk_open) (const char *);
  int (*schunk_free) (blosc2_schunk *);
  int64_t (*schunk_append_buffer) (blosc2_schunk *, const void *, int32_t);
  int64_t (*schunk_update_chunk) (blosc2_schunk *, int64_t, uint8_t *, bool);
  int64_t (*schunk_delete_chunk) (blosc2_schunk *, int64_t);
  int (*schunk_decompress_chunk) (blosc2_schunk *, int64_t, void *, int32_t);
  int (*vlmeta_add) (blosc2_schunk *, const char *, uint8_t *, int32_t,
		     blosc2_cparams *);
  int (*vlmeta_update) (blosc2_schunk *, const char *, uint8_t *, int32_t,
			blosc2_cparams *);
  int (*vlmeta_get) (blosc2_schunk *, const char *, uint8_t **, int32_t *);
  int (*vlmeta_exists) (blosc2_schunk *, const char *);
  /* N-dimensional containers, for shape-aware array items.  */
  b2nd_context_t *(*b2nd_create_ctx) (const blosc2_storage *, int8_t,
				      const int64_t *, const int32_t *,
				      const int32_t *, const char *, int8_t,
				      const blosc2_metalayer *, int32_t);
  int (*b2nd_free_ctx) (b2nd_context_t *);
  int (*b2nd_from_cbuffer) (b2nd_context_t *, b2nd_array_t **, const void *,
			    int64_t);
  int (*b2nd_to_cframe) (const b2nd_array_t *, uint8_t **, int64_t *, bool *);
  int (*b2nd_from_cframe) (uint8_t *, int64_t, bool, b2nd_array_t **);
  int (*b2nd_to_cbuffer) (const b2nd_array_t *, void *, int64_t);
  int (*b2nd_free) (b2nd_array_t *);
} gfc_blosc2_api;

extern gfc_blosc2_api gfc_b2;
internal_proto (gfc_b2);

/* cparams for the current GFORTRAN_COMPRESS setting and TYPESIZE.  */
extern blosc2_cparams gfc_blosc2_cparams (int typesize);
internal_proto (gfc_blosc2_cparams);
#endif

#endif
