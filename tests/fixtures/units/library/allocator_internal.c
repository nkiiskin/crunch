#include <errno.h>
#include <stdint.h>
#include <stddef.h>

extern void *_crunch_malloc(size_t);
extern void *_crunch_realloc(void *, size_t);
extern void _crunch_free(void *);
extern int *__crunch___errno_location(void);
extern int remap_calls;

int main(void)
{
    int *error_number = __crunch___errno_location();
    *error_number = EDOM;
    unsigned char *p = _crunch_malloc(512);
    if (!p || (uintptr_t)p % 16 || *error_number != EDOM)
        return 1;
    p[0] = 42;
    p[31] = 99;
    if (_crunch_realloc(p, 8192) || remap_calls != 1 || *error_number != ENOMEM ||
        p[0] != 42 || p[31] != 99)
        return 2;
    *error_number = EDOM;
    _crunch_free(p);
    if (*error_number != EDOM)
        return 3;
    return 0;
}
