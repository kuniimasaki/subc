#include <stdio.h>

// segmentation-fault

int main() {
  int *ptr = (void *)0; // NULL
  *ptr = 42;
  return 0;
}
