#ifndef CRUNCH_STRING_H
#define CRUNCH_STRING_H

#include <stddef.h>

#ifndef NULL
#macro NULL 0
#endif

/**
 * @brief Copies a byte range between non-overlapping memory regions.
 * @param destination Start of the destination region.
 * @param source Start of the source region.
 * @param byte_count Number of bytes to copy.
 * @return destination.
 */
void* memcpy(void* destination, const void* source, size_t byte_count);

/**
 * @brief Copies a byte range, permitting overlap.
 * @param destination Start of the destination region.
 * @param source Start of the source region.
 * @param byte_count Number of bytes to copy.
 * @return destination.
 */
void* memmove(void* destination, const void* source, size_t byte_count);

/**
 * @brief Fills a byte range with the low byte of a value.
 * @param destination Start of the destination region.
 * @param value Integer whose low byte is repeated.
 * @param byte_count Number of bytes to fill.
 * @return destination.
 */
void* memset(void* destination, int value, size_t byte_count);

/**
 * @brief Compares two byte ranges lexicographically.
 * @param left First byte range.
 * @param right Second byte range.
 * @param byte_count Maximum number of bytes to compare.
 * @return Negative, zero, or positive according to the first differing bytes.
 */
int memcmp(const void* left, const void* right, size_t byte_count);

/**
 * @brief Finds the first matching byte in a bounded memory region.
 * @param memory Region to search.
 * @param value Integer whose low byte is sought.
 * @param byte_count Number of bytes to search.
 * @return Pointer to the matching byte, or NULL if none is found.
 */
void* memchr(const void* memory, int value, size_t byte_count);

/**
 * @brief Compares two NUL-terminated strings lexicographically.
 * @param left First string.
 * @param right Second string.
 * @return Negative, zero, or positive according to the first differing bytes.
 */
int strcmp(const char* left, const char* right);

/**
 * @brief Counts the bytes before a string's NUL terminator.
 * @param text NUL-terminated string to measure.
 * @return String length in bytes.
 */
size_t strlen(const char* text);

/**
 * @brief Compares up to a specified number of bytes in two strings.
 * @param left First string.
 * @param right Second string.
 * @param byte_count Maximum number of bytes to compare.
 * @return Negative, zero, or positive according to the first differing bytes.
 */
int strncmp(const char* left, const char* right, size_t byte_count);

/**
 * @brief Copies a NUL-terminated string.
 * @param destination Writable destination buffer large enough for the copy.
 * @param source NUL-terminated source string.
 * @return destination.
 */
char* strcpy(char* destination, const char* source);

/**
 * @brief Appends a NUL-terminated string to another string.
 * @param destination Writable destination string with room for the result.
 * @param source NUL-terminated string to append.
 * @return destination.
 */
char* strcat(char* destination, const char* source);

/**
 * @brief Finds the first occurrence of a byte in a NUL-terminated string.
 * @param text String to search.
 * @param value Byte value to find.
 * @return Pointer to the matching byte, or NULL if no match is found.
 */
char* strchr(const char* text, int value);

/**
 * @brief Finds the last occurrence of a byte in a NUL-terminated string.
 * @param text String to search.
 * @param value Byte value to find.
 * @return Pointer to the last matching byte, or NULL if no match is found.
 */
char* strrchr(const char* text, int value);

/**
 * @brief Finds the first occurrence of a substring.
 * @param text NUL-terminated string to search.
 * @param needle NUL-terminated substring to find.
 * @return Pointer to the match, or NULL if no match is found.
 */
char* strstr(const char* text, const char* needle);

/**
 * @brief Describes a runtime error number.
 * @param error_number Linux error number.
 * @return Pointer to a static error message; do not modify it.
 * @note Error numbers outside 0 through 40 use a generic message.
 */
char* strerror(int error_number);

/**
 * @brief Allocates a copy of a NUL-terminated string.
 * @param text String to copy.
 * @return Allocated copy, or NULL with errno set on allocation failure.
 */
char* strdup(const char* text);

/**
 * @brief Splits a string into nonempty tokens using shared internal state.
 * @param text String to modify, or NULL to continue.
 * @param delimiters NUL-terminated set of delimiter bytes.
 * @return Next token, or NULL when no token remains.
 * @note Not reentrant or thread-safe; use strtok_r for independent state.
 */
char* strtok(char* text, const char* delimiters);

/**
 * @brief Splits a string into nonempty tokens using caller-owned state.
 * @param text String to modify, or NULL to continue.
 * @param delimiters NUL-terminated set of delimiter bytes.
 * @param save Pointer to the continuation state.
 * @return Next token, or NULL when no token remains.
 */
char* strtok_r(char* text, const char* delimiters, char** save);

#endif /* CRUNCH_STRING_H */
