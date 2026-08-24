#include <string.h>

// strcpy-buffer-overflow: the classic string-overflow bug this project's
// task2 design doc explicitly recommended string.h support for. strcpy()
// writes through setMemory() one byte at a time, so overflowing the
// destination hits the same bounds check every other subc write does,
// instead of silently corrupting adjacent memory the way real libc would.

int main() {
    char buf[4];
    char *s = "this is way too long";
    strcpy(&buf[0], s);
    return 0;
}
