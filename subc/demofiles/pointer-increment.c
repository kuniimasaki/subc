#include <stdio.h>

// pointer-increment

int main() {
  int array[5] = {0, 1, 2, 3, 4};
  int *ptr = array + 4;
  ++ptr;  printf("%p\n", ptr);
  ++ptr;  printf("%p\n", ptr); // out of bounds
  return 0;
}
