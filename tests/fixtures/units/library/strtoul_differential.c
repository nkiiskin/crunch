#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int crunch_errno;
unsigned long __crunch_strtoul(const char *, char **, int);
long __crunch_strtol(const char *, char **, int);

int *_crunch_errno_location(void) { return &crunch_errno; }

static int compare(const char *text, int base) {
    char *expected_end, *actual_end;
    errno = 123;
    crunch_errno = 123;
    unsigned long expected = strtoul(text, &expected_end, base);
    int expected_errno = errno;
    unsigned long actual = __crunch_strtoul(text, &actual_end, base);
    if (actual != expected || actual_end - text != expected_end - text ||
        crunch_errno != expected_errno) {
        fprintf(stderr, "strtoul(%s, %d): %lu/%ld/%d != %lu/%ld/%d\n",
                text, base, actual, (long)(actual_end - text), crunch_errno,
                expected, (long)(expected_end - text), expected_errno);
        return 1;
    }
    errno = 123;
    crunch_errno = 123;
    long signed_expected = strtol(text, &expected_end, base);
    expected_errno = errno;
    long signed_actual = __crunch_strtol(text, &actual_end, base);
    if (signed_actual != signed_expected || actual_end - text != expected_end - text ||
        crunch_errno != expected_errno) {
        fprintf(stderr, "strtol(%s, %d): %ld/%ld/%d != %ld/%ld/%d\n",
                text, base, signed_actual, (long)(actual_end - text), crunch_errno,
                signed_expected, (long)(expected_end - text), expected_errno);
        return 1;
    }
    return 0;
}

int main(void) {
    const char *cases[] = {
        "", " ", " +", " -xyz", "0", "00", "0x", "0Xg", "0x1",
        "0xFfZ", "0778", "10102", "zZ", "  \t\n\v\f\r-123tail",
        "18446744073709551614", "18446744073709551615",
        "18446744073709551616", "184467440737095516160000000!",
        "-18446744073709551615", "-18446744073709551616",
        "9223372036854775806", "9223372036854775807", "9223372036854775808",
        "-9223372036854775807", "-9223372036854775808", "-9223372036854775809",
        "7fffffffffffffff", "8000000000000000", "-8000000000000000",
        "-8000000000000001", "9999999999999999999999999999999999999999!",
        "ffffffffffffffff", "10000000000000000", "+000000000000000000000001",
        "\200", "12\377"
    };
    for (int base = 0; base <= 36; ++base) {
        if (base == 1) continue;
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
            if (compare(cases[i], base)) return 1;
    }
    unsigned state = 0x12345678;
    const char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+- \t!";
    for (int i = 0; i < 20000; ++i) {
        char text[50];
        state = state * 1664525u + 1013904223u;
        size_t length = state % (sizeof(text) - 1);
        for (size_t j = 0; j < length; ++j) {
            state = state * 1664525u + 1013904223u;
            text[j] = alphabet[state % (sizeof(alphabet) - 1)];
        }
        text[length] = 0;
        if (compare(text, i % 36 == 0 ? 0 : i % 35 + 2)) return 1;
    }
    char *end;
    for (int base = -1; base <= 38; ++base) {
        if (base == 0 || (base >= 2 && base <= 36)) continue;
        const char *text = "123";
        crunch_errno = 123;
        if (__crunch_strtoul(text, &end, base) != 0 || end != text ||
            crunch_errno != EINVAL) return 1;
        crunch_errno = 123;
        if (__crunch_strtol(text, &end, base) != 0 || end != text ||
            crunch_errno != EINVAL) return 1;
    }
    crunch_errno = 123;
    if (__crunch_strtoul("42", NULL, 10) != 42 || crunch_errno != 123) return 1;
    if (__crunch_strtol("-42", NULL, 10) != -42 || crunch_errno != 123) return 1;
    return 0;
}
