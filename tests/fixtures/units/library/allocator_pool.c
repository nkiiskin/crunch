#include <errno.h>
#include <stddef.h>
#include <stdint.h>

extern void *_crunch_malloc(size_t);
extern void *_crunch_realloc(void *, size_t);
extern void _crunch_free(void *);
extern int *__crunch___errno_location(void);
extern void *__real__crunch_mmap(void *, long, int, int, int, long);
extern int __real__crunch_munmap(void *, long);

static int maps;
static int unmaps;
static int fail_map;
static int fail_unmap;
static unsigned char *blocks[1400];

void *__wrap__crunch_mmap(void *address, long size, int prot, int flags, int fd, long offset)
{
    if (fail_map) {
        *__crunch___errno_location() = ENOMEM;
        return (void *)(intptr_t)-1;
    }
    void *result = __real__crunch_mmap(address, size, prot, flags, fd, offset);
    if (result != (void *)(intptr_t)-1) maps++;
    return result;
}

int __wrap__crunch_munmap(void *address, long size)
{
    if (fail_unmap) {
        *__crunch___errno_location() = EINVAL;
        return -1;
    }
    int result = __real__crunch_munmap(address, size);
    if (result == 0) unmaps++;
    return result;
}

static int classes(void)
{
    const long sizes[] = {0, 1, 16, 17, 32, 33, 48, 49, 64, 65, 80, 81, 96,
                          97, 112, 113, 128, 129, 160, 161, 192, 193, 224, 225, 256};
    int *error_number = __crunch___errno_location();
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        int before_maps = maps, before_unmaps = unmaps;
        *error_number = EDOM;
        unsigned char *a = _crunch_malloc(sizes[i]);
        unsigned char *b = _crunch_malloc(sizes[i]);
        if (!a || !b || a == b || (uintptr_t)a % 16 || (uintptr_t)b % 16 ||
            maps != before_maps + 1 || *error_number != EDOM) return 1;
        if (sizes[i]) {
            a[0] = 17;
            a[sizes[i] - 1] = 42;
            b[0] = 99;
        }
        _crunch_free(a);
        if (unmaps != before_unmaps) return 2;
        unsigned char *c = _crunch_malloc(sizes[i]);
        if (c != a || (sizes[i] && b[0] != 99)) return 3;
        _crunch_free(c);
        _crunch_free(b);
        if (unmaps != before_unmaps + 1 || *error_number != EDOM) return 4;
    }
    return 0;
}

static int exhaustion(void)
{
    int before_maps = maps, before_unmaps = unmaps;
    for (int i = 0; i < 1400; i++) {
        blocks[i] = _crunch_malloc(64);
        if (!blocks[i]) return 1;
        blocks[i][0] = (unsigned char)(i % 251);
    }
    if (maps != before_maps + 3) return 2;
    for (int i = 0; i < 682; i++) {
        if (blocks[i][0] != (unsigned char)(i % 251)) return 3;
        _crunch_free(blocks[i]);
    }
    if (unmaps != before_unmaps + 1) return 4;
    for (int i = 682; i < 1400; i++) {
        if (blocks[i][0] != (unsigned char)(i % 251)) return 5;
        _crunch_free(blocks[i]);
    }
    if (unmaps != before_unmaps + 3) return 6;
    return 0;
}

static int growth(void)
{
    int *error_number = __crunch___errno_location();
    unsigned char *p = _crunch_malloc(16);
    if (!p) return 1;
    for (int i = 0; i < 16; i++) p[i] = (unsigned char)(i + 1);
    fail_map = 1;
    *error_number = EDOM;
    if (_crunch_realloc(p, 32) || *error_number != ENOMEM ||
        p[0] != 1 || p[15] != 16) return 9;
    fail_map = 0;
    *error_number = EDOM;
    unsigned char *q = _crunch_realloc(p, 32);
    if (!q) return 2;
    for (int i = 0; i < 16; i++) if (q[i] != i + 1) return 3;
    if (_crunch_realloc(q, 31) != q || q[15] != 16 ||
        _crunch_realloc(q, 32) != q) return 10;
    q[31] = 71;
    unsigned char *r = _crunch_realloc(q, 256);
    if (!r || r[0] != 1 || r[15] != 16 || r[31] != 71) return 4;
    r[255] = 72;
    fail_map = 1;
    *error_number = EDOM;
    if (_crunch_realloc(r, 257) || *error_number != ENOMEM ||
        r[0] != 1 || r[31] != 71 || r[255] != 72) return 5;
    fail_map = 0;
    *error_number = EDOM;
    unsigned char *large = _crunch_realloc(r, 257);
    if (!large || large[0] != 1 || large[31] != 71 || large[255] != 72 ||
        *error_number != EDOM) return 6;
    unsigned char *shrunk = _crunch_realloc(large, 16);
    if (shrunk != large || shrunk[0] != 1 || *error_number != EDOM) return 7;
    _crunch_free(shrunk);
    return *error_number == EDOM ? 0 : 8;
}

static int failed_unmap(void)
{
    int *error_number = __crunch___errno_location();
    int before_maps = maps, before_unmaps = unmaps;
    unsigned char *p = _crunch_malloc(16);
    if (!p) return 1;
    fail_unmap = 1;
    _crunch_free(p);
    if (*error_number != EINVAL || maps != before_maps + 1 ||
        unmaps != before_unmaps) return 2;
    fail_unmap = 0;
    p = _crunch_malloc(16);
    if (!p || maps != before_maps + 1) return 3;
    _crunch_free(p);
    return unmaps == before_unmaps + 1 ? 0 : 4;
}

static int mixed_operations(void)
{
    unsigned char *live[64] = {0};
    long sizes[64] = {0};
    unsigned char values[64] = {0};
    uint32_t random = 1234567;
    for (int step = 0; step < 5000; step++) {
        random = random * 1664525u + 1013904223u;
        unsigned slot = (random >> 16) % 64;
        for (unsigned i = 0; i < 64; i++)
            if (live[i] && (live[i][0] != values[i] ||
                            live[i][sizes[i] - 1] != values[i])) return 1;
        if (live[slot] && (random & 3u) == 0) {
            _crunch_free(live[slot]);
            live[slot] = NULL;
            continue;
        }
        random = random * 1664525u + 1013904223u;
        long size = (long)((random >> 8) % 6000) + 1;
        int was_live = live[slot] != NULL;
        unsigned char *p = was_live ? _crunch_realloc(live[slot], size)
                                    : _crunch_malloc(size);
        if (!p) return 2;
        long preserved = size < sizes[slot] ? size : sizes[slot];
        if (was_live)
            for (long i = 0; i < preserved; i++)
                if (p[i] != values[slot]) return 3;
        values[slot] = (unsigned char)(step % 251);
        for (long i = 0; i < size; i++) p[i] = values[slot];
        live[slot] = p;
        sizes[slot] = size;
    }
    for (unsigned i = 0; i < 64; i++) _crunch_free(live[i]);
    return 0;
}

int main(void)
{
    int status = classes();
    if (status) return 10 + status;
    status = exhaustion();
    if (status) return 20 + status;
    status = growth();
    if (status) return 30 + status;
    status = failed_unmap();
    if (status) return 40 + status;
    status = mixed_operations();
    if (status) return 50 + status;
    return maps == unmaps ? 0 : 60;
}
