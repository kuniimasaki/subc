extern void *malloc(long size);
extern void  free(void *pointer);
extern void *realloc(void *pointer, long size);
extern void *calloc(long n, long size);

extern void exit(int status);
extern void abort(void);

extern int    atoi(char *string);
extern long   atol(char *string);
extern double atof(char *string);
extern int    abs(int n);

extern int  rand(void);
extern void srand(int seed);
