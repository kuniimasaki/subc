int nfib(int n) {
    if (n < 2) return 1;
    return 1 + nfib(n-1) + nfib(n-2);
}
int main(){
    nfib(32);
    return 0;
}