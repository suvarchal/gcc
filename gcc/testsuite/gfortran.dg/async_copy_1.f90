! { dg-do run }
! { dg-additional-options "-pthread" }
! { dg-set-target-env-var GFORTRAN_ASYNC_COPY "yes" }
! With GFORTRAN_ASYNC_COPY the data of an asynchronous WRITE is copied
! when the statement executes, so overwriting the variable afterwards
! (before WAIT) must not change what reaches the file.  Covers scalars,
! contiguous and strided array sections, characters of kind 1 and 4,
! and a derived type with an array component.
program async_copy_1
  implicit none
  type :: t
     integer :: tag
     real(8) :: v(3)
  end type t
  integer, parameter :: n = 1000000
  real(8), allocatable, asynchronous :: a(:), r(:)
  integer, allocatable, asynchronous :: m(:,:)
  integer, allocatable :: rm(:,:), m0(:,:)
  character(len=8), asynchronous :: c(4)
  character(len=8) :: rc(4)
  character(kind=4, len=5), asynchronous :: w
  character(kind=4, len=5) :: rw
  type(t), asynchronous :: d
  type(t) :: rd
  integer, asynchronous :: s
  integer :: rs, i, id

  allocate (a(n), r(n), m(10, 10), rm(5, 10), m0(10, 10))
  a = [(real(i, 8), i = 1, n)]
  m0 = reshape ([(i, i = 1, 100)], [10, 10])
  m = m0
  c = ['alpha   ', 'beta    ', 'gamma   ', 'delta   ']
  w = 4_'hello'
  d = t (42, [1.5d0, 2.5d0, 3.5d0])
  s = 7

  open (10, file='async_copy_1.bin', form='unformatted', access='stream', &
        asynchronous='yes', status='replace')
  write (10, asynchronous='yes', id=id) s, a, m(1:10:2, :), c, w, d

  ! Clobber everything right away.  Non-conforming for a zero-copy
  ! implementation, which is exactly what copy mode is meant to tolerate.
  s = -1
  a = -1.0d0
  m = -1
  c = 'xxxxxxxx'
  w = 4_'zzzzz'
  d = t (-1, [-1.0d0, -1.0d0, -1.0d0])

  wait (10, id=id)
  rewind (10)
  read (10) rs, r, rm, rc, rw, rd
  close (10, status='delete')

  if (rs /= 7) stop 1
  if (any (r /= [(real(i, 8), i = 1, n)])) stop 2
  if (any (rm /= m0(1:10:2, :))) stop 3
  if (any (rc /= ['alpha   ', 'beta    ', 'gamma   ', 'delta   '])) stop 4
  if (rw /= 4_'hello') stop 5
  if (rd%tag /= 42 .or. any (rd%v /= [1.5d0, 2.5d0, 3.5d0])) stop 6
end program async_copy_1
