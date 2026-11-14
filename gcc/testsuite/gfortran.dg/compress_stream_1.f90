! { dg-do run }
! { dg-set-target-env-var GFORTRAN_COMPRESS "yes" }
! { dg-set-target-env-var GFORTRAN_COMPRESS_CHUNK "4" }
! { dg-set-target-env-var GFORTRAN_COMPRESS_ACCESS "stream,direct" }
! Compressed stream and direct access (GFORTRAN_COMPRESS with c-blosc2):
! the file is a c-blosc2 frame of 4 KiB chunks, so records constantly
! cross chunk boundaries.  POS=, REC=, out-of-order writes, rewrites,
! append, INQUIRE(SIZE=) and reopening must behave as for an ordinary
! file.  Passes unchanged when c-blosc2 is not available.
program compress_stream_1
  implicit none
  integer, parameter :: n = 5000
  real(8) :: a(n), b(n), r(n), x
  integer :: i, k, rec(20), ir(1000), u
  integer(8) :: sz
  a = [(sin (0.001d0 * i), i = 1, n)]
  b = [(cos (0.002d0 * i), i = 1, n)]

  ! ---- stream access
  open (newunit=u, file='compress_stream_1.str', access='stream', &
        form='unformatted', status='replace')
  write (u, pos=8*n+1) b             ! second half first: out of order
  write (u, pos=1) a
  write (u, pos=8*100+1) -1.0d0      ! rewrite one value in the middle
  inquire (u, size=sz)
  if (sz /= 16*n) stop 1
  close (u)
  open (newunit=u, file='compress_stream_1.str', access='stream', &
        form='unformatted', status='old')
  read (u, pos=1) r
  if (r(101) /= -1.0d0) stop 2
  r(101) = a(101)
  if (any (r /= a)) stop 3
  read (u) r                          ! sequential continuation after POS=
  if (any (r /= b)) stop 4
  read (u, pos=8*(n+4999)+1) x
  if (x /= b(5000)) stop 5
  close (u)
  open (newunit=u, file='compress_stream_1.str', access='stream', &
        form='unformatted', status='old', position='append')
  write (u) 42.0d0
  close (u)
  open (newunit=u, file='compress_stream_1.str', access='stream', &
        form='unformatted', status='old')
  inquire (u, size=sz)
  if (sz /= 16*n + 8) stop 6
  read (u, pos=16*n+1) x
  if (x /= 42.0d0) stop 7
  close (u, status='delete')

  ! ---- direct access, records of 8000 bytes written in shuffled order
  open (newunit=u, file='compress_stream_1.dir', access='direct', &
        form='unformatted', recl=8000, status='replace')
  rec = [(mod (7 * k, 20) + 1, k = 1, 20)]
  do k = 1, 20
     write (u, rec=rec(k)) (real (rec(k), 8) + 0.5d0 * i, i = 1, 1000)
  end do
  write (u, rec=13) (-real (i, 8), i = 1, 1000)    ! overwrite one record
  close (u)
  open (newunit=u, file='compress_stream_1.dir', access='direct', &
        form='unformatted', recl=8000, status='old')
  do k = 20, 1, -1
     read (u, rec=k) r(1:1000)
     if (k == 13) then
        if (any (r(1:1000) /= [(-real (i, 8), i = 1, 1000)])) stop 8
     else
        if (any (r(1:1000) /= [(real (k, 8) + 0.5d0 * i, i = 1, 1000)])) stop 9
     end if
  end do
  read (u, rec=3) ir
  close (u, status='delete')
end program compress_stream_1
