#ifndef CRUNCH_SYS_VFS_H
#define CRUNCH_SYS_VFS_H

struct statfs {
    long f_type;
    long f_bsize;
    unsigned long f_blocks;
    unsigned long f_bfree;
    unsigned long f_bavail;
    unsigned long f_files;
    unsigned long f_ffree;
    int f_fsid[2];
    long f_namelen;
    long f_frsize;
    long f_flags;
    long f_spare[4];
};
/**
 * @brief Reads filesystem statistics into the Linux x86-64 kernel statfs layout.
 * @param path NUL-terminated path.
 * @param buffer Writable output buffer.
 * @return The result, or -1 with errno set.
 */
int statfs(const char* path, struct statfs* buffer);
/**
 * @brief Reads filesystem statistics for an open descriptor into the Linux x86-64 kernel statfs layout.
 * @param descriptor Open file descriptor.
 * @param buffer Writable output buffer.
 * @return The result, or -1 with errno set.
 */
int fstatfs(int descriptor, struct statfs* buffer);
#endif
