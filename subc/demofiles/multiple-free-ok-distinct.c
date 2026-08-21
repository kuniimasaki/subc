#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// multiple-free NEGATIVE control: free two DISTINCT allocations, each
// exactly once. This must NOT be flagged as a double-free.

int main() {
  int *a = (int *)malloc(sizeof(*a));
  int *b = (int *)malloc(sizeof(*b));
  assert(a != 0);
  assert(b != 0);
  *a = 1;
  *b = 2;
  printf("%d %d\n", *a, *b);
  free(a);
  free(b);
  return 0;
}
