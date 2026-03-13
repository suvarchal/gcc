/* Test host multicore gang reductions: sum, max, min with gang-partitioned
   loops across multiple gangs.
   { dg-do run } */

#include <stdlib.h>
#include <math.h>

#define N 10000

int
main ()
{
  double a[N];
  double sum_result, max_result, min_result;
  double sum_expected, max_expected, min_expected;
  int i;

  for (i = 0; i < N; i++)
    a[i] = (i + 1) * 0.001;

  /* Test reduction(+:sum) with gang-partitioned loop.  */
  sum_expected = 0.0;
  for (i = 0; i < N; i++)
    sum_expected += a[i];

  sum_result = 0.0;
#pragma acc parallel loop gang num_gangs (4) reduction (+:sum_result)
  for (i = 0; i < N; i++)
    sum_result += a[i];

  if (fabs (sum_result - sum_expected) > 1.0e-6)
    abort ();

  /* Test reduction(max:) with gang-partitioned loop.  */
  max_expected = a[0];
  for (i = 1; i < N; i++)
    if (a[i] > max_expected)
      max_expected = a[i];

  max_result = a[0];
#pragma acc parallel loop gang num_gangs (4) reduction (max:max_result)
  for (i = 0; i < N; i++)
    if (a[i] > max_result)
      max_result = a[i];

  if (fabs (max_result - max_expected) > 1.0e-10)
    abort ();

  /* Test reduction(min:) with gang-partitioned loop.  */
  min_expected = a[0];
  for (i = 1; i < N; i++)
    if (a[i] < min_expected)
      min_expected = a[i];

  min_result = a[0];
#pragma acc parallel loop gang num_gangs (4) reduction (min:min_result)
  for (i = 0; i < N; i++)
    if (a[i] < min_result)
      min_result = a[i];

  if (fabs (min_result - min_expected) > 1.0e-10)
    abort ();

  /* Test integer reduction with different gang count.  */
  int isum = 0;
#pragma acc parallel loop gang num_gangs (8) reduction (+:isum)
  for (i = 0; i < 100; i++)
    isum += i;

  if (isum != 4950)
    abort ();

  return 0;
}
