! { dg-do run }
! { dg-additional-options "-pthread" }
! { dg-set-target-env-var GFORTRAN_ASYNC_COPY "yes" }
! { dg-set-target-env-var GFORTRAN_ASYNC_COPY_LIMIT "1" }
! Copy mode with a 1 MiB limit on outstanding copies per unit: writes
! larger than the limit must drain the queue and still be copied, so the
! data in the file is what was there at the statement, and mixing in
! synchronous statements and a second unit works.
program async_copy_2
  implicit none
  integer, parameter :: n = 2**19   ! 4 MiB of integers, 4x the limit
  integer, allocatable, asynchronous :: a(:)
  integer, allocatable :: r(:)
  integer :: k, i, id(5)
  allocate (a(n), r(n))
  open (10, file='async_copy_2.bin', form='unformatted', access='stream', &
        asynchronous='yes', status='replace')
  open (11, file='async_copy_2b.bin', form='unformatted', &
        asynchronous='yes', status='replace')
  do k = 1, 5
     a = k
     write (10, asynchronous='yes', id=id(k)) a
     write (11, asynchronous='yes') a(1:1000)
     a = -k            ! clobber before WAIT
  end do
  write (10) 12345     ! synchronous statement on the async unit
  wait (10)
  wait (11)
  rewind (10)
  do k = 1, 5
     read (10) r
     if (any (r /= k)) stop k
  end do
  read (10) i
  if (i /= 12345) stop 6
  close (10, status='delete')
  rewind (11)
  do k = 1, 5
     read (11) r(1:1000)
     if (any (r(1:1000) /= k)) stop 10 + k
  end do
  close (11, status='delete')
end program async_copy_2
