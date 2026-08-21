#include <stdio.h>

// null-pointer NEGATIVE control: check for NULL before dereferencing.
// This must NOT be flagged.

int main() {
  char *ptr = (void *)0; // NULL
  if (ptr)
    printf("%s\n", ptr);
  else
    printf("ptr is null\n");
  return 0;
}
