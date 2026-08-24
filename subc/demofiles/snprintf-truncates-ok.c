#include <stdio.h>
#include <string.h>

// snprintf-truncates-ok: the safe counterpart to sprintf-buffer-overflow.c
// -- snprintf must never write past its given capacity, truncating instead
// (while still reporting the length that *would* have been written, like
// real C's snprintf), so this must NOT be flagged as an overflow.

int main() {
    char buf[8];
    int n = snprintf(&buf[0], (long)8, "this is way too long");
    printf("%s %ld %d\n", &buf[0], strlen(&buf[0]), n == 20);
    return 0;
}
