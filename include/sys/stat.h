#ifndef CRUNCH_SYS_STAT_H
#define CRUNCH_SYS_STAT_H

#include <sys/types.h>

#macro S_ISREG(mode) (((mode) & 61440) == 32768)

/* Linux x86-64 kernel stat layout; integer fields use Crunch's signed types. */
struct stat {
    long st_dev;
    long st_ino;
    long st_nlink;
    int st_mode;
    int st_uid;
    int st_gid;
    int _padding;
    long st_rdev;
    long st_size;
    long st_blksize;
    long st_blocks;
    long st_atime;
    long st_atime_nsec;
    long st_mtime;
    long st_mtime_nsec;
    long st_ctime;
    long st_ctime_nsec;
    long _reserved0;
    long _reserved1;
    long _reserved2;
};

/**
 * @brief Gets metadata for a directory-relative path.
 * @param directory Open directory descriptor or AT_FDCWD.
 * @param path NUL-terminated path.
 * @param buffer Writable stat structure.
 * @param flags Linux fstatat flags, or zero.
 * @return Zero on success, or -1 with errno set.
 */
int fstatat(int directory, const char* path, struct stat* buffer, int flags);

/**
 * @brief Creates a directory relative to a directory descriptor.
 * @param directory Open directory descriptor or AT_FDCWD.
 * @param path NUL-terminated path.
 * @param mode Creation permissions, modified by the process umask.
 * @return Zero on success, or -1 with errno set.
 */
int mkdirat(int directory, const char* path, mode_t mode);

/**
 * @brief Gets file metadata in the Linux x86-64 stat layout.
 * @param path NUL-terminated path.
 * @param buffer Writable stat structure.
 * @return Zero on success, or -1 with errno set.
 */
int stat(const char* path, struct stat* buffer);

/**
 * @brief Gets metadata for an open file descriptor.
 * @param descriptor Open file descriptor.
 * @param buffer Writable stat structure.
 * @return Zero on success, or -1 with errno set.
 */
int fstat(int descriptor, struct stat* buffer);

/**
 * @brief Gets metadata for a path without following its final symbolic link.
 * @param path NUL-terminated path.
 * @param buffer Writable stat structure.
 * @return Zero on success, or -1 with errno set.
 */
int lstat(const char* path, struct stat* buffer);

/**
 * @brief Creates a directory.
 * @param path NUL-terminated path.
 * @param mode Creation permissions, modified by the process umask.
 * @return Zero on success, or -1 with errno set.
 */
int mkdir(const char* path, mode_t mode);

/**
 * @brief Changes the permissions of a file.
 * @param path NUL-terminated path.
 * @param mode New Linux permission bits.
 * @return Zero on success, or -1 with errno set.
 */
int chmod(const char* path, mode_t mode);

/**
 * @brief Changes the permissions of an open file.
 * @param descriptor Open descriptor.
 * @param mode New Linux permission bits.
 * @return Zero on success, or -1 with errno set.
 */
int fchmod(int descriptor, mode_t mode);
/**
 * @brief Creates a filesystem node.
 * @param path NUL-terminated path.
 * @param mode Node type and permissions.
 * @param device Raw Linux encoded device number.
 * @return The result, or -1 with errno set.
 */
int mknod(const char* path, mode_t mode, unsigned int device);

/**
 * @brief Sets the process file creation mask.
 * @param mask New creation mask.
 * @return Previous mask.
 */
mode_t umask(mode_t mask);

#endif /* CRUNCH_SYS_STAT_H */
