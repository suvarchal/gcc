! Test host multicore gang parallelism: loop partitioning without
! reductions.  Each gang writes to a disjoint portion of the array.
! { dg-do run }

program gang_host_multicore_1
  implicit none
  integer, parameter :: n = 1000
  integer :: a(n), i

  a = 0

  ! Gang-partitioned loop: each gang handles a subset of iterations.
  !$acc parallel loop gang num_gangs (4)
  do i = 1, n
    a(i) = i
  end do

  do i = 1, n
    if (a(i) /= i) STOP 1
  end do

  ! Test with different gang count.
  a = 0

  !$acc parallel loop gang num_gangs (7)
  do i = 1, n
    a(i) = i * 2
  end do

  do i = 1, n
    if (a(i) /= i * 2) STOP 2
  end do
end program
