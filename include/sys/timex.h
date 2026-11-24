#ifndef CRUNCH_SYS_TIMEX_H
#define CRUNCH_SYS_TIMEX_H
#include <sys/time.h>

#macro ADJ_OFFSET 0x0001
#macro ADJ_FREQUENCY 0x0002
#macro ADJ_MAXERROR 0x0004
#macro ADJ_ESTERROR 0x0008
#macro ADJ_STATUS 0x0010
#macro ADJ_TIMECONST 0x0020
#macro ADJ_TAI 0x0080
#macro ADJ_SETOFFSET 0x0100
#macro ADJ_MICRO 0x1000
#macro ADJ_NANO 0x2000
#macro ADJ_TICK 0x4000

struct timex {
    unsigned int modes;
    long offset;
    long freq;
    long maxerror;
    long esterror;
    int status;
    long constant;
    long precision;
    long tolerance;
    struct timeval time;
    long tick;
    long ppsfreq;
    long jitter;
    int shift;
    long stabil;
    long jitcnt;
    long calcnt;
    long errcnt;
    long stbcnt;
    int tai;
    int reserved[11];
};
/**
 * @brief Queries or adjusts the system clock using the Linux x86-64 timex layout.
 * @param state Linux x86-64 timex structure.
 * @return Returns the syscall result, or -1 with errno set.
 */
int adjtimex(struct timex* state);
#endif
