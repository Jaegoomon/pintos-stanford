#include "threads/arithmetic.h"

fixed_point int2fixed(int n)
{
    return n * F;
}

int fixed2int(fixed_point x)
{
    return x >= 0 ? (x + F / 2) / F : (x - F / 2) / F;
}

fixed_point x_add_y(fixed_point x, fixed_point y)
{
    return x + y;
}

fixed_point x_add_n(fixed_point x, int n)
{
    return x + int2fixed(n);
}

fixed_point x_sub_y(fixed_point x, fixed_point y)
{
    return x - y;
}

fixed_point x_sub_n(fixed_point x, int n)
{
    return x - int2fixed(n);
}

fixed_point x_mul_y(fixed_point x, fixed_point y)
{
    return ((int64_t)x) * y / F;
}

fixed_point x_mul_n(fixed_point x, int n)
{
    return x * n;
}

fixed_point x_div_y(fixed_point x, fixed_point y)
{
    return ((int64_t)x) * F / y;
}

fixed_point x_div_n(fixed_point x, int n)
{
    return x / n;
}