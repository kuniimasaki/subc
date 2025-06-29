# include <stdint.h> // uint32_t
# include <stdio.h>
float Q_rsqrt( float number )
{
	long i;
	float x2, y;
	float threehalfs = 1.5;

	x2 = number * 0.5;
    //printf("t1:%f\n",x2);
	y  = number;
	i  = * ( long * ) &y;                       // evil floating point bit level hacking
	//printf("t1:%ld\n",i);
    i  = (long)(0x5f3759df - ( i >> 1 ));               // what the fuck?
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}

int main(){
    float n = (float)100.0;
    printf("%f\n",Q_rsqrt(n));
}