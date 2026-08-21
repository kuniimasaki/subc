#include <stdio.h>

// out-of-bounds-access NEGATIVE control: access strictly within the
// array's bounds. This must NOT be flagged.

int main() {
  int array[5] = { 0, 1, 2, 3, 4 };
  for (int i = 0;  i < 5;  ++i)
    printf("%d\n", array[i]);
  return 0;
}
