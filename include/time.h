#ifndef CRUNCH_TIME_H
#define CRUNCH_TIME_H

#include <sys/types.h>

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
/**
 * @brief Suspends execution for a relative interval.
 * @param requested Requested interval.
 * @param remaining Receives the remaining interval on interruption, or NULL.
 * @return Zero, or -1 with errno set.
 */
int nanosleep(const struct timespec* requested, struct timespec* remaining);
/**
 * @brief Returns seconds since the Unix epoch and optionally stores the result.
 * @param result Optional output location.
 * @return The syscall result, or -1 with errno set.
 */
time_t time(time_t* result);

#endif
