int main(void) {
    int i = 0;
    int a = i++;   // a=0, i=1
    int b = ++i;   // i=2, b=2
    int c = i--;   // c=2, i=1
    int d = --i;   // i=0, d=0
    return a + b + c + d; // 0+2+2+0 = 4
}
