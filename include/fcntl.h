#ifndef CRUNCH_FCNTL_H
#define CRUNCH_FCNTL_H

#include <sys/types.h>

#macro O_RDONLY 0
#macro O_WRONLY 1
#macro O_RDWR 2
#macro O_CREAT 64
#macro O_EXCL 128
#macro O_TRUNC 512
#macro O_APPEND 1024
#macro O_NONBLOCK 2048
#macro O_DIRECTORY 65536
#macro O_NOFOLLOW 131072
#macro O_CLOEXEC 524288
#macro F_DUPFD 0
#macro F_GETFD 1
#macro F_SETFD 2
#macro F_GETFL 3
#macro F_SETFL 4
#macro F_GETLK 5
#macro F_SETLK 6
#macro F_SETLKW 7
#macro F_DUPFD_CLOEXEC 1030
#macro FD_CLOEXEC 1
#macro F_RDLCK 0
#macro F_WRLCK 1
#macro F_UNLCK 2
#macro AT_FDCWD -100
#macro AT_SYMLINK_NOFOLLOW 256
#macro AT_REMOVEDIR 512
#macro AT_EMPTY_PATH 4096

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

/**
 * @brief Opens a file with Linux flags and creation permissions.
 * @param path NUL-terminated path.
 * @param flags Linux O_* flag combination.
 * @param mode Creation permissions, modified by the process umask.
 * @return Descriptor on success, or -1 with errno set.
 */
int open(const char* path, int flags, mode_t mode);

/**
 * @brief Creates or truncates a file for writing.
 * @param path NUL-terminated path.
 * @param mode Creation permissions, modified by the process umask.
 * @return Descriptor, or -1 with errno set.
 */
int creat(const char* path, mode_t mode);

/**
 * @brief Opens a path relative to a directory descriptor, or AT_FDCWD.
 * @param directory Open directory descriptor or AT_FDCWD.
 * @param path NUL-terminated path; an absolute path ignores directory.
 * @param flags Linux O_* flag combination.
 * @param mode Creation permissions, modified by the process umask.
 * @return Descriptor on success, or -1 with errno set.
 */
int openat(int directory, const char* path, int flags, mode_t mode);

/**
 * @brief Performs a file control operation with an integer argument.
 * @param descriptor Open file descriptor.
 * @param command Linux F_* command.
 * @param argument Integer argument, or zero when unused.
 * @return Command result, or -1 with errno set.
 */
int fcntl(int descriptor, int command, int argument);

/**
 * @brief Performs a file control operation with a pointer argument.
 * @param descriptor Open file descriptor.
 * @param command Linux F_* command.
 * @param argument Command-specific pointer.
 * @return Command result, or -1 with errno set.
 */
int fcntl_ptr(int descriptor, int command, void* argument);

#endif /* CRUNCH_FCNTL_H */
