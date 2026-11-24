#ifndef CRUNCH_SYS_EPOLL_H
#define CRUNCH_SYS_EPOLL_H

/**
 * @brief Creates an epoll instance.
 * @param size Buffer or bitmap size in bytes.
 * @return The syscall result, or -1 with errno set.
 */
int epoll_create(int size);

#endif
