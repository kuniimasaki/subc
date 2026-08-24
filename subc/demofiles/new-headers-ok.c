#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

// new-headers-ok: correctness check for math.h/ctype.h/stdlib.h additions
// and stdio.h's putchar/getchar/puts (sprintf/snprintf get their own
// dedicated tests below, since they're the ones with memory-safety
// implications worth isolating).

int main() {
    printf("%d\n", sqrtf(16.0) == 4.0);
    printf("%d\n", fabsf(-3.5) == 3.5);
    printf("%d\n", floorf(3.7) == 3.0);
    printf("%d\n", ceilf(3.2) == 4.0);
    printf("%d\n", powf(2.0, 10.0) == 1024.0);

    printf("%d %d\n", isalpha('a'), isalpha('5'));
    printf("%d %d\n", isdigit('5'), isdigit('a'));
    printf("%d %d\n", isupper('A'), islower('a'));
    printf("%c %c\n", toupper('a'), tolower('A'));

    printf("%d %d\n", abs(-5), abs(5));
    char *n = "123";
    printf("%ld\n", atol(n));

    srand(42);
    int r1 = rand();
    srand(42);
    int r2 = rand();
    printf("%d\n", r1 == r2);

    putchar('o');
    putchar('k');
    putchar('\n');
    char *s = "puts-ok";
    puts(s);

    return 0;
}
