#ifndef CRUNCH_COMMON_IDENTIFIER_H
#define CRUNCH_COMMON_IDENTIFIER_H

/**
 * @brief Tests whether a byte can begin a Crunch identifier.
 * @param ch Byte value to test.
 * @return Nonzero for an ASCII letter or underscore; otherwise zero.
 */
static inline int crunch_identifier_start(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

/**
 * @brief Tests whether a byte can continue a Crunch identifier.
 * @param ch Byte value to test.
 * @return Nonzero for an identifier-start byte or ASCII digit; otherwise zero.
 */
static inline int crunch_identifier_continue(unsigned char ch)
{
    return crunch_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

#endif
