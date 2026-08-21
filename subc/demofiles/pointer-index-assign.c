#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// p[i] = x on a malloc'd pointer: assign()'s Index case previously only
// handled fixed-size Array literals, not Pointer, so writes through a
// malloc'd pointer via subscript silently failed instead of storing.

int main() {
  long n = 3;
  int *p = (int *)malloc(n * sizeof(int));
  assert(p != 0);
  p[0] = 10;
  p[1] = 20;
  p[2] = 30;
  assert(p[0] == 10);
  assert(p[1] == 20);
  assert(p[2] == 30);
  printf("%d %d %d\n", p[0], p[1], p[2]);
  free(p);
  return 0;
}
