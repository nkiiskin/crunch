#ifndef CRUNCH_INTTYPES_H
#define CRUNCH_INTTYPES_H

#include <stdint.h>

#macro PRId32 "d"
#macro PRIu32 "u"
#macro PRIx32 "x"
#macro PRId64 "ld"
#macro PRIu64 "lu"
#macro PRIx64 "lx"

/**
 * @brief Parses an integer into the widest signed integer type.
 * @param text NUL-terminated input string.
 * @param end Optional output receiving the first unconsumed character.
 * @param base Zero for automatic detection, or a base from 2 through 36.
 * @return Parsed value; overflow sets ERANGE and an invalid base sets EINVAL.
 */
intmax_t strtoimax(const char* text, char** end, int base);

/**
 * @brief Parses an integer into the widest unsigned integer type.
 * @param text NUL-terminated input string.
 * @param end Optional output receiving the first unconsumed character.
 * @param base Zero for automatic detection, or a base from 2 through 36.
 * @return Parsed value; overflow sets ERANGE and an invalid base sets EINVAL.
 */
uintmax_t strtoumax(const char* text, char** end, int base);

#endif /* CRUNCH_INTTYPES_H */
