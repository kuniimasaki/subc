#include <stdio.h>

// char-literal-assignment-ok NEGATIVE control / feature-enablement guard:
// character literals are parsed as plain int values (as real C actually
// specifies -- 'a' really is type int, not char), and this subset's
// converter table had no int->char/int->short entry at all, so even the
// most basic `char c = 'a';` fatal'd with "cannot convert 'int' to 'char'".
// Separately, plain assignment (`x = y;`, as opposed to a declaration
// initializer) never consulted the converter table at all, requiring the
// exact same type on both sides -- so even `int x; long n=5; x = n;`
// fatal'd, and `buf[i] = 'a' + i;` for a char array/buffer could never
// work. Both are fixed; this must NOT be flagged.

int main() {
  char c = 'a';         // declaration-initializer int->char conversion
  c = 'b';              // plain assignment int->char conversion
  char buf[4];
  for (int i = 0; i < 4; i++) buf[i] = 'w' + i; // array-element assignment
  int x;
  long n = 5;
  x = n;                // plain assignment long->int conversion
  printf("%d %d %d %d\n", c, buf[0], buf[3], x);
  return 0;
}
