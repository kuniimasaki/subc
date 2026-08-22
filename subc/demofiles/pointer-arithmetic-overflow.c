#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// pointer-arithmetic-overflow: existing pointer OOB demos only probe by a
// handful of elements past the end (see pointer-out-of-bounds.c /
// pointer-out-of-bounds-2.c). This checks a much larger displacement --
// pointer arithmetic that lands far outside the block -- is still caught
// by the same bounds check, not just small overruns.

int main() {
  long n = 4;
  int *p = (int *)malloc(n * sizeof(int));
  assert(p != 0);
  int *q = p + 1000000; // far past the end of the block
  *q = 1;
  return 0;
}
