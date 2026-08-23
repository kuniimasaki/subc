int main(void) {
    int sum = 0;
    for (int i = 0; i < 10; i = i + 1) {
        if (i == 5) continue;
        if (i == 8) break;
        sum = sum + i;
    }
    return sum; // 0+1+2+3+4+6+7 = 23
}
