#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

extern void *_crunch_malloc(size_t);
extern void *_crunch_realloc(void *, size_t);
extern void _crunch_free(void *);
extern int *__crunch___errno_location(void);

int main(void)
{
    const long sizes[] = {0, 1, 15, 16, 17, 4095, 4096, 4097, 8192};
    int *error_number = __crunch___errno_location();
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        long size = sizes[i];
        *error_number = EDOM;
        unsigned char *p = _crunch_malloc(size);
        if (!p || (uintptr_t)p % 16 != 0 || *error_number != EDOM)
            return 1;
        for (long j = 0; j < size; j++)
            p[j] = (unsigned char)(j % 251);
        long larger_size = size + 8193;
        unsigned char *larger = _crunch_realloc(p, larger_size);
        if (!larger || (uintptr_t)larger % 16 != 0 || *error_number != EDOM)
            return 2;
        for (long j = 0; j < size; j++)
            if (larger[j] != (unsigned char)(j % 251))
                return 3;
        larger[larger_size - 1] = 77;
        unsigned char *same_size = _crunch_realloc(larger, larger_size);
        if (!same_size || same_size[larger_size - 1] != 77 || *error_number != EDOM)
            return 4;
        _crunch_free(same_size);
        if (*error_number != EDOM)
            return 5;
    }
    *error_number = EDOM;
    if (_crunch_malloc(LONG_MAX) || *error_number != ENOMEM)
        return 6;
    unsigned char *p = _crunch_malloc(32);
    if (!p) return 7;
    p[0] = 42;
    *error_number = EDOM;
    if (_crunch_realloc(p, LONG_MAX) || *error_number != ENOMEM || p[0] != 42)
        return 8;
    _crunch_free(p);
    return 0;
}
