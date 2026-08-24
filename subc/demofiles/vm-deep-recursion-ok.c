// regression test for the VM's value/frame stacks growing dynamically
// instead of being hard-capped at 32 entries (docs/design/
// vm-implementation-status.md's former "known remaining issue" #2).
// Recursion depth 5000 is far beyond the old fixed limit, which used to
// fail with "too many function calls" / stack overflow well before this.

int depth(int n) {
    if (n == 0) return 0;
    return 1 + depth(n-1);
}

int main() {
    return depth(5000) % 256;
}
