#ifndef CRUNCH_STDLIB_H
#define CRUNCH_STDLIB_H

#include <stddef.h>

#ifndef NULL
#macro NULL 0
#endif

/**
 * @brief Terminates the current process.
 * @param status Process exit status.
 * @return Does not return.
 */
void noreturn exit(int status);

/**
 * @brief Parses ASCII whitespace, an optional sign, and digits in the requested base.
 * @param text Non-null NUL-terminated input string.
 * @param end Optional writable output receiving the first unconsumed character, or
 * the original input when no digits are converted or the base is invalid. The
 * result points into text and is writable only if text is writable.
 * @param base Zero for automatic detection, or a base from 2 through 36.
 * @return Parsed signed long. Overflow consumes all valid digits, returns the
 * appropriate signed long limit, and sets errno to ERANGE. An invalid base returns
 * zero and sets EINVAL. Success and no conversion leave errno unchanged.
 */
long strtol(const char* text, char** end, int base);

/**
 * @brief Parses ASCII whitespace, an optional sign, and digits in the requested base.
 * @param text Non-null NUL-terminated input string.
 * @param end Optional writable output receiving the first unconsumed character, or
 * the original input when no digits are converted or the base is invalid. The
 * result points into text and is writable only if text is writable.
 * @param base Zero for automatic detection, or a base from 2 through 36.
 * @return Parsed unsigned long, with a negative sign applied modulo 2^64.
 * Overflow returns the maximum unsigned long and sets errno to ERANGE after
 * consuming all valid digits. An invalid base returns zero and sets EINVAL.
 * Success and no conversion leave errno unchanged.
 */
unsigned long strtoul(const char* text, char** end, int base);

/**
 * @brief Parses an integer into a signed long long.
 * @param text NUL-terminated input string.
 * @param end Optional output receiving the first unconsumed character.
 * @param base Zero for automatic detection, or a base from 2 through 36.
 * @return Parsed value; overflow sets ERANGE and an invalid base sets EINVAL.
 */
long long strtoll(const char* text, char** end, int base);

/**
 * @brief Parses an integer into an unsigned long long.
 * @param text NUL-terminated input string.
 * @param end Optional output receiving the first unconsumed character.
 * @param base Zero for automatic detection, or a base from 2 through 36.
 * @return Parsed value; overflow sets ERANGE and an invalid base sets EINVAL.
 */
unsigned long long strtoull(const char* text, char** end, int base);

/**
 * @brief Parses an ASCII floating-point subject sequence as binary64.
 * @param text Non-null NUL-terminated input string.
 * @param end Optional output receiving the first unconsumed character, or text
 * when no conversion is performed.
 * @return Nearest binary64 value, ties to even. Overflow and inexact results
 * that round to a subnormal or zero set ERANGE; other results leave errno unchanged.
 */
double strtod(const char* text, char** end);

/**
 * @brief Parses an ASCII floating-point subject sequence directly as binary32.
 * @param text Non-null NUL-terminated input string.
 * @param end Optional output receiving the first unconsumed character, or text
 * when no conversion is performed.
 * @return Nearest binary32 value, ties to even. Overflow and inexact results
 * that round to a subnormal or zero set ERANGE; other results leave errno unchanged.
 */
float strtof(const char* text, char** end);

/**
 * @brief Parses an ASCII floating-point subject sequence as binary80.
 * @param text Non-null NUL-terminated input string.
 * @param end Optional output receiving the first unconsumed character, or text
 * when no conversion is performed.
 * @return Nearest binary80 value, ties to even. Overflow and inexact results
 * that round to a subnormal or zero set ERANGE; other results leave errno unchanged.
 */
long double strtold(const char* text, char** end);

/**
 * @brief Allocates an independent block of memory.
 * @param byte_count Number of bytes to allocate.
 * @return Aligned pointer on success, or null with errno set on failure.
 */
void* malloc(size_t byte_count);

/**
 * @brief Allocates a zero-filled block for count objects.
 * @param count Number of objects.
 * @param size Size of each object in bytes.
 * @return Aligned pointer on success, or null with errno set on failure.
 */
void* calloc(size_t count, size_t size);

/**
 * @brief Releases a block allocated by malloc or realloc.
 * @param address Allocation pointer, or null.
 * @return No value; an underlying unmap failure sets errno.
 */
void free(void* address);

/**
 * @brief Resizes a block allocated by malloc or realloc.
 * @param address Allocation pointer, or null to allocate a new block.
 * @param byte_count New size in bytes.
 * @return Resized pointer, or null on failure or when freeing a zero-size block;
 *         on failure the original allocation remains valid and errno is set.
 */
void* realloc(void* address, size_t byte_count);

/**
 * @brief Retries releasing retained allocator mappings and empty arenas.
 * @return Zero on success, or -1 with errno set to the first unmap error.
 */
int allocator_trim();

/**
 * @brief Parses a decimal integer from a NUL-terminated string.
 * @param text String to parse.
 * @return Parsed value narrowed to int; returns zero when no digits are present.
 */
int atoi(const char* text);

/**
 * @brief Parses a decimal integer from a NUL-terminated string.
 * @param text String to parse.
 * @return Parsed long value; returns zero when no digits are present.
 */
long atol(const char* text);

#endif /* CRUNCH_STDLIB_H */
