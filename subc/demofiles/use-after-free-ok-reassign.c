#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// use-after-free NEGATIVE control: free a pointer, then immediately
// reassign the variable to a fresh allocation before using it again.
// This must NOT be flagged -- the variable no longer refers to the freed
// block by the time it is dereferenced.

int main() {
  int *ptr = (int *)malloc(sizeof(*ptr));
  assert(ptr != 0);
  *ptr = 1;
  free(ptr);
  ptr = (int *)malloc(sizeof(*ptr)); // fresh allocation, not the freed one
  assert(ptr != 0);
  *ptr = 2;
  printf("%d\n", *ptr);
  free(ptr);
  return 0;
}
