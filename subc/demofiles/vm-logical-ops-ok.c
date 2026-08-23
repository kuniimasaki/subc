int main(void) {
    int x = 1, y = 0;
    int land1 = 0;
    if (x && y) land1 = 1; // stays 0
    int land2 = 0;
    if (x && x) land2 = 1; // becomes 1
    int lor1 = 0;
    if (y || x) lor1 = 1; // becomes 1
    int lor2 = 0;
    if (y || y) lor2 = 1; // stays 0
    return land1*1 + land2*2 + lor1*4 + lor2*8; // 0+2+4+0 = 6
}
