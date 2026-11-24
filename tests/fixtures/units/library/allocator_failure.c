#include <errno.h>
#include <stddef.h>

extern void *_crunch_malloc(size_t);
extern void *_crunch_realloc(void *, size_t);
extern int *__crunch___errno_location(void);

int main(void)
{
    int *error_number = __crunch___errno_location();
    *error_number = 0;
    if (_crunch_malloc(32) || *error_number != EACCES)
        return 1;
    *error_number = 0;
    if (_crunch_realloc(0, 32) || *error_number != EACCES)
        return 2;
    return 0;
}
