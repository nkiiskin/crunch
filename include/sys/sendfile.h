#ifndef CRUNCH_SYS_SENDFILE_H
#define CRUNCH_SYS_SENDFILE_H

#include <sys/types.h>

/**
 * @brief Transfers data between descriptors using Linux sendfile.
 * @param output Destination descriptor.
 * @param input Source descriptor.
 * @param offset Optional source offset, updated after the transfer.
 * @param count Maximum bytes to transfer.
 * @return Bytes transferred, zero at EOF, or -1 with errno set.
 */
ssize_t sendfile(int output, int input, off_t* offset, size_t count);

#endif
