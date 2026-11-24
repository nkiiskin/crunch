#include <math.h>
#include <stdio.h>

extern double __crunch_pow(double, double);

int main(void)
{
    const double cases[][2] = {
        {2, 8}, {2, -2}, {2, 0.5}, {9, 0.5}, {-2, 3}, {-2, 4},
        {-2, 0.5}, {0, -1}, {-0.0, -3}, {-0.0, 3}, {-0.0, 2},
        {-INFINITY, 3}, {-INFINITY, -3}, {INFINITY, -1},
        {NAN, 0}, {1, NAN}, {-1, INFINITY}, {0.5, INFINITY},
        {2, -INFINITY}, {2, 1e308}, {0.5, 1e308}, {2, -1074},
        {0.9999999999999999, 1000000000}, {1.0001, -10000}
    };
    for (unsigned repeat = 0; repeat < 100; ++repeat) {
        for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
            double expected = pow(cases[i][0], cases[i][1]);
            double actual = __crunch_pow(cases[i][0], cases[i][1]);
            int ok = isnan(expected) ? isnan(actual) :
                     isinf(expected) || expected == 0 ? actual == expected &&
                         signbit(actual) == signbit(expected) :
                     isfinite(actual) && fabs((actual - expected) / expected) <= 1e-14;
            if (!ok) {
                fprintf(stderr, "pow(%g, %g): got %.17g, expected %.17g\n",
                        cases[i][0], cases[i][1], actual, expected);
                return 1;
            }
        }
    }
    return 0;
}
