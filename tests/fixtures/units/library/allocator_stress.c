#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

extern void *_crunch_malloc(size_t);
extern void *_crunch_realloc(void *, size_t);
extern void _crunch_free(void *);
extern int *__crunch___errno_location(void);
extern void *__real__crunch_mmap(void *, long, int, int, int, long);
extern int __real__crunch_munmap(void *, long);
extern void *__real__crunch_mremap(void *, long, long, int, void *);
extern void _crunch_cache_rollback_mapping(void *);
extern int __crunch_allocator_trim(void);

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

enum { MAX_MAPS = 256, BLOCKS = 2800, ARENA_64_BLOCKS = 682 };
static struct {
    uintptr_t address;
    long length;
    int active;
} mappings[MAX_MAPS];
static unsigned char *blocks[BLOCKS];
static unsigned char *small_blocks[BLOCKS];
static uintptr_t addresses[BLOCKS];
static int map_calls, unmap_calls, remap_calls, live_maps, bad_mapping;
static int fail_map, fail_unmap, fail_remap;
static int rollback_unmap_error;
static int fail_unmap_at_first, fail_unmap_at_second;

static int find_mapping(uintptr_t address, long length)
{
    for (int i = 0; i < MAX_MAPS; i++)
        if (mappings[i].active && mappings[i].address == address &&
            mappings[i].length == length) return i;
    return -1;
}

void *__wrap__crunch_mmap(void *address, long length, int prot, int flags, int fd, long offset)
{
    map_calls++;
    if (fail_map) {
        *__crunch___errno_location() = ENOMEM;
        return (void *)(intptr_t)-1;
    }
    void *result = __real__crunch_mmap(address, length, prot, flags, fd, offset);
    if (result != (void *)(intptr_t)-1) {
        int i;
        for (i = 0; i < MAX_MAPS && mappings[i].active; i++) {}
        if (i == MAX_MAPS) bad_mapping = 1;
        else {
            mappings[i].address = (uintptr_t)result;
            mappings[i].length = length;
            mappings[i].active = 1;
            live_maps++;
        }
    }
    return result;
}

int __wrap__crunch_munmap(void *address, long length)
{
    unmap_calls++;
    int i = find_mapping((uintptr_t)address, length);
    if (i < 0) bad_mapping = 1;
    if (unmap_calls == fail_unmap_at_first || unmap_calls == fail_unmap_at_second) {
        *__crunch___errno_location() =
            unmap_calls == fail_unmap_at_first ? EACCES : EINVAL;
        return -1;
    }
    if (fail_unmap) {
        fail_unmap--;
        *__crunch___errno_location() =
            fail_unmap == 0 && rollback_unmap_error ? rollback_unmap_error : EINVAL;
        return -1;
    }
    int result = __real__crunch_munmap(address, length);
    if (result == 0 && i >= 0) {
        mappings[i].active = 0;
        live_maps--;
    }
    return result;
}

void *__wrap__crunch_mremap(void *address, long old_length, long new_length,
                            int flags, void *new_address)
{
    remap_calls++;
    int i = find_mapping((uintptr_t)address, old_length);
    if (i < 0) bad_mapping = 1;
    if (fail_remap) {
        *__crunch___errno_location() = ENOMEM;
        return (void *)(intptr_t)-1;
    }
    void *result = __real__crunch_mremap(address, old_length, new_length, flags, new_address);
    if (result != (void *)(intptr_t)-1 && i >= 0) {
        mappings[i].address = (uintptr_t)result;
        mappings[i].length = new_length;
    }
    return result;
}

static unsigned char pattern(int block, long offset)
{
    return (unsigned char)((block * 37 + offset * 13) % 251);
}

static void fill(unsigned char *p, long size, int block)
{
    for (long i = 0; i < size; i++) p[i] = pattern(block, i);
}

static int matches(const unsigned char *p, long size, int block)
{
    for (long i = 0; i < size; i++)
        if (p[i] != pattern(block, i)) return 0;
    return 1;
}

static int compare_addresses(const void *left, const void *right)
{
    uintptr_t a = *(const uintptr_t *)left, b = *(const uintptr_t *)right;
    return (a > b) - (a < b);
}

