#ifndef CRUNCH_MATH_H
#define CRUNCH_MATH_H

/**
 * @brief Tests whether a floating-point value is finite.
 * @param value Floating-point value to classify.
 * @return Nonzero for finite values, including zero and subnormals.
 */
static inline int isfinite(long double value)
{
    unsigned char *bytes = (unsigned char *)&value;
    return ((bytes[9] & 127) * 256 + bytes[8]) != 32767;
}

/**
 * @brief Tests whether a floating-point value is a NaN.
 * @param value Floating-point value to classify.
 * @return Nonzero for a NaN.
 */
static inline int isnan(long double value)
{
    unsigned char *bytes = (unsigned char *)&value;
    if (((bytes[9] & 127) * 256 + bytes[8]) != 32767)
        return 0;
    if (bytes[7] & 127)
        return 1;
    for (int i = 0; i < 7; ++i)
        if (bytes[i])
            return 1;
    return 0;
}

/**
 * @brief Tests the sign bit of a floating-point value.
 * @param value Floating-point value to classify.
 * @return Nonzero if the sign bit is set.
 */
static inline int signbit(long double value)
{
    unsigned char *bytes = (unsigned char *)&value;
    return (bytes[9] & 128) != 0;
}

/**
 * @brief Clears the sign bit of a binary80 value.
 * @param value Floating-point value.
 * @return The value with a positive sign.
 */
static inline long double fabsl(long double value)
{
    unsigned char *bytes = (unsigned char *)&value;
    bytes[9] = bytes[9] & 127;
    return value;
}

/**
 * @brief Rounds a binary80 value toward zero.
 * @param value Floating-point value.
 * @return The integral value, retaining zero, infinity, and NaN signs.
 */
static inline long double truncl(long double value)
{
    unsigned char *bytes = (unsigned char *)&value;
    int exponent = (bytes[9] & 127) * 256 + bytes[8];
    if (exponent >= 16383 + 63)
        return value;
    if (exponent < 16383) {
        for (int i = 0; i < 9; ++i)
            bytes[i] = 0;
        bytes[9] = bytes[9] & 128;
        return value;
    }
    int fractional = 63 - (exponent - 16383);
    for (int i = 0; i < 8; ++i) {
        if (fractional >= 8) {
            bytes[i] = 0;
            fractional -= 8;
        } else if (fractional > 0) {
            bytes[i] = bytes[i] & (255 << fractional);
            fractional = 0;
        }
    }
    return value;
}

#endif /* CRUNCH_MATH_H */
