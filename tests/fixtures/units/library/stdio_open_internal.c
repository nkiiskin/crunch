#include <assert.h>
#include <stddef.h>
#include <string.h>

struct crunch_file { int descriptor; int flags; int status; int allocated; };
extern struct crunch_file *__crunch_fopen(const char *, const char *);
extern int __crunch_fclose(struct crunch_file *);

static struct crunch_file storage;
static int error_number, allocation_fails, open_fails, close_fails;
static int allocations, releases, opens, closes, flags_seen, mode_seen;

int *_crunch_errno_location(void) { return &error_number; }
void *__crunch_malloc(size_t size) {
    assert(size == sizeof storage);
    allocations++;
    if (allocation_fails) { error_number = 12; return NULL; }
    return &storage;
}
void __crunch_free(void *pointer) {
    assert(pointer == &storage);
    releases++;
    error_number = 5;
}
int __crunch_open(const char *path, int flags, int mode) {
    assert(strcmp(path, "path") == 0);
    opens++;
    flags_seen = flags;
    mode_seen = mode;
    if (open_fails) { error_number = 2; return -1; }
    return 17;
}
int __crunch_close(int descriptor) {
    assert(descriptor == 17);
    closes++;
    if (close_fails) { error_number = 9; return -1; }
    return 0;
}

int main(void) {
    static const struct { const char *mode; int flags; } cases[] = {
        {"r", 0}, {"rb", 0}, {"r+", 2}, {"rb+", 2}, {"r+b", 2},
        {"w", 577}, {"w+", 578}, {"wb+x", 706},
        {"a", 1089}, {"a+", 1090}, {"a+be", 525378},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        error_number = 34;
        struct crunch_file *stream = __crunch_fopen("path", cases[i].mode);
        assert(stream == &storage && storage.descriptor == 17);
        assert(storage.flags == cases[i].flags && storage.status == 0 && storage.allocated == 1 &&
               flags_seen == cases[i].flags);
        assert(mode_seen == 438 && error_number == 34);
        assert(__crunch_fclose(stream) == 0 && error_number == 34);
    }
    static const char *const invalid[] = {"", "q", "r++", "wbb", "rx", "weee", "a?"};
    int before = allocations;
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        assert(__crunch_fopen("path", invalid[i]) == NULL && error_number == 22);
    }
    assert(__crunch_fopen(NULL, "r") == NULL && error_number == 22);
    assert(__crunch_fopen("path", NULL) == NULL && error_number == 22);
    assert(allocations == before);
    allocation_fails = 1;
    before = opens;
    assert(__crunch_fopen("path", "w") == NULL && error_number == 12);
    assert(opens == before);
    allocation_fails = 0;
    open_fails = 1;
    before = releases;
    assert(__crunch_fopen("path", "r") == NULL && error_number == 2);
    assert(releases == before + 1);
    open_fails = 0;
    struct crunch_file *stream = __crunch_fopen("path", "r");
    close_fails = 1;
    before = releases;
    assert(__crunch_fclose(stream) == -1 && error_number == 9);
    assert(releases == before + 1);
    storage.allocated = 0;
    before = releases;
    assert(__crunch_fclose(&storage) == -1 && error_number == 9 && releases == before);
    close_fails = 0;
    error_number = 34;
    assert(__crunch_fclose(&storage) == 0 && error_number == 34 && releases == before);
    before = closes;
    assert(__crunch_fclose(NULL) == -1 && error_number == 22 && closes == before);
    return 0;
}
