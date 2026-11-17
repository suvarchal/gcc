! { dg-do run }
! PR libfortran/128796
! Unformatted transfer of contiguous and non-contiguous arrays and
! sections, WRITE and READ.
program array_io_blocks_1
  implicit none
  type t
    integer :: i, j
  end type t
  integer, target :: a(4,5,6), b(1,30), c(30,1)
  integer, pointer :: p(:,:,:)
  type(t) :: s(3,4)
  integer :: k

  a = reshape ([(k, k = 1, size (a))], shape (a))
  b = reshape ([(k, k = 1, size (b))], shape (b))
  c = reshape ([(k, k = 1, size (c))], shape (c))
  s = reshape ([(t(k, -k), k = 1, size (s))], shape (s))
  p => a(:,2:4,:)

  open (10, status='scratch', form='unformatted')
  write (10) a
  write (10) a(:,:,2:4)
  write (10) a(:,2:4,:)
  write (10) a(2:3,:,:)
  write (10) a(:,::2,:)
  write (10) a(1:1,:,:)
  write (10) a(:,:,6:6)
  write (10) p
  write (10) b
  write (10) c
  write (10) s%j
  rewind (10)
  call check (pack (a, .true.))
  call check (pack (a(:,:,2:4), .true.))
  call check (pack (a(:,2:4,:), .true.))
  call check (pack (a(2:3,:,:), .true.))
  call check (pack (a(:,::2,:), .true.))
  call check (pack (a(1:1,:,:), .true.))
  call check (pack (a(:,:,6:6), .true.))
  call check (pack (p, .true.))
  call check (pack (b, .true.))
  call check (pack (c, .true.))
  call check (pack (s%j, .true.))

  rewind (10)
  call reread
  close (10)
contains
  subroutine check (ref)
    integer, intent(in) :: ref(:)
    integer :: got(size (ref))
    read (10) got
    if (any (got /= ref)) stop 1
  end subroutine check

  subroutine reread
    integer, target :: x(4,5,6), y(1,30)
    integer, pointer :: q(:,:,:)
    type(t) :: u(3,4)
    x = 0
    read (10) x
    if (any (x /= a)) stop 2
    x = 0
    read (10) x(:,:,2:4)
    if (any (x(:,:,2:4) /= a(:,:,2:4)) .or. any (x(:,:,1) /= 0)) stop 3
    x = 0
    read (10) x(:,2:4,:)
    if (any (x(:,2:4,:) /= a(:,2:4,:)) .or. any (x(:,1,:) /= 0)) stop 4
    x = 0
    read (10) x(2:3,:,:)
    if (any (x(2:3,:,:) /= a(2:3,:,:)) .or. any (x(1,:,:) /= 0)) stop 5
    x = 0
    read (10) x(:,::2,:)
    if (any (x(:,::2,:) /= a(:,::2,:)) .or. any (x(:,2,:) /= 0)) stop 6
    read (10)
    read (10)
    x = 0
    q => x(:,2:4,:)
    read (10) q
    if (any (x(:,2:4,:) /= a(:,2:4,:)) .or. any (x(:,5,:) /= 0)) stop 7
    read (10) y
    if (any (y /= b)) stop 8
    read (10)
    u = t(0, 0)
    read (10) u%j
    if (any (u%j /= s%j) .or. any (u%i /= 0)) stop 9
  end subroutine reread
end program array_io_blocks_1
