def nfib(n):
    if n < 2:
        return 1
    return 1 + nfib(n - 1) + nfib(n - 2)

print(nfib(34))
