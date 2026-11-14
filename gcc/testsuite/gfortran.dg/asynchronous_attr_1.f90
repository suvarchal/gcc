! { dg-do compile }
! { dg-options "-O2 -fdump-tree-original" }
! By default a variable with the ASYNCHRONOUS attribute is treated as
! volatile.
subroutine s (a, n)
  integer, intent(in) :: n
  real, asynchronous :: a(n)
  real, allocatable, asynchronous :: b(:)
  integer :: i
  allocate (b(n))
  do i = 1, n
     b(i) = a(i) * 2.0
  end do
  a(1) = b(n)
end subroutine s
! { dg-final { scan-tree-dump "volatile" "original" } }
