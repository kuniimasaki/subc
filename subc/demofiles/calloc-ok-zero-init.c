#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// calloc-ok-zero-init NEGATIVE control: calloc() must zero-initialize its
// memory (unlike malloc(), which leaves it uninitialized -- compare
// uninitialised.c). Reading freshly calloc'd memory before writing to it
// must NOT be flagged and must read back as all zero.

int main() {
  long n = 5;
  int *p = (int *)calloc(n, sizeof(int));
  assert(p != 0);
  printf("%d %d %d %d %d\n", p[0], p[1], p[2], p[3], p[4]);
  free(p);
  return 0;
}
