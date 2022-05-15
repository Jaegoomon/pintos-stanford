#include <debug.h>
#include <stdint.h>

typedef int fixed_point;

#define F 16384

fixed_point int2fixed(int n);
int fixed2int(fixed_point x);
fixed_point x_add_y(fixed_point x, fixed_point y);
fixed_point x_sub_y(fixed_point x, fixed_point y);
fixed_point x_mul_y(fixed_point x, fixed_point y);
fixed_point x_div_y(fixed_point x, fixed_point y);
fixed_point x_add_n(fixed_point x, int n);
fixed_point x_sub_n(fixed_point x, int n);
fixed_point x_mul_n(fixed_point x, int n);
fixed_point x_div_n(fixed_point x, int n);