/* { dg-do compile }  */
/* { dg-options "-O2 -fdump-tree-cddce2 -ffinite-loops" } */

int
f1 (void)
{
  int i, j;

#pragma acc parallel loop tile(2,3)
  for (i = 1; i < 10; i++)
    for (j = 1; j < 10; j++)
      for (;;)
	;

  return i + j;
}

int
f2 (void)
{
  int i, j, k;

#pragma acc parallel loop tile(2,3)
  for (i = 1; i < 10; i++)
    for (j = 1; j < 10; j++)
      for (k = 1; k < 10; k++)
	;

  return i + j;
}
/* On offload targets all dims are constant and all 'if's can be eliminated.
   On host, gang dim is dynamic for multicore support, so loop bound
   comparisons against __goacc_host_dim_size remain.  */
/* { dg-final { scan-tree-dump-not "if" "cddce2" { target { offload_device } } } } */
