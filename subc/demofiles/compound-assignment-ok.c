#include <stdio.h>

// compound-assignment-ok: subc's grammar had no compound assignment
// operators at all (+= -= *= /= %= &= |= ^= <<= >>=), forcing every demo
// to spell out `x = x + n;` etc. Added by desugaring `l OP= r` into
// `newAssign(l, newBinary(OP, l, r, t), t)` at parse time -- no new AST
// node or evaluator change needed. Exercising %=, &=, |=, ^=, <<=, >>=
// here also happens to be the first real coverage of typeCheck()'s MOD/
// BAND/BXOR/BOR Binary cases, which turned out to be unimplemented stubs
// (`assert(!"unimplemented")`) even for the plain, non-compound operators
// -- fixed alongside this feature (see modulo-and-bitwise-ok.c).

int main() {
  int x = 10;
  x += 5;  printf("%d\n", x); // 15
  x -= 3;  printf("%d\n", x); // 12
  x *= 2;  printf("%d\n", x); // 24
  x /= 4;  printf("%d\n", x); // 6
  x %= 4;  printf("%d\n", x); // 2
  x = 6;
  x &= 3;  printf("%d\n", x); // 2
  x |= 8;  printf("%d\n", x); // 10
  x ^= 2;  printf("%d\n", x); // 8
  x <<= 2; printf("%d\n", x); // 32
  x >>= 3; printf("%d\n", x); // 4
  return 0;
}
