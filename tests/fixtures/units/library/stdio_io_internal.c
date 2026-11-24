#include <assert.h>
#include <stddef.h>
#include <string.h>

struct crunch_file { int descriptor; int flags; int status; int allocated; };
extern size_t __crunch_fread(void *, size_t, size_t, struct crunch_file *);
extern size_t __crunch_fwrite(const void *, size_t, size_t, struct crunch_file *);
extern int __crunch_fseek(struct crunch_file *, long, int);
extern long __crunch_ftell(struct crunch_file *);
extern void __crunch_rewind(struct crunch_file *);

static int error_number, calls, seek_fail, last_whence;
static long outcomes[4], last_offset;
static long offsets[4], counts[4];
static unsigned outcome_count;
static char *base;

int *_crunch_errno_location(void) { return &error_number; }
static long transfer(int descriptor, const void *buffer, long count) {
    assert(descriptor == 7 && count > 0);
    assert((const char *)buffer >= base);
    assert((unsigned)calls < outcome_count);
    offsets[calls] = (const char *)buffer - base;
    counts[calls] = count;
    long result = outcomes[calls++];
    if (result < 0) error_number = 4;
    return result;
}
long __crunch_read(int fd, void *buffer, long count) {
    return transfer(fd, buffer, count);
}
long __crunch_write(int fd, const void *buffer, long count) {
    return transfer(fd, buffer, count);
}
long __crunch_lseek(int fd, long offset, int whence) {
    assert(fd == 7 && (whence == 0 || whence == 1));
    last_offset = offset;
    last_whence = whence;
    if (seek_fail) { error_number = 29; return -1; }
    return 123;
}

int main(void) {
    char buffer[16] = {0};
    struct crunch_file f = {7, 2, 0, 1};
    base = buffer;
    outcomes[0] = 4; outcomes[1] = 3; outcomes[2] = 0;
    outcome_count = 3;
    assert(__crunch_fread(buffer, 3, 3, &f) == 2 && calls == 3 && f.status == 1);
    assert(offsets[0] == 0 && offsets[1] == 4 && offsets[2] == 7 &&
           counts[0] == 9 && counts[1] == 5 && counts[2] == 2);
    assert(__crunch_fread(buffer, 1, 1, &f) == 0 && calls == 3);
    seek_fail = 1;
    assert(__crunch_fseek(&f, -2, 1) == -1 && error_number == 29 && f.status == 1);
    seek_fail = 0;
    f.status |= 2;
    assert(__crunch_fseek(&f, -2, 1) == 0 && last_offset == -2 && f.status == 2);
    calls = 0;
    outcomes[0] = 4; outcomes[1] = -1; outcome_count = 2;
    f.status = 0;
    assert(__crunch_fwrite(buffer, 3, 3, &f) == 1 && calls == 2 &&
           error_number == 4 && f.status == 2);
    assert(offsets[0] == 0 && offsets[1] == 4 && counts[0] == 9 && counts[1] == 5);
    calls = 0;
    outcomes[0] = 0; outcome_count = 1;
    f.status = 0;
    assert(__crunch_fwrite(buffer, 1, 1, &f) == 0 && error_number == 5 && f.status == 2);
    calls = 0;
    f.status = 0;
    assert(__crunch_fread(NULL, 0, 1, &f) == 0 && calls == 0 && f.status == 0);
    assert(__crunch_fread(NULL, 0, (size_t)-1, &f) == 0 && calls == 0 && f.status == 0);
    assert(__crunch_fread(NULL, 1, 1, &f) == 0 && error_number == 22 && f.status == 2);
    f.status = 0;
    assert(__crunch_fread(buffer, 0x7fffffffffffffffL, 2, &f) == 0 &&
           error_number == 75 && f.status == 2);
    f.status = 0;
    assert(__crunch_fread(buffer, (size_t)-1, 2, &f) == 0 &&
           error_number == 75 && f.status == 2 && calls == 0);
    calls = 0;
    outcomes[0] = -1; outcome_count = 1;
    f.status = 0;
    assert(__crunch_fread(buffer, 1, 0x80000000L, &f) == 0 &&
           counts[0] == 0x7ffff000L && error_number == 4 && f.status == 2);
    f.status = 3;
    error_number = 34;
    assert(__crunch_ftell(&f) == 123 && last_offset == 0 && last_whence == 1 &&
           f.status == 3 && error_number == 34);
    seek_fail = 1;
    assert(__crunch_ftell(&f) == -1 && error_number == 29 && f.status == 3);
    __crunch_rewind(&f);
    assert(last_offset == 0 && last_whence == 0 && f.status == 0 && error_number == 29);
    seek_fail = 0;
    f.status = 3;
    error_number = 34;
    __crunch_rewind(&f);
    assert(last_offset == 0 && last_whence == 0 && f.status == 0 && error_number == 34);
    return 0;
}
