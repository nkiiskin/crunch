#include "../../../src/codegen/x86_64.c"

/**
 * @brief Compares the backend's restricted scanner with libc.
 * @note This function takes no arguments.
 * @return Zero for matching results, nonzero for a mismatch.
 */
int main(void) {
    const char *inputs[] = {
        "", " ", "word", "  alpha\tbeta gamma ", "alpha beta",
        "abcdefghijklmnopqrstuvwxyz0123456789 rest", "  a\nb c",
        "L", "L+", "L-12", "L  +4294967296tail", "L18446744073709551616",
        "LX", "x1"
    };
    for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; i++) {
        char a[32] = {0}, b[32] = {0}, c[32] = {0};
        char x[32] = {0}, y[32] = {0}, z[32] = {0};
        int n = -1, m = -1;
        int host = sscanf(inputs[i], "%31s %31s %31s %n", a, b, c, &n);
        int actual = scan_ir(inputs[i], "%31s %31s %31s %n", x, y, z, &m);
        if (host != actual || n != m || strcmp(a, x) || strcmp(b, y) || strcmp(c, z))
            return 1;
        a[0] = x[0] = 0;
        host = sscanf(inputs[i], "%*s %31s", a);
        actual = scan_ir(inputs[i], "%*s %31s", x);
        if (host != actual || strcmp(a, x)) return 2;
        unsigned value = 123, other = 123;
        n = m = -1;
        errno = 0;
        host = sscanf(inputs[i], "L%u%n", &value, &n);
        int saved_errno = errno;
        errno = 0;
        actual = scan_ir(inputs[i], "L%u%n", &other, &m);
        if (host != actual || value != other || n != m || errno != saved_errno)
            return 3;
    }
    return 0;
}
