#include <assert.h>
#include <string.h>

struct crunch_file { int descriptor; int flags; int status; int allocated; };
extern char *__crunch_fgets(char *, int, struct crunch_file *);
extern int __crunch_fputs(const char *, struct crunch_file *);

static int error_number, calls, fail_after;
static const char *input;
static char output[16];
static int written;

int *_crunch_errno_location(void) { return &error_number; }
long __crunch_read(int fd, void *buffer, long count) {
    assert(fd == 7 && count == 1);
    if (calls++ == fail_after) { error_number = 5; return -1; }
    if (!*input) return 0;
    *(char *)buffer = *input++;
    return 1;
}
long __crunch_write(int fd, const void *buffer, long count) {
    assert(fd == 7 && count > 0);
    if (calls++ == fail_after) { error_number = 5; return -1; }
    long n = count > 2 ? 2 : count;
    memcpy(output + written, buffer, (size_t)n);
    written += (int)n;
    return n;
}

int main(void) {
    struct crunch_file f = {7, 2, 0, 0};
    char buffer[8] = {0};
    input = "abc";
    fail_after = 2;
    assert(__crunch_fgets(buffer, sizeof buffer, &f) == 0 &&
           error_number == 5 && f.status == 2 && calls == 3);
    f.status = 2;
    calls = 0;
    fail_after = -1;
    input = "z";
    assert(__crunch_fgets(buffer, sizeof buffer, &f) == buffer &&
           strcmp(buffer, "z") == 0 && f.status == 3);
    f.status = 0;
    calls = 0;
    fail_after = 1;
    written = 0;
    assert(__crunch_fputs("abcd", &f) == -1 && error_number == 5 &&
           f.status == 2 && written == 2 && memcmp(output, "ab", 2) == 0);
    f.status = 0;
    calls = 0;
    fail_after = -1;
    written = 0;
    assert(__crunch_fputs("abcde", &f) >= 0 && calls == 3 &&
           written == 5 && memcmp(output, "abcde", 5) == 0);
    assert(__crunch_fputs(NULL, &f) == -1 && error_number == 22 && f.status == 2);
    return 0;
}
