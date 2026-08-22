#include <stdio.h>

// modulo-and-bitwise-ok: typeCheck()'s Binary case had `assert(!"unimplemented")`
// stubs for MOD (%), BAND (&), BXOR (^), BOR (|), LE (<=), GE (>=), LAND (&&),
// and LOR (||) -- meaning even a plain, non-compound `x % 4` or `a <= b` or
// `a && b` crashed the interpreter outright (confirmed this predates any
// change this session: main_orig, the pre-Task-1 binary, crashes identically
// on `x = x % 4;`). Found while adding compound-assignment support (%= etc.
// desugar to exactly these Binary nodes). Fixed by giving each a typeCheck
// result (MOD mirrors MUL/DIV's int/long/float/double check; the rest match
// the lenient `return t_int` stub already used for LT/GT/EQ/NE/SHL/SHR).

int main() {
  printf("%d\n", 10 % 3);       // 1
  printf("%d\n", 5 & 3);        // 1
  printf("%d\n", 5 | 2);        // 7
  printf("%d\n", 5 ^ 1);        // 4
  printf("%d\n", 3 <= 3);       // 1 (true)
  printf("%d\n", 4 >= 5);       // 0 (false)
  printf("%d\n", 1 && 1);       // 1 (true)
  printf("%d\n", 0 || 1);       // 1 (true)
  return 0;
}
