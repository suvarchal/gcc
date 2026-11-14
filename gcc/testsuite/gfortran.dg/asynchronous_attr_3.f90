! { dg-do compile }
! { dg-options "-O2 -fno-async-volatile -fdump-tree-original" }
! -fno-async-volatile must not affect variables declared VOLATILE.
subroutine s (n)
  integer, intent(in) :: n
  real, volatile :: v
  real, asynchronous :: a(n)
  a = v
end subroutine s
! { dg-final { scan-tree-dump "volatile" "original" } }
