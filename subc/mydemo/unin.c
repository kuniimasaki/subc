#include <stdio.h>
struct S {
	int i1 ;
	int i2 ;
	int i3 ;
} ;

int main(void)
{
	struct S s1 ;
	s1.i1 = 0 ;
	s1.i2 = 42 ;
    printf("%d\n",s1.i3);
	return 0 ;
}