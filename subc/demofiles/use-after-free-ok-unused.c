#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// use-after-free NEGATIVE control: a variable holds a pointer to freed
// memory, but the program never touches it again through that pointer.
// This must NOT be flagged -- merely holding a stale pointer is not itself
// a use-after-free.

int main() {
  int *ptr = (int *)malloc(sizeof(*ptr));
  assert(ptr != 0);
  *ptr = 42;
  printf("%d\n", *ptr);
  free(ptr);
  printf("done\n"); // ptr is never read or written again
  return 0;
}
