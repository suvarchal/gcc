! { dg-do run }
! { dg-set-target-env-var GFORTRAN_COMPRESS "lz4" }
! With GFORTRAN_COMPRESS set, an existing uncompressed unformatted
! sequential file must still be read correctly: the runtime decides by
! the header record, not by the environment.  Build such a file byte by
! byte through a stream unit (4-byte record markers), then read it as
! unformatted sequential.  Also check that a file written compressed can
! be appended to and read back entirely.
program compress_2
  implicit none
  integer :: v(3), r(3), i, ios
  real(8) :: x, y
  integer(4) :: marker
  v = [11, 22, 33]
  open (10, file='compress_2.dat', access='stream', form='unformatted', status='replace')
  marker = 12
  write (10) marker, v, marker
  marker = 8
  write (10) marker, 2.5d0, marker
  close (10)

  open (10, file='compress_2.dat', form='unformatted', status='old')
  read (10) r
  if (any (r /= v)) stop 1
  read (10) x
  if (x /= 2.5d0) stop 2
  read (10, iostat=ios) x
  if (ios >= 0) stop 3
  close (10, status='delete')

  open (10, file='compress_2b.dat', form='unformatted', status='replace')
  write (10) v
  close (10)
  open (10, file='compress_2b.dat', form='unformatted', status='old', position='append')
  write (10) 7.5d0
  close (10)
  open (10, file='compress_2b.dat', form='unformatted', status='old')
  read (10) r
  read (10) y
  if (any (r /= v) .or. y /= 7.5d0) stop 4
  close (10, status='delete')
end program compress_2
