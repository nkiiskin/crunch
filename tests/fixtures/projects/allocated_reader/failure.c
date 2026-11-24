#include <stddef.h>

extern int copy_file(const char *path, int output);
extern void *__real___crunch_malloc(size_t size);
extern void *__real___crunch_realloc(void *ptr, size_t size);
extern void __real___crunch_free(void *ptr);
extern int __real___crunch_close(int fd);

static int fail_malloc;
static int fail_realloc_at;
static int realloc_calls;
static int frees;
static int closes;
static int wrong_pointer;
static int close_failed;
static void *allocation;

void *__wrap___crunch_malloc(size_t size) {
    if (fail_malloc) return NULL;
    allocation = __real___crunch_malloc(size);
    return allocation;
}

void *__wrap___crunch_realloc(void *ptr, size_t size) {
    realloc_calls++;
    if (realloc_calls == fail_realloc_at) return NULL;
    void *result = __real___crunch_realloc(ptr, size);
    if (result) allocation = result;
    return result;
}

void __wrap___crunch_free(void *ptr) {
    if (ptr != allocation) wrong_pointer = 1;
    frees++;
    __real___crunch_free(ptr);
}

int __wrap___crunch_close(int fd) {
    closes++;
    int result = __real___crunch_close(fd);
    if (result != 0) close_failed = 1;
    return result;
}

int main(int argc, char **argv) {
    if (argc != 3) return 10;

    fail_malloc = 1;
    if (copy_file(argv[1], -1) != 3 || closes != 1 || frees != 0 ||
        close_failed) return 11;

    fail_malloc = 0;
    fail_realloc_at = 1;
    closes = 0;
    if (copy_file(argv[1], -1) != 3 || closes != 1 || frees != 1 ||
        realloc_calls != 1 || wrong_pointer || close_failed) return 12;

    fail_realloc_at = 2;
    realloc_calls = 0;
    closes = 0;
    frees = 0;
    if (copy_file(argv[1], -1) != 3 || closes != 1 || frees != 1 ||
        realloc_calls != 2 || wrong_pointer || close_failed) return 13;

    fail_realloc_at = 0;
    closes = 0;
    frees = 0;
    if (copy_file(argv[2], -1) != 4 || closes != 1 || frees != 1 ||
        wrong_pointer || close_failed) return 14;

    closes = 0;
    frees = 0;
    if (copy_file(argv[1], -1) != 6 || closes != 1 || frees != 1 ||
        wrong_pointer || close_failed) return 15;
    return 0;
}
