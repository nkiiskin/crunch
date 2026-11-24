#ifndef CRUNCH_SYS_TIMES_H
#define CRUNCH_SYS_TIMES_H

#include <sys/types.h>

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

/**
 * @brief Reads process and child CPU times in clock ticks; the return value is elapsed ticks from an arbitrary origin.
 * @param buffer Writable output buffer.
 * @return The result, or -1 with errno set.
 */
clock_t times(struct tms* buffer);
#endif
