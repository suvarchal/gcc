! { dg-do run }
! { dg-additional-options "-pthread" }
! { dg-set-target-env-var GFORTRAN_ASYNC_COPY "yes" }
! Buffered asynchronous READ: the I/O thread reads into a runtime buffer
! and the destination is filled at the synchronising WAIT, INQUIRE
! (PENDING=.false.) or synchronous statement.  Until then the destination
! keeps its old value, so looking early never shows a torn mix.
program async_copy_read_1
  implicit none
  integer, parameter :: n = 2000000
  real(8), allocatable :: a(:), ref(:)
  real(8), allocatable, asynchronous :: r(:)
  integer, allocatable, asynchronous :: m(:,:)
  integer, allocatable :: m0(:,:)
  character(len=8), asynchronous :: c(3)
  integer, asynchronous :: s
  integer :: i, id1, id2, k
  logical :: pend
  real(8) :: chk

  allocate (a(n), ref(n), r(n), m(10, 10), m0(10, 10))
  ref = [(real (i, 8) * 0.5d0, i = 1, n)]
  m0 = reshape ([(i, i = 1, 100)], [10, 10])
  open (10, file='async_copy_read_1.bin', form='unformatted', access='stream', &
        status='replace')
  write (10) 42, ref, m0(1:10:2, :), 'alpha   ', 'beta    ', 'gamma   '
  write (10) ref
  close (10)

  r = -1.0d0
  m = -1
  c = 'xxxxxxxx'
  s = -1
  open (10, file='async_copy_read_1.bin', form='unformatted', access='stream', &
        status='old', asynchronous='yes')
  read (10, asynchronous='yes', id=id1) s, r, m(1:10:2, :), c
  ! Keep the main thread busy for a while; the destinations must be
  ! untouched whatever the I/O thread has done so far.
  chk = 0
  do k = 1, 20
     chk = chk + sum (a)      ! a is unrelated, just work
     a = a + 1.0d0
  end do
  if (s /= -1 .or. any (r /= -1.0d0) .or. any (m /= -1) .or. any (c /= 'xxxxxxxx')) stop 1
  wait (10, id=id1)
  if (s /= 42) stop 2
  if (any (r /= ref)) stop 3
  if (any (m(1:10:2, :) /= m0(1:10:2, :)) .or. any (m(2:10:2, :) /= -1)) stop 4
  if (any (c /= ['alpha   ', 'beta    ', 'gamma   '])) stop 5

  ! Completion through INQUIRE(PENDING=) returning false.
  r = -2.0d0
  read (10, asynchronous='yes', id=id2) r
  do
     inquire (10, id=id2, pending=pend)
     if (.not. pend) exit
  end do
  if (any (r /= ref)) stop 6
  close (10, status='delete')
  if (chk < 0) print *, chk
end program async_copy_read_1
