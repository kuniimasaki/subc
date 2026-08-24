#include <stdlib.h>
#include <string.h>

// strlen-use-after-free: strlen (like every string.h primitive here)
// resolves its argument through the same requireNotFreed() check as
// every other memory access, so calling it on a freed pointer is caught
// the same way a raw *ptr use-after-free would be.

int main() {
    char *p = (char *)malloc((long)6);
    char *hello = "hello";
    strcpy(p, hello);
    free(p);
    strlen(p);
    return 0;
}
