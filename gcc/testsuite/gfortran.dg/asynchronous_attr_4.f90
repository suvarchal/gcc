! { dg-do run }
! { dg-options "-O2 -fno-async-volatile" }
! { dg-additional-options "-pthread" }
! With -fno-async-volatile the items of an asynchronous READ are ordinary
! variables.  The library stores into them after the transfer call has
! returned, so the transfer calls of an asynchronous statement must declare
! their data argument as escaping; otherwise the value cached before the
! WAIT would be used afterwards.
program asynchronous_attr_4
  implicit none
  integer, asynchronous :: s
  real(8), allocatable, asynchronous :: r(:)
  character(len=8), asynchronous :: c(3)
  real(8), allocatable :: ref(:)
  integer :: i, id
  allocate (r(100000), ref(100000))
  ref = [(i * 0.25d0, i = 1, 100000)]
  open (10, file='asynchronous_attr_4.bin', form='unformatted', access='stream', &
        status='replace')
  write (10) 7, ref, 'alpha   ', 'beta    ', 'gamma   '
  close (10)
  s = -1
  r = -1
  c = 'xxxxxxxx'
  open (10, file='asynchronous_attr_4.bin', form='unformatted', access='stream', &
        status='old', asynchronous='yes')
  read (10, asynchronous='yes', id=id) s, r, c
  wait (10, id=id)
  if (s /= 7) stop 1
  if (any (r /= ref)) stop 2
  if (any (c /= ['alpha   ', 'beta    ', 'gamma   '])) stop 3
  close (10, status='delete')
end program asynchronous_attr_4
