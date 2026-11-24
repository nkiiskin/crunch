#ifndef CRUNCH_SYS_RANDOM_H
#define CRUNCH_SYS_RANDOM_H

#include <stddef.h>
#include <sys/types.h>

/**
 * @brief Reads random bytes from the Linux kernel.
 * @param buffer Writable destination.
 * @param count Maximum number of bytes.
 * @param flags Linux getrandom flags, or zero.
 * @return Number of bytes read, or -1 with errno set.
 */
ssize_t getrandom(void* buffer, size_t count, unsigned int flags);

#endif
