#include <stdio.h>

// char-buffer-oob: an off-the-end write into a fixed-size char buffer via
// manual indexing (the classic string-overflow bug pattern -- subc has no
// strcpy/strcat yet to trigger this the usual way, but plain indexing hits
// the same array bounds check).

int main() {
  char buf[8];
  for (int i = 0; i < 8; i++) buf[i] = 'a' + i;
  printf("%c\n", buf[7]);

  buf[8] = 'X'; // one past the end
  return 0;
}
