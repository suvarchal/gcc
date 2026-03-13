/* Test host multicore gang parallelism: loop partitioning without
   reductions.  Each gang writes to a disjoint portion of the array.
   { dg-do run } */

#include <stdlib.h>

#define N 1000

int
main ()
{
  int a[N];
  int i;

  for (i = 0; i < N; i++)
    a[i] = 0;

  /* Gang-partitioned loop: each gang handles a subset of iterations.  */
#pragma acc parallel loop gang num_gangs (4)
  for (i = 0; i < N; i++)
    a[i] = i + 1;

  for (i = 0; i < N; i++)
    if (a[i] != i + 1)
      abort ();

  /* Test with different gang counts.  */
  for (i = 0; i < N; i++)
    a[i] = 0;

#pragma acc parallel loop gang num_gangs (7)
  for (i = 0; i < N; i++)
    a[i] = i * 2;

  for (i = 0; i < N; i++)
    if (a[i] != i * 2)
      abort ();

  return 0;
}
