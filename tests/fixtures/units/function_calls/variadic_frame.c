#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

struct state {
    unsigned gp_offset, fp_offset;
    void *overflow, *registers;
};
struct mixed { long integer; double real; };
struct large { long values[3]; };

extern int probe_simple(int, ...);
extern int probe_vector(int, ...);
extern int probe_stack(long, long, long, long, long, long, long,
                       double, double, double, double, double, double, double, double, double, ...);
extern int probe_mixed(long, long, long, long, long, long, struct mixed, ...);
extern struct large probe_hidden(int, ...);

static int hidden_error;

int inspect_simple(void *raw)
{
    struct state *state = raw;
    va_list *ap = raw;
    int ok = state->gp_offset == 8 && state->fp_offset == 48 &&
             va_arg(*ap, int) == 42 && va_arg(*ap, double) == 6.25 &&
             strcmp(va_arg(*ap, const char *), "ok") == 0;
    return !ok;
}

int inspect_stack(void *raw)
{
    struct state *state = raw;
    va_list ap;
    memcpy(ap, raw, sizeof ap);
    int ok = state->gp_offset == 48 && state->fp_offset == 176 &&
             va_arg(ap, int) == 123 && va_arg(ap, double) == 3.75;
    va_end(ap);
    return !ok;
}

int inspect_vector(void *raw)
{
    va_list ap;
    float values[4];
    memcpy(ap, raw, sizeof ap);
    __m128 value = va_arg(ap, __m128);
    memcpy(values, &value, sizeof values);
    va_end(ap);
    return values[0] != 1.0f || values[1] != 2.0f ||
           values[2] != 3.0f || values[3] != 4.0f;
}

int inspect_mixed(void *raw)
{
    struct state *state = raw;
    va_list ap;
    memcpy(ap, raw, sizeof ap);
    int ok = state->gp_offset == 48 && state->fp_offset == 48 &&
             va_arg(ap, double) == 2.5 && va_arg(ap, int) == 71;
    va_end(ap);
    return !ok;
}

int inspect_hidden(void *raw)
{
    struct state *state = raw;
    va_list ap;
    memcpy(ap, raw, sizeof ap);
    hidden_error = !(state->gp_offset == 16 && state->fp_offset == 48 &&
                     va_arg(ap, int) == 84 && va_arg(ap, double) == 1.75);
    va_end(ap);
    return 0;
}

int main(void)
{
    struct mixed value = {33, 1.0};
    __m128 vector = {1.0f, 2.0f, 3.0f, 4.0f};
    if (probe_simple(1, 42, 6.25, "ok")) return 1;
    if (probe_vector(1, vector)) return 5;
    if (probe_stack(1, 2, 3, 4, 5, 6, 7,
                    1., 2., 3., 4., 5., 6., 7., 8., 9., 123, 3.75)) return 2;
    if (probe_mixed(1, 2, 3, 4, 5, 6, value, 2.5, 71)) return 3;
    (void)probe_hidden(1, 84, 1.75);
    return hidden_error ? 4 : 0;
}
