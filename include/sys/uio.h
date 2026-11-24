#ifndef CRUNCH_SYS_UIO_H
#define CRUNCH_SYS_UIO_H
#include <stddef.h>
#include <sys/types.h>
struct iovec {
    void* iov_base;
    size_t iov_len;
};

/**
 * @brief Reads into an array of buffers.
 * @param descriptor Open file descriptor.
 * @param vectors Array of writable buffer descriptions.
 * @param count Number of entries in vectors.
 * @return Bytes read, zero at EOF, or -1 with errno set.
 */
ssize_t readv(int descriptor, const struct iovec* vectors, int count);

/**
 * @brief Writes from an array of buffers.
 * @param descriptor Open file descriptor.
 * @param vectors Array of buffer descriptions.
 * @param count Number of entries in vectors.
 * @return Bytes written, or -1 with errno set.
 */
ssize_t writev(int descriptor, const struct iovec* vectors, int count);
#endif
