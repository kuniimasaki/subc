#include <stdlib.h>
#include <stdio.h>

// malloc-negative-size: malloc() with a negative size must be rejected
// explicitly, not silently reinterpreted as an enormous unsigned size_t
// (the classic negative-size-to-huge-unsigned bug class -- the same one
// calloc-invalid-negative.c checks for calloc()). prim_malloc used to
// convert the signed argument to size_t *before* checking its sign, which
// made the sign check a tautology (an unsigned value is never < 0); it was
// only saved by a separate, unrelated size cap happening to also reject
// the huge wrapped-around value. Fixed to check the sign first.

int main() {
  long n = -1;
  int *p = (int *)malloc(n);
  printf("should not be reached\n");
  return 0;
}
