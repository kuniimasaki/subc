#include <stdio.h>

// null-pointer

int main() {
  char *ptr = (void *)0; // NULL
  printf("%s\n", ptr);
  return 0;
}
