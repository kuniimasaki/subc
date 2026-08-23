int main(void) {
    int a[5];
    a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4; a[4] = 5;
    int sum = a[0] + a[1] + a[2] + a[3] + a[4]; // 15

    int b[3] = { 10, 20, 30 };
    sum = sum + b[0] + b[1] + b[2]; // +60 = 75

    return sum;
}
