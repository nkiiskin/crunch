#include <stdint.h>
#include <string.h>

struct entry {
    long inode;
    long offset;
    const unsigned char *name;
    long name_length;
    int kind;
};

extern int __crunch_dirent64_next(const unsigned char *, long, long *, struct entry *);
extern int *_crunch_errno_location(void);

int main(void)
{
    unsigned char buf[64] = {0};
    struct entry e, before;
    long pos = 0;
    uint64_t ino = 123, off = 456;
    memcpy(buf, &ino, 8);
    memcpy(buf + 8, &off, 8);
    buf[16] = 24;
    buf[18] = 8;
    memcpy(buf + 19, "name", 5);
    *_crunch_errno_location() = 34;
    if (__crunch_dirent64_next(buf, 24, &pos, &e) != 1 || pos != 24 ||
        e.inode != 123 || e.offset != 456 || e.name != buf + 19 ||
        e.name_length != 4 || e.kind != 8 || *_crunch_errno_location() != 34)
        return 1;
    if (__crunch_dirent64_next(buf, 24, &pos, &e) != 0 ||
        *_crunch_errno_location() != 34)
        return 2;
    before = e;
    pos = 0;
    buf[16] = 19;
    if (__crunch_dirent64_next(buf, 24, &pos, &e) != -1 ||
        *_crunch_errno_location() != 22 || pos != 0 || before.inode != e.inode ||
        before.offset != e.offset || before.name != e.name ||
        before.name_length != e.name_length || before.kind != e.kind)
        return 3;
    buf[16] = 25;
    if (__crunch_dirent64_next(buf, 24, &pos, &e) != -1 || pos != 0)
        return 4;
    buf[16] = 24;
    buf[23] = 'x';
    if (__crunch_dirent64_next(buf, 24, &pos, &e) != -1 || pos != 0)
        return 5;
    if (__crunch_dirent64_next(buf, 19, &pos, &e) != -1 ||
        __crunch_dirent64_next(buf, -1, &pos, &e) != -1 ||
        __crunch_dirent64_next(0, 24, &pos, &e) != -1 ||
        __crunch_dirent64_next(buf, 24, 0, &e) != -1 ||
        __crunch_dirent64_next(buf, 24, &pos, 0) != -1)
        return 6;
    pos = 25;
    if (__crunch_dirent64_next(buf, 24, &pos, &e) != -1 || pos != 25)
        return 7;
    pos = -1;
    if (__crunch_dirent64_next(buf, 24, &pos, &e) != -1 || pos != -1)
        return 8;
    return 0;
}
