#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// memory-leak

int main() {
  for (int i = 0;  i < 10;  ++i) {
    int *ptr = (int *)malloc(sizeof(*ptr));
    assert(ptr != 0);
    printf("%p\n", ptr);
    *ptr = i;
  }
  return 0;
}
