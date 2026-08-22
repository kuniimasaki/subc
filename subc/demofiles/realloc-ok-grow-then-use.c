#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-ok-grow-then-use NEGATIVE control: a growing realloc() that
// moves the allocation, followed by legitimate reads and writes through
// the NEW pointer realloc() actually returned. This must NOT be flagged --
// only continued use of the stale OLD pointer is a bug (see
// realloc-move-then-use-old.c).

int main() {
  long n = 2;
  int *p = (int *)malloc(n * sizeof(int));
  assert(p != 0);
  p[0] = 1; p[1] = 2;

  long big = 2000000; // forces a moving realloc
  int *q = (int *)realloc(p, big * sizeof(int));
  assert(q != 0);
  printf("%d %d\n", q[0], q[1]);

  q[0] = 100; q[1] = 200;
  printf("%d %d\n", q[0], q[1]);
  free(q);
  return 0;
}
