#define _GNU_SOURCE
#include <errno.h>
#include <sys/mman.h>

extern void *_crunch_mmap(void *, long, int, int, int, long);
extern int _crunch_munmap(void *, long);
extern void *_crunch_mremap(void *, long, long, int, ...);
extern int *__crunch___errno_location(void);

int main(void)
{
    int *error_number = __crunch___errno_location();
    *error_number = EDOM;
    if (_crunch_mmap(0, 0, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED ||
        *error_number != EINVAL)
        return 1;
    *error_number = 0;
    if (_crunch_munmap(0, 0) != -1 || *error_number != EINVAL)
        return 2;
    *error_number = 0;
    if (_crunch_mremap(0, 4096, 8192, MREMAP_MAYMOVE) != MAP_FAILED ||
        *error_number != EFAULT)
        return 3;
    *error_number = EDOM;
    void *source = _crunch_mmap(0, 4096, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void *destination = _crunch_mmap(0, 4096, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (source == MAP_FAILED || destination == MAP_FAILED || *error_number != EDOM)
        return 4;
    *(char *)source = 42;
    source = _crunch_mremap(source, 4096, 8192, MREMAP_MAYMOVE);
    if (source == MAP_FAILED || *(char *)source != 42 || *error_number != EDOM)
        return 5;
    void *moved = _crunch_mremap(source, 8192, 4096,
                                 MREMAP_MAYMOVE | MREMAP_FIXED, destination);
    if (moved != destination || *(char *)moved != 42 || *error_number != EDOM)
        return 6;
    if (_crunch_munmap(moved, 4096) != 0 || *error_number != EDOM)
        return 7;
    return 0;
}
