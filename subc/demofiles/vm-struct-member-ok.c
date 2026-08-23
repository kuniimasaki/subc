struct Point { int x, y; };
int main(void) {
    struct Point p;
    p.x = 3;
    p.y = 4;
    int sum = p.x + p.y; // 7

    struct Point q = { 100, 200 };
    sum = sum + q.x + q.y; // +300 = 307

    return sum - 300; // 7
}
