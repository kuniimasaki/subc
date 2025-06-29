#include <stdio.h>

// pointer-out-of-bounds-2

int main() {
  int padl, array[5] = { 0, 1, 2, 3, 4 }, padr;
  int *ptr = &array[5]; // OK
  ptr[-2] = 42;         // OK
  ptr[-6] = 666;        // illegal
  return 0;
}
