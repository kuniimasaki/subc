#include <stdlib.h>
#include <stdio.h>

// calloc-invalid-negative: calloc() with a negative element count must be
// rejected explicitly rather than being silently reinterpreted as a huge
// unsigned size_t (the classic negative-size-to-huge-unsigned bug class).

int main() {
  long n = -1;
  int *p = (int *)calloc(n, sizeof(int));
  printf("should not be reached\n");
  return 0;
}
