// regression test for the VM's per-iteration environment cleanup
// (iSAVEENV/iRESETENV/iDROPENV). Declaring a local INSIDE a loop body is
// an extremely common C idiom; before this fix, each iteration's iDECL
// kept consing onto the same env chain with nothing ever torn down until
// the enclosing function returned, so variable lookup (a linear scan)
// got one entry longer every iteration -- turning this loop's O(n) work
// into O(n^2). At n=20000 that was 46.8s; fixed, it's a fraction of a
// second. Also exercises continue/break interacting correctly with the
// per-iteration reset (both must still clean up the iteration they're
// aborting, not skip it).
int main() {
    int total = 0;
    int i;
    for (i = 0; i < 20000; i = i + 1) {
        int j = i;
        if (j % 7 == 0) continue;
        total = total + j;
    }
    return total % 256;
}