static int distinct_blocks(long size)
{
    for (int i = 0; i < BLOCKS; i++) addresses[i] = (uintptr_t)blocks[i];
    qsort(addresses, BLOCKS, sizeof addresses[0], compare_addresses);
    for (int i = 1; i < BLOCKS; i++)
        if (addresses[i] - addresses[i - 1] < (uintptr_t)size) return 0;
    return 1;
}

static int class_pressure(void)
{
    const long capacities[] = {16, 32, 48, 64, 80, 96, 112, 128,
                               160, 192, 224, 256};
    for (size_t c = 0; c < sizeof capacities / sizeof capacities[0]; c++) {
        long size = capacities[c];
        int before = map_calls;
        for (int i = 0; i < BLOCKS; i++) {
            blocks[i] = _crunch_malloc(size);
            CHECK(blocks[i] && (uintptr_t)blocks[i] % 16 == 0);
            fill(blocks[i], size, i);
        }
        int per_arena = (int)(65472 / (size + 32));
        CHECK(map_calls - before == (BLOCKS + per_arena - 1) / per_arena);
        CHECK(distinct_blocks(size));
        for (int i = 0; i < BLOCKS; i++) CHECK(matches(blocks[i], size, i));
        for (int i = 1; i < BLOCKS; i += 2) {
            _crunch_free(blocks[i]);
            blocks[i] = NULL;
        }
        fail_map = 1;
        before = map_calls;
        for (int i = 1; i < BLOCKS; i += 2) {
            blocks[i] = _crunch_malloc(size);
            CHECK(blocks[i]);
            fill(blocks[i], size, i);
        }
        CHECK(map_calls == before);
        fail_map = 0;
        CHECK(distinct_blocks(size));
        for (int i = 0; i < BLOCKS; i++) CHECK(matches(blocks[i], size, i));
        for (int i = BLOCKS - 1; i >= 0; i--) _crunch_free(blocks[i]);
        CHECK(live_maps == 0 && !bad_mapping);
    }
    return 0;
}

static int arena_lists(void)
{
    const int count = 3 * ARENA_64_BLOCKS + 1;
    for (int i = 0; i < count; i++) {
        blocks[i] = _crunch_malloc(64);
        CHECK(blocks[i]);
        fill(blocks[i], 64, i);
    }
    CHECK(live_maps == 4);
    for (int arena = 0; arena < 3; arena++) {
        int first = arena * ARENA_64_BLOCKS;
        _crunch_free(blocks[first]);
        blocks[first] = NULL;
    }
    /* Empty the middle non-head arena, but fail the unmap once. */
    fail_unmap = 1;
    for (int i = ARENA_64_BLOCKS + 1; i < 2 * ARENA_64_BLOCKS; i++)
        _crunch_free(blocks[i]);
    CHECK(live_maps == 4 && *__crunch___errno_location() == EINVAL);
    fail_map = 1;
    unsigned char *reuse = _crunch_malloc(64);
    CHECK(reuse && live_maps == 4);
    fail_map = 0;
    _crunch_free(reuse);
    CHECK(live_maps == 3);
    for (int i = 1; i < ARENA_64_BLOCKS; i++) {
        CHECK(matches(blocks[i], 64, i));
        _crunch_free(blocks[i]);
    }
    for (int i = 2 * ARENA_64_BLOCKS + 1; i < count; i++) {
        CHECK(matches(blocks[i], 64, i));
        _crunch_free(blocks[i]);
    }
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int growth_pressure(void)
{
    const long sizes[] = {1, 16, 17, 32, 33, 48, 49, 64, 65, 80, 81, 96,
                          97, 112, 113, 128, 129, 160, 161, 192, 193, 224,
                          225, 256, 257, 4096, 8192};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        unsigned char *p = _crunch_malloc(sizes[i]);
        CHECK(p);
        fill(p, sizes[i], (int)i);
        for (size_t j = i + 1; j < sizeof sizes / sizeof sizes[0]; j++) {
            long old_size = sizes[j - 1];
            unsigned char *next = _crunch_realloc(p, sizes[j]);
            CHECK(next && (uintptr_t)next % 16 == 0 && matches(next, old_size, (int)i));
            fill(next, sizes[j], (int)i);
            p = next;
        }
        unsigned char *smaller = _crunch_realloc(p, 1);
        CHECK(smaller && matches(smaller, 1, (int)i));
        CHECK(_crunch_realloc(smaller, 0) == NULL);
        CHECK(live_maps == 0 && !bad_mapping);
    }
    return 0;
}

