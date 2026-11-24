#ifndef CRUNCH_DIRENT_H
#define CRUNCH_DIRENT_H

#include <stddef.h>
#include <sys/types.h>

#macro DT_UNKNOWN 0
#macro DT_FIFO 1
#macro DT_CHR 2
#macro DT_DIR 4
#macro DT_BLK 6
#macro DT_REG 8
#macro DT_LNK 10
#macro DT_SOCK 12

/**
 * @brief Reads Linux x86-64 directory records into a byte buffer.
 * @param descriptor Open directory descriptor.
 * @param buffer Writable destination.
 * @param byte_count Buffer size, from zero through 4294967295.
 * @return Bytes read, zero at end of directory, or -1 with errno set.
 */
ssize_t getdents64(int descriptor, void* buffer, size_t byte_count);

/* A decoded directory entry; name points into the caller's getdents64 buffer. */
struct dirent64 {
    long inode;
    long offset;
    const char* name;
    long name_length;
    int kind;
};

/**
 * @brief Decodes the next record in a getdents64 buffer.
 * @param buffer Bytes returned by getdents64.
 * @param byte_count Number of returned bytes.
 * @param position In/out byte offset, initially zero for each buffer.
 * @param entry Writable decoded entry; its name points into buffer.
 * @return One for an entry, zero at the end, or -1 with errno set to EINVAL.
 */
int dirent64_next(const char* buffer, long byte_count, long* position, struct dirent64* entry);


#endif /* CRUNCH_DIRENT_H */
