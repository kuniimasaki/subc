#include <stdio.h>

int main() {
    int value = 5; // 初期値5
    printf("シフト前: %d\n", value);

    value = value << 1; // 左に1ビットシフト
    printf("シフト後: %d\n", value);

    return 0;
}
