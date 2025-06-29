#include <stdio.h>

int main() {
    int value = 20; // 初期値20
    printf("シフト前: %d\n", value);

    value = value >> 1; // 右に1ビットシフト
    printf("シフト後: %d\n", value);

    return 0;
}
