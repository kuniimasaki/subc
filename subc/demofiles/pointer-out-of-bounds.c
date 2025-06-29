#include <stdio.h>

// pointer-out-of-bounds

int main() {
  int array[5] = {0, 1, 2, 3, 4};
  int *ptr = &array[5];
  *ptr = 42;
  return 0;
}
