#include <stdio.h>

// out-of-bounds-access

int main() {
  int array[5] = { 0, 1, 2, 3, 4 }, i = 4;
  printf("%d\n", array[i++]);
  printf("%d\n", array[i++]); // out of bounds
  return 0;
}
