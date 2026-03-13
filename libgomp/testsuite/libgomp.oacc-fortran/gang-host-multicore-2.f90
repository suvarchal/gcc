! Test host multicore gang reductions: sum, max, min with gang-partitioned
! loops across multiple gangs.
! { dg-do run }

program gang_host_multicore_2
  implicit none
  integer, parameter :: n = 10000
  real(8) :: a(n)
  real(8) :: sum_result, max_result, min_result
  real(8) :: sum_expected, max_expected, min_expected
  integer :: i, isum

  do i = 1, n
    a(i) = real(i, 8) * 0.001d0
  end do

  ! Test reduction(+:sum) with gang-partitioned loop.
  sum_expected = 0.0d0
  do i = 1, n
    sum_expected = sum_expected + a(i)
  end do

  sum_result = 0.0d0
  !$acc parallel loop gang num_gangs (4) reduction(+:sum_result)
  do i = 1, n
    sum_result = sum_result + a(i)
  end do

  if (abs(sum_result - sum_expected) > 1.0d-6) STOP 1

  ! Test reduction(max:) with gang-partitioned loop.
  max_expected = a(1)
  do i = 2, n
    max_expected = max(max_expected, a(i))
  end do

  max_result = a(1)
  !$acc parallel loop gang num_gangs (4) reduction(max:max_result)
  do i = 1, n
    max_result = max(max_result, a(i))
  end do

  if (abs(max_result - max_expected) > 1.0d-10) STOP 2

  ! Test reduction(min:) with gang-partitioned loop.
  min_expected = a(1)
  do i = 2, n
    min_expected = min(min_expected, a(i))
  end do

  min_result = a(1)
  !$acc parallel loop gang num_gangs (4) reduction(min:min_result)
  do i = 1, n
    min_result = min(min_result, a(i))
  end do

  if (abs(min_result - min_expected) > 1.0d-10) STOP 3

  ! Test integer reduction with different gang count.
  isum = 0
  !$acc parallel loop gang num_gangs (8) reduction(+:isum)
  do i = 0, 99
    isum = isum + i
  end do

  if (isum /= 4950) STOP 4
end program
