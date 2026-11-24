#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern long _crunch_open(const char *, int, int);
extern long _crunch_read(int, void *, long);
extern long _crunch_write(int, const void *, long);
extern long _crunch_close(int);
extern long _crunch_stat(const char *, struct stat *);
extern long _crunch_lseek(int, long, int);
extern long _crunch_fstat(int, struct stat *);
extern long _crunch_unlink(const char *);
extern long _crunch_rename(const char *, const char *);
extern long _crunch_mkdir(const char *, int);
extern long _crunch_rmdir(const char *);
extern long _crunch_openat(int, const char *, int, int);
extern long _crunch_lstat(const char *, struct stat *);
extern long _crunch_readlink(const char *, char *, long);
extern long _crunch_getdents64(int, void *, long);
extern long _crunch_fstatat(int, const char *, struct stat *, int);
extern long _crunch_readlinkat(int, const char *, char *, long);
extern long _crunch_unlinkat(int, const char *, int);
extern long _crunch_mkdirat(int, const char *, int);
extern long _crunch_renameat(int, const char *, int, const char *);
extern long _crunch_symlinkat(const char *, int, const char *);

int main(void)
{
    struct stat expected, actual;
    char buf[5];
    errno = EDOM;
    if (_crunch_open("missing", O_RDONLY, 0) != -ENOENT ||
        _crunch_read(-1, buf, 1) != -EBADF ||
        _crunch_write(-1, buf, 1) != -EBADF ||
        _crunch_close(-1) != -EBADF ||
        _crunch_stat("missing", &actual) != -ENOENT ||
        _crunch_lseek(-1, 0, SEEK_SET) != -EBADF ||
        _crunch_fstat(-1, &actual) != -EBADF ||
        _crunch_unlink("missing") != -ENOENT ||
        _crunch_rename("missing", "other") != -ENOENT ||
        _crunch_mkdir("missing/child", 0700) != -ENOENT ||
        _crunch_rmdir("missing") != -ENOENT ||
        _crunch_openat(-1, "missing", O_RDONLY, 0) != -EBADF ||
        _crunch_lstat("missing", &actual) != -ENOENT ||
        _crunch_readlink("missing", buf, sizeof buf) != -ENOENT ||
        _crunch_getdents64(-1, buf, sizeof buf) != -EBADF ||
        _crunch_fstatat(-1, "x", &actual, 0) != -EBADF ||
        _crunch_readlinkat(-1, "x", buf, sizeof buf) != -EBADF ||
        _crunch_unlinkat(-1, "x", 0) != -EBADF ||
        _crunch_mkdirat(-1, "x", 0700) != -EBADF ||
        _crunch_renameat(-1, "x", -1, "y") != -EBADF ||
        _crunch_symlinkat("x", -1, "y") != -EBADF || errno != EDOM)
        return 1;
    long fd = _crunch_open("data", O_RDONLY, 0);
    if (fd < 0 || _crunch_lseek((int)fd, 3, SEEK_SET) != 3 ||
        _crunch_lseek((int)fd, -1, SEEK_END) != 4 ||
        _crunch_lseek((int)fd, 0, SEEK_SET) != 0 ||
        _crunch_fstat((int)fd, &actual) != 0 || actual.st_size != 5 ||
        _crunch_read((int)fd, buf, 5) != 5 ||
        memcmp(buf, "hello", 5) || _crunch_close((int)fd) != 0)
        return 2;
    if (stat("data", &expected) || _crunch_stat("data", &actual) ||
        actual.st_dev != expected.st_dev || actual.st_ino != expected.st_ino ||
        actual.st_mode != expected.st_mode || actual.st_size != expected.st_size ||
        actual.st_nlink != expected.st_nlink || actual.st_blocks != expected.st_blocks)
        return 3;
    long directory = _crunch_open(".", O_RDONLY | 0200000, 0);
    if (directory < 0 || _crunch_getdents64((int)directory, &actual, sizeof actual) <= 0 ||
        _crunch_lstat("rawlink", &actual) || (actual.st_mode & 0170000) != 0120000 ||
        _crunch_readlink("rawlink", buf, sizeof buf) != 4 || memcmp(buf, "data", 4))
        return 5;
    if (_crunch_mkdirat((int)directory, "raw-at", 0700) ||
        _crunch_symlinkat("data", (int)directory, "raw-at-link") ||
        _crunch_fstatat((int)directory, "raw-at-link", &actual, 256) ||
        (actual.st_mode & 0170000) != 0120000 ||
        _crunch_readlinkat((int)directory, "raw-at-link", buf, 3) != 3 ||
        memcmp(buf, "dat", 3) ||
        _crunch_renameat((int)directory, "raw-at-link", (int)directory, "raw-at/link") ||
        _crunch_unlinkat((int)directory, "raw-at/link", 0) ||
        _crunch_unlinkat((int)directory, "raw-at", 512) || errno != EDOM)
        return 8;
    long created = _crunch_openat((int)directory, "raw-created", O_CREAT | O_EXCL | O_RDWR, 0600);
    if (created < 0 || _crunch_fstat((int)created, &actual) ||
        (actual.st_mode & 0777) != 0600)
        return 6;
    if (_crunch_close((int)created) || _crunch_unlink("raw-created") ||
        _crunch_close((int)directory))
        return 7;
    if (_crunch_mkdir("rawdir", 0700) || _crunch_rename("data", "rawdir/data") ||
        _crunch_unlink("rawdir/data") || _crunch_rmdir("rawdir") || errno != EDOM)
        return 4;
    return 0;
}
