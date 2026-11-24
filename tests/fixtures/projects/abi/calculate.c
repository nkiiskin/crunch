#include "calculate.h"
#include <stdio.h>

double calculate(double r1, int i1, double r2, int i2, double r3, int i3,
               double r4, int i4, double r5, int i5, double r6, int i6,
               double r7, int i7, double r8, int i8, double r9, int i9) {
    char buffer[32];
    snprintf(buffer, 32, "runtime %d", i9);
    puts(buffer);
    return r9 + i9;
}
