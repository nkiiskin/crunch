#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern int __crunch_vsnprintf(char *, size_t, const char *, va_list);

/**
 * @brief Compares the runtime formatter with libc for one argument sequence.
 * @param format Format string followed by its corresponding values.
 * @return Nonzero when both formatters agree, including truncated output.
 */
static int compare(const char *format, ...)
{
    char expected[512], actual[512], truncated[9];
    va_list ap, copy;
    va_start(ap, format);
    va_copy(copy, ap);
    int a = vsnprintf(expected, sizeof expected, format, copy);
    va_end(copy);
    va_copy(copy, ap);
    int b = __crunch_vsnprintf(actual, sizeof actual, format, copy);
    va_end(copy);
    va_copy(copy, ap);
    int c = __crunch_vsnprintf(truncated, sizeof truncated, format, copy);
    va_end(copy);
    int d = __crunch_vsnprintf(NULL, 0, format, ap);
    va_end(ap);
    return a == b && b == c && c == d && strcmp(expected, actual) == 0 &&
           strncmp(expected, truncated, sizeof truncated - 1) == 0 &&
           truncated[sizeof truncated - 1] == '\0';
}

/**
 * @brief Exercises register and stack argument handling in the runtime formatter.
 * @return Zero on success, or a nonzero failing case number.
 */
int main(void)
{
    const char bounded[] = {'a', 'b', 'c'};
    if (!compare("%d %u %lx %s %.*s %.17g %f", -123, 123U,
                 0xabcdefUL, "message", 3, bounded, 1.25, 2.5)) return 1;
    if (!compare("%d %d %d %d %d %d %d %d %d %d",
                 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)) return 2;
    if (!compare("%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g",
                 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0)) return 3;
    if (!compare("%.*s %.*s", -1, "negative", 0, "unused")) return 4;
    if (!compare("%d %d %d %d %d %d %d %.17g %.17g %.17g %.17g %.17g "
                 "%.17g %.17g %.17g %.17g %d %.17g",
                 1, 2, 3, 4, 5, 6, 7, 1.0, 2.0, 3.0, 4.0, 5.0,
                 6.0, 7.0, 8.0, 9.0, 8, 10.0)) return 5;
    return 0;
}
