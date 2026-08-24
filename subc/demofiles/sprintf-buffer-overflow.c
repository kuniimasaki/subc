#include <stdio.h>

// sprintf-buffer-overflow: sprintf, like strcpy, writes through setMemory()
// one byte at a time, so overflowing the destination hits the same
// bounds-check detection every other subc write does. This is as classic
// a memory bug pattern as strcpy overflow.

int main() {
    char buf[4];
    sprintf(&buf[0], "this is way too long for a 4-byte buffer");
    return 0;
}
