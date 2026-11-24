#ifndef CRUNCH_SYS_SELECT_H
#define CRUNCH_SYS_SELECT_H

#include <sys/types.h>

#ifndef NULL
#macro NULL 0
#endif

#macro FD_SETSIZE 1024

typedef struct {
    unsigned long bits[16];
} fd_set;

/**
 * @brief Clears a descriptor set.
 * @param set Writable descriptor set.
 * @return Nothing.
 */
static inline void crunch_fd_zero(fd_set* set) {
    for (int i = 0; i < 16; i = i + 1) set->bits[i] = 0;
}

#macro FD_ZERO(set) crunch_fd_zero(set)
#macro FD_SET(fd, set) ((set)->bits[(fd) / 64] |= 1UL << ((fd) % 64))
#macro FD_CLR(fd, set) ((set)->bits[(fd) / 64] &= ~(1UL << ((fd) % 64)))
#macro FD_ISSET(fd, set) (((set)->bits[(fd) / 64] & (1UL << ((fd) % 64))) != 0)

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

/**
 * @brief Waits for descriptor readiness using Linux select.
 * @param count Highest descriptor of interest plus one.
 * @param readfds Read bitmap, or zero.
 * @param writefds Write bitmap, or zero.
 * @param exceptfds Exceptional-condition bitmap, or zero.
 * @param timeout Writable timeout, or zero for no timeout.
 * @return Number of ready bits, zero on timeout, or -1 with errno set.
 * @note Linux updates all supplied sets and may modify timeout, including
 *       on interruption. FD_* macros require descriptors below FD_SETSIZE.
 */
int select(int count, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);

#endif /* CRUNCH_SYS_SELECT_H */
