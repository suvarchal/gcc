! { dg-do run }
! { dg-additional-options "-pthread" }
! { dg-set-target-env-var GFORTRAN_ASYNC_COPY "yes" }
! { dg-set-target-env-var GFORTRAN_ASYNC_COPY_THREADS "3" }
! Copy mode with a multi-threaded copy: items above the 8 MiB threshold are
! split in 4 KiB aligned chunks across threads.  Use sizes that are not
! multiples of the chunk size, a contiguous 3-D section, and a strided
! section (which takes the element-wise path), and clobber before WAIT.
program async_copy_3
  implicit none
  integer, parameter :: n1 = 3*1024*1024 + 123    ! 12 MiB + odd tail, integer(4)
  integer, allocatable, asynchronous :: a(:)
  real(8), allocatable, asynchronous :: f(:,:,:)
  integer, allocatable :: ra(:)
  real(8), allocatable :: rf(:,:,:), rs(:,:)
  integer :: i, j, k, id
  allocate (a(n1), ra(n1), f(47, 30001, 2), rf(47, 30001, 2), rs(24, 30001))
  a = [(i, i = 1, n1)]
  do k = 1, 2
     do j = 1, 30001
        do i = 1, 47
           f(i, j, k) = i + 100 * j + 1d7 * k
        end do
     end do
  end do
  open (10, file='async_copy_3.bin', form='unformatted', access='stream', &
        asynchronous='yes', status='replace')
  write (10, asynchronous='yes', id=id) a, f(:, :, 2), f(1:47:2, :, 1)
  a = 0
  f = 0
  wait (10, id=id)
  rewind (10)
  read (10) ra, rf(:, :, 2), rs
  close (10, status='delete')
  if (any (ra /= [(i, i = 1, n1)])) stop 1
  do j = 1, 30001
     do i = 1, 47
        if (rf(i, j, 2) /= i + 100 * j + 2d7) stop 2
     end do
     do i = 1, 24
        if (rs(i, j) /= (2 * i - 1) + 100 * j + 1d7) stop 3
     end do
  end do
end program async_copy_3
