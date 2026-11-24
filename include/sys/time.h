#ifndef CRUNCH_SYS_TIME_H
#define CRUNCH_SYS_TIME_H

#include <sys/select.h>

#macro ITIMER_REAL 0
#macro ITIMER_VIRTUAL 1
#macro ITIMER_PROF 2

struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

/**
 * @brief Reads the system's wall-clock time.
 * @param value Receives seconds and microseconds since the epoch.
 * @param zone Obsolete Linux timezone output, or NULL.
 * @return Zero, or -1 with errno set.
 * @note This invokes the Linux syscall directly; a non-NULL zone is obsolete.
 */
int gettimeofday(struct timeval* value, void* zone);
/**
 * @brief Sets wall clock time or the obsolete kernel timezone; typically requires CAP_SYS_TIME.
 * @param value Wall-clock time, or NULL.
 * @param zone Obsolete timezone, or NULL.
 * @return Returns the syscall result, or -1 with errno set.
 */
int settimeofday(const struct timeval* value, const struct timezone* zone);

/**
 * @brief Reads an interval timer.
 * @param which ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF.
 * @param value Writable timer state.
 * @return Zero, or -1 with errno set.
 */
int getitimer(int which, struct itimerval* value);

/**
 * @brief Changes an interval timer.
 * @param which ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF.
 * @param value New timer state.
 * @param previous Receives the old state, or NULL.
 * @return Zero, or -1 with errno set.
 */
int setitimer(int which, const struct itimerval* value, struct itimerval* previous);

#endif
