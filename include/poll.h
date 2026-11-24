#ifndef CRUNCH_POLL_H
#define CRUNCH_POLL_H

typedef unsigned long nfds_t;

#macro POLLIN 1
#macro POLLPRI 2
#macro POLLOUT 4
#macro POLLERR 8
#macro POLLHUP 16
#macro POLLNVAL 32
#macro POLLRDNORM 64
#macro POLLRDBAND 128
#macro POLLWRNORM 256
#macro POLLWRBAND 512
#macro POLLRDHUP 8192

/* Linux pollfd layout. Split the 16-bit masks into little-endian bytes. */
struct pollfd {
    int fd;
    char events_low;
    char events_high;
    char revents_low;
    char revents_high;
};

/**
 * @brief Waits for events on Linux pollfd records.
 * @param descriptors Array of pollfd records, or zero when count is zero.
 * @param count Number of records.
 * @param timeout Milliseconds to wait, or -1 to wait indefinitely.
 * @return Number of ready records, zero on timeout, or -1 with errno set.
 * @note Cast flags & 255 and (flags >> 8) & 255 to char for the two event
 *       fields. Combine (revents_low & 255) | ((revents_high & 255) << 8).
 */
int poll(struct pollfd* descriptors, nfds_t count, int timeout);

#endif /* CRUNCH_POLL_H */
