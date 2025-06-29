#include <stdio.h>
#include <stdint.h>

// invalid-pointer

int main() {
  int i, *ptr;

  ptr = &i;
  printf("%p\n",ptr);
  *ptr = 42;			// legal access
  printf("%d\n", *ptr);

  ptr = (int *)(intptr_t)0xDeadD0d0;
  printf("%p\n",ptr);
  *ptr = 42;			// illegal access
  printf("%d\n", *ptr);

  return 0;
}
