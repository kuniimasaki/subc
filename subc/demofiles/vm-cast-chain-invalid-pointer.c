#include <stdio.h>
#include <stdint.h>

// same bug as demofiles/invalid-pointer.c, run under -O: a typedef'd
// integer cast to a pointer type must actually become a Pointer object
// (not stay a bare Integer), so the existing "arbitrary memory location"
// detection gets a chance to fire on the write through it.

int main() {
  int *ptr;

  ptr = (int *)(intptr_t)0xDeadD0d0;
  printf("%p\n", ptr);
  *ptr = 42;			// illegal access
  return 0;
}