static int failure_pressure(void)
{
    int *error_number = __crunch___errno_location();
    for (int i = 0; i < ARENA_64_BLOCKS; i++) {
        blocks[i] = _crunch_malloc(64);
        CHECK(blocks[i]);
        fill(blocks[i], 64, i);
    }
    CHECK(live_maps == 1);
    fail_map = 1;
    *error_number = EDOM;
    CHECK(!_crunch_malloc(64) && *error_number == ENOMEM && live_maps == 1);
    CHECK(!_crunch_realloc(blocks[1], 128) && *error_number == ENOMEM);
    for (int i = 0; i < ARENA_64_BLOCKS; i++) CHECK(matches(blocks[i], 64, i));
    _crunch_free(blocks[0]);
    int before = map_calls;
    blocks[0] = _crunch_malloc(64);
    CHECK(blocks[0] && map_calls == before);
    fill(blocks[0], 64, 0);
    fail_map = 0;
    unsigned char *extra = _crunch_malloc(64);
    CHECK(extra && live_maps == 2);
    _crunch_free(extra);
    unsigned char *large = _crunch_malloc(4096);
    CHECK(large);
    fill(large, 4096, 17);
    fail_remap = 1;
    *error_number = EDOM;
    CHECK(!_crunch_realloc(large, 16384) && *error_number == ENOMEM &&
          matches(large, 4096, 17) && remap_calls == 1);
    fail_remap = 0;
    unsigned char *grown = _crunch_realloc(large, 16384);
    CHECK(grown && matches(grown, 4096, 17) && remap_calls == 2);
    _crunch_free(grown);
    for (int i = 0; i < ARENA_64_BLOCKS; i++) {
        CHECK(matches(blocks[i], 64, i));
        _crunch_free(blocks[i]);
    }
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int resource_limit(void)
{
    struct rlimit limit;
    CHECK(getrlimit(RLIMIT_AS, &limit) == 0);
    if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > 64 * 1024 * 1024)
        limit.rlim_cur = 64 * 1024 * 1024;
    CHECK(setrlimit(RLIMIT_AS, &limit) == 0);
    int count = 0;
    for (; count < MAX_MAPS; count++) {
        blocks[count] = _crunch_malloc(256 * 1024);
        if (!blocks[count]) break;
        blocks[count][0] = (unsigned char)(count % 251);
        blocks[count][256 * 1024 - 1] = (unsigned char)((count + 1) % 251);
    }
    CHECK(count > 8 && count < MAX_MAPS && *__crunch___errno_location() == ENOMEM);
    CHECK(live_maps == count && !bad_mapping);
    for (int i = 0; i < count; i++)
        CHECK(blocks[i][0] == (unsigned char)(i % 251) &&
              blocks[i][256 * 1024 - 1] == (unsigned char)((i + 1) % 251));
    int small_count = 0;
    for (; small_count < BLOCKS; small_count++) {
        small_blocks[small_count] = _crunch_malloc(64);
        if (!small_blocks[small_count]) break;
        small_blocks[small_count][0] = (unsigned char)(small_count % 251);
    }
    CHECK(small_count < BLOCKS && *__crunch___errno_location() == ENOMEM &&
          !bad_mapping);
    for (int i = 0; i < small_count; i++) {
        CHECK(small_blocks[i][0] == (unsigned char)(i % 251));
        _crunch_free(small_blocks[i]);
    }
    for (int i = 0; i < 8; i++) _crunch_free(blocks[i]);
    unsigned char *recovered = _crunch_malloc(256 * 1024);
    CHECK(recovered);
    recovered[0] = 42;
    _crunch_free(recovered);
    for (int i = 8; i < count; i++) _crunch_free(blocks[i]);
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int realloc_reclaim_one(long new_size, int failures)
{
    int *error_number = __crunch___errno_location();
    unsigned char *p = _crunch_malloc(16);
    CHECK(p);
    fill(p, 16, 73);
    fail_unmap = failures;
    rollback_unmap_error = failures == 2 ? EBUSY : 0;
    *error_number = EDOM;
    CHECK(!_crunch_realloc(p, new_size));
    CHECK(*error_number == EINVAL && matches(p, 16, 73));
    CHECK(live_maps == failures && fail_unmap == 0);
    rollback_unmap_error = 0;
    if (failures == 2) {
        int before = map_calls;
        fail_map = 1;
        unsigned char *reuse = _crunch_malloc(new_size);
        CHECK(reuse && map_calls == before && matches(p, 16, 73));
        fail_map = 0;
        _crunch_free(reuse);
    }
    unsigned char *grown = _crunch_realloc(p, new_size);
    CHECK(grown && matches(grown, 16, 73));
    _crunch_free(grown);
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int realloc_reclaim_failure(void)
{
    CHECK(realloc_reclaim_one(32, 1) == 0);
    CHECK(realloc_reclaim_one(32, 2) == 0);
    CHECK(realloc_reclaim_one(4096, 1) == 0);
    CHECK(realloc_reclaim_one(4096, 2) == 0);
    unsigned char *p = _crunch_malloc(16);
    unsigned char *peer = _crunch_malloc(16);
    CHECK(p && peer);
    fill(p, 16, 74);
    fail_unmap = 1;
    *__crunch___errno_location() = EDOM;
    unsigned char *grown = _crunch_realloc(p, 32);
    CHECK(grown && matches(grown, 16, 74) &&
          *__crunch___errno_location() == EDOM && fail_unmap == 1);
    fail_unmap = 0;
    _crunch_free(peer);
    _crunch_free(grown);
    CHECK(live_maps == 0 && !bad_mapping);

    for (int i = 0; i < 2 * ARENA_64_BLOCKS; i++) {
        blocks[i] = _crunch_malloc(64);
        CHECK(blocks[i]);
        fill(blocks[i], 64, i);
    }
    _crunch_free(blocks[1]);
    _crunch_free(blocks[ARENA_64_BLOCKS]);
    for (int i = 2; i < ARENA_64_BLOCKS; i++) _crunch_free(blocks[i]);
    fail_unmap = 1;
    CHECK(!_crunch_realloc(blocks[0], 128) && fail_unmap == 0 &&
          *__crunch___errno_location() == EINVAL && matches(blocks[0], 64, 0));
    fail_map = 1;
    unsigned char *available = _crunch_malloc(64);
    CHECK(available && matches(blocks[0], 64, 0));
    fail_map = 0;
    _crunch_free(available);
    _crunch_free(blocks[0]);
    for (int i = ARENA_64_BLOCKS + 1; i < 2 * ARENA_64_BLOCKS; i++) {
        CHECK(matches(blocks[i], 64, i));
        _crunch_free(blocks[i]);
    }
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int rollback_cache_repeated(void)
{
    const long sizes[] = {4096, 8192, 16384};
    int *error_number = __crunch___errno_location();
    unsigned char *original = _crunch_malloc(16);
    CHECK(original);
    fill(original, 16, 85);
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        int before = map_calls;
        fail_unmap = 2;
        rollback_unmap_error = EBUSY;
        *error_number = EDOM;
        CHECK(!_crunch_realloc(original, sizes[i]) && *error_number == EINVAL);
        CHECK(fail_unmap == 0 && map_calls == before + 1 &&
              matches(original, 16, 85) && live_maps == (int)i + 2);
    }
    rollback_unmap_error = 0;
    fail_map = 1;
    int before = map_calls;
    CHECK(!_crunch_malloc(32768) && *error_number == ENOMEM &&
          map_calls == before + 1 && live_maps == 4);
    before = map_calls;
    CHECK(!_crunch_malloc(32) && *error_number == ENOMEM &&
          map_calls == before + 1 && live_maps == 4);
    unsigned char *held[3];
    for (int i = 0; i < 3; i++) {
        before = map_calls;
        *error_number = EDOM;
        held[i] = _crunch_malloc(sizes[2 - i]);
        CHECK(held[i] && (uintptr_t)held[i] % 16 == 0 &&
              map_calls == before && *error_number == EDOM &&
              matches(original, 16, 85));
        fill(held[i], sizes[2 - i], i + 90);
    }
    for (int i = 0; i < 3; i++) CHECK(matches(held[i], sizes[2 - i], i + 90));
    before = map_calls;
    CHECK(!_crunch_malloc(257) && map_calls == before + 1 && *error_number == ENOMEM);
    fail_map = 0;
    for (int i = 0; i < 3; i++) _crunch_free(held[i]);
    _crunch_free(original);
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int rollback_cache_list(void)
{
    int *error_number = __crunch___errno_location();
    unsigned char *small = _crunch_malloc(4096);
    unsigned char *middle = _crunch_malloc(16384);
    unsigned char *large = _crunch_malloc(8192);
    CHECK(small && middle && large);
    uintptr_t small_address = (uintptr_t)small, middle_address = (uintptr_t)middle;
    uintptr_t large_address = (uintptr_t)large;
    _crunch_cache_rollback_mapping(large);
    _crunch_cache_rollback_mapping(middle);
    _crunch_cache_rollback_mapping(small);
    fail_map = 1;
    int before = map_calls;
    *error_number = EDOM;
    CHECK(!_crunch_malloc(32768) && *error_number == ENOMEM &&
          map_calls == before + 1 && live_maps == 3);
    before = map_calls;
    *error_number = EDOM;
    unsigned char *taken_middle = _crunch_malloc(16000);
    CHECK(taken_middle && (uintptr_t)taken_middle == middle_address &&
          map_calls == before && *error_number == EDOM);
    fill(taken_middle, 16000, 101);
    unsigned char *taken_tail = _crunch_malloc(8192);
    CHECK(taken_tail && (uintptr_t)taken_tail == large_address && map_calls == before);
    fill(taken_tail, 8192, 102);
    unsigned char *taken_head = _crunch_malloc(4096);
    CHECK(taken_head && (uintptr_t)taken_head == small_address && map_calls == before);
    fill(taken_head, 4096, 103);
    CHECK(matches(taken_middle, 16000, 101) && matches(taken_tail, 8192, 102) &&
          matches(taken_head, 4096, 103));
    CHECK(!_crunch_malloc(257) && *error_number == ENOMEM);
    fail_map = 0;
    before = remap_calls;
    CHECK(_crunch_realloc(taken_middle, 16384) == taken_middle &&
          remap_calls == before && matches(taken_middle, 16000, 101));
    unsigned char *resized = _crunch_realloc(taken_middle, 32768);
    CHECK(resized && matches(resized, 16000, 101));
    _crunch_free(resized);
    _crunch_free(taken_tail);
    _crunch_free(taken_head);
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int trim_retained(void)
{
    int *error_number = __crunch___errno_location();
    int before = unmap_calls;
    *error_number = EDOM;
    CHECK(__crunch_allocator_trim() == 0 && *error_number == EDOM &&
          unmap_calls == before);

    unsigned char *live = _crunch_malloc(512);
    CHECK(live);
    fill(live, 512, 111);
    unsigned char *cached[3];
    for (int i = 0; i < 3; i++) {
        cached[i] = _crunch_malloc(4096L << i);
        CHECK(cached[i]);
        _crunch_cache_rollback_mapping(cached[i]);
    }
    for (int i = 0; i < 1365; i++) {
        blocks[i] = _crunch_malloc(16);
        CHECK(blocks[i]);
    }
    fill(blocks[1], 16, 112);
    _crunch_free(blocks[0]);
    for (int i = 2; i < 1364; i++) _crunch_free(blocks[i]);
    fail_unmap = 1;
    _crunch_free(blocks[1364]);
    CHECK(fail_unmap == 0);
    unsigned char *empty = _crunch_malloc(32);
    CHECK(empty);
    fail_unmap = 1;
    _crunch_free(empty);
    CHECK(fail_unmap == 0 && live_maps == 7);

    before = unmap_calls;
    fail_unmap_at_first = before + 2;
    fail_unmap_at_second = before + 4;
    *error_number = EDOM;
    CHECK(__crunch_allocator_trim() == -1 && *error_number == EACCES &&
          unmap_calls == before + 5 && live_maps == 4 &&
          matches(live, 512, 111) && matches(blocks[1], 16, 112));
    fail_unmap_at_first = fail_unmap_at_second = 0;
    int maps_before = map_calls;
    fail_map = 1;
    unsigned char *reused = _crunch_malloc(8192);
    CHECK(reused && map_calls == maps_before);
    fail_map = 0;
    _crunch_free(reused);
    before = unmap_calls;
    *error_number = EDOM;
    CHECK(__crunch_allocator_trim() == 0 && *error_number == EDOM &&
          unmap_calls == before + 1 && live_maps == 2 &&
          matches(live, 512, 111) && matches(blocks[1], 16, 112));
    before = unmap_calls;
    CHECK(__crunch_allocator_trim() == 0 && unmap_calls == before);
    _crunch_free(blocks[1]);
    _crunch_free(live);
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

static int trim_cache_edges(void)
{
    unsigned char *p[3];
    for (int i = 0; i < 3; i++) {
        p[i] = _crunch_malloc(4096L << i);
        CHECK(p[i]);
        _crunch_cache_rollback_mapping(p[i]);
    }
    int before = unmap_calls;
    fail_unmap_at_first = before + 1;
    fail_unmap_at_second = before + 3;
    CHECK(__crunch_allocator_trim() == -1 && *__crunch___errno_location() == EACCES &&
          unmap_calls == before + 3 && live_maps == 2);
    fail_unmap_at_first = fail_unmap_at_second = 0;
    fail_map = 1;
    int maps_before = map_calls;
    unsigned char *reused = _crunch_malloc(8192);
    CHECK(reused && map_calls == maps_before);
    fail_map = 0;
    _crunch_free(reused);
    before = unmap_calls;
    CHECK(__crunch_allocator_trim() == 0 && unmap_calls == before + 1 &&
          live_maps == 0 && !bad_mapping);
    return 0;
}

static int trim_after_rollback(void)
{
    unsigned char *original = _crunch_malloc(16);
    CHECK(original);
    fill(original, 16, 113);
    for (long size = 4096; size <= 16384; size *= 2) {
        fail_unmap = 2;
        rollback_unmap_error = EBUSY;
        CHECK(!_crunch_realloc(original, size) &&
              *__crunch___errno_location() == EINVAL && fail_unmap == 0 &&
              matches(original, 16, 113));
    }
    rollback_unmap_error = 0;
    CHECK(live_maps == 4);
    int before = unmap_calls;
    *__crunch___errno_location() = EDOM;
    CHECK(__crunch_allocator_trim() == 0 && *__crunch___errno_location() == EDOM &&
          unmap_calls == before + 3 && live_maps == 1 &&
          matches(original, 16, 113));
    _crunch_free(original);
    CHECK(live_maps == 0 && !bad_mapping);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    if (!strcmp(argv[1], "classes")) return class_pressure();
    if (!strcmp(argv[1], "arena-lists")) return arena_lists();
    if (!strcmp(argv[1], "growth")) return growth_pressure();
    if (!strcmp(argv[1], "failures")) return failure_pressure();
    if (!strcmp(argv[1], "resource-limit")) return resource_limit();
    if (!strcmp(argv[1], "realloc-reclaim-failure")) return realloc_reclaim_failure();
    if (!strcmp(argv[1], "rollback-cache-repeated")) return rollback_cache_repeated();
    if (!strcmp(argv[1], "rollback-cache-list")) return rollback_cache_list();
    if (!strcmp(argv[1], "trim-retained")) return trim_retained();
    if (!strcmp(argv[1], "trim-cache-edges")) return trim_cache_edges();
    if (!strcmp(argv[1], "trim-after-rollback")) return trim_after_rollback();
    fprintf(stderr, "unknown stress case: %s\n", argv[1]);
    return 2;
}
