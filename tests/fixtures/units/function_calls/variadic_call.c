#include <stdarg.h>

union Pair { struct { long integer; double real; } values; };
union Big { long values[3]; };

/**
 * @brief Checks arguments received from a Crunch-compiled variadic caller.
 * @param mode Selects the expected argument sequence.
 * @return One if the sequence matches, or zero otherwise.
 */
int variadic_probe(int mode, ...)
{
    va_list ap;
    int ok = 1;
    va_start(ap, mode);
    if (mode == 0) {
        int integer = va_arg(ap, int);
        double real = va_arg(ap, double);
        ok = integer == -5 && real == 1.5;
    } else if (mode == 1) {
        for (int i = 1; i <= 7; ++i)
            if (va_arg(ap, int) != i) ok = 0;
        for (int i = 1; i <= 9; ++i)
            if (va_arg(ap, double) != (double)i) ok = 0;
    } else if (mode == 2) {
        union Pair pair = va_arg(ap, union Pair);
        double real = va_arg(ap, double);
        ok = pair.values.integer == 42 && pair.values.real == 1.5 && real == 2.5;
    } else if (mode == 4) {
        ok = va_arg(ap, int) == -5;
    } else if (mode == 5) {
        union Big big = va_arg(ap, union Big);
        int integer = va_arg(ap, int);
        ok = big.values[0] == 10 && big.values[1] == 20 &&
             big.values[2] == 30 && integer == 7;
    } else if (mode == 6) {
        int character = va_arg(ap, int);
        int short_value = va_arg(ap, int);
        int boolean = va_arg(ap, int);
        int plain = va_arg(ap, int);
        unsigned int wide = va_arg(ap, unsigned int);
        ok = character == 255 && short_value == 65535 && boolean == 1 &&
             plain == 255 && wide == 0xffffffffU;
    } else
        ok = 0;
    va_end(ap);
    return ok;
}

/**
 * @brief Checks named and variadic floating-point arguments.
 * @param fixed Named floating-point argument.
 * @param mode Named integer argument.
 * @return One if the sequence matches, or zero otherwise.
 */
int variadic_probe_fp(double fixed, int mode, ...)
{
    va_list ap;
    va_start(ap, mode);
    int integer = va_arg(ap, int);
    double real = va_arg(ap, double);
    va_end(ap);
    return fixed == 4.0 && mode == 3 && integer == -5 && real == 1.5;
}

/**
 * @brief Returns a register-class aggregate from variadic arguments.
 * @param mode Named integer argument.
 * @return Aggregate containing the requested values.
 */
union Pair variadic_return(int mode, ...)
{
    va_list ap;
    va_start(ap, mode);
    union Pair result;
    result.values.integer = va_arg(ap, long);
    result.values.real = va_arg(ap, double);
    va_end(ap);
    if (mode != 7) result.values.integer = 0;
    return result;
}

/**
 * @brief Returns a memory-class aggregate from variadic arguments.
 * @param mode Named integer argument.
 * @return Aggregate containing the requested values.
 */
union Big variadic_return_big(int mode, ...)
{
    va_list ap;
    va_start(ap, mode);
    union Big result;
    result.values[0] = va_arg(ap, int);
    result.values[1] = 20;
    result.values[2] = 30;
    va_end(ap);
    if (mode != 8) result.values[0] = 0;
    return result;
}
