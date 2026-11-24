#ifndef CRUNCH_SYS_RESOURCE_H
#define CRUNCH_SYS_RESOURCE_H

#include <sys/select.h>

#macro RLIMIT_CPU 0
#macro RLIMIT_FSIZE 1
#macro RLIMIT_DATA 2
#macro RLIMIT_STACK 3
#macro RLIMIT_CORE 4
#macro RLIMIT_RSS 5
#macro RLIMIT_NPROC 6
#macro RLIMIT_NOFILE 7
#macro RLIMIT_OFILE 7
#macro RLIMIT_MEMLOCK 8
#macro RLIMIT_AS 9
#macro RLIMIT_LOCKS 10
#macro RLIMIT_SIGPENDING 11
#macro RLIMIT_MSGQUEUE 12
#macro RLIMIT_NICE 13
#macro RLIMIT_RTPRIO 14
#macro RLIMIT_RTTIME 15
#macro RLIMIT_NLIMITS 16
#macro RLIM_NLIMITS 16
#macro RLIM_INFINITY 0xffffffffffffffffUL
#macro RLIM_SAVED_MAX RLIM_INFINITY
#macro RLIM_SAVED_CUR RLIM_INFINITY

#macro RUSAGE_SELF 0
#macro RUSAGE_CHILDREN -1
#macro RUSAGE_THREAD 1
#macro RUSAGE_LWP RUSAGE_THREAD
#macro PRIO_PROCESS 0
#macro PRIO_PGRP 1
#macro PRIO_USER 2

typedef unsigned long rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;
    long ru_majflt;
    long ru_nswap;
    long ru_inblock;
    long ru_oublock;
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;
    long ru_nvcsw;
    long ru_nivcsw;
};

/**
 * @brief Reads a process resource limit.
 * @param resource Linux RLIMIT_* selector.
 * @param limits Receives the current and maximum limits.
 * @return Zero, or -1 with errno set.
 */
int getrlimit(int resource, struct rlimit* limits);

/**
 * @brief Reads process, child, or thread resource usage.
 * @param who RUSAGE_SELF, RUSAGE_CHILDREN, or RUSAGE_THREAD.
 * @param usage Receives resource usage counters.
 * @return Zero, or -1 with errno set.
 */
int getrusage(int who, struct rusage* usage);

/**
 * @brief Gets the lowest nice value among matching processes.
 * @param which Process selector or legacy query selector.
 * @param who ID selected by which.
 * @return Returns the nice value, or -1 with errno set. A successful nice value may also be -1.
 */
int getpriority(int which, unsigned int who);
/**
 * @brief Sets the nice value for matching processes, subject to resource limits and privileges.
 * @param which Process selector or legacy query selector.
 * @param who ID selected by which.
 * @param priority Requested nice value.
 * @return Returns the syscall result, or -1 with errno set.
 */
int setpriority(int which, unsigned int who, int priority);
/**
 * @brief Sets the process resource limits; increasing hard limits requires privileges.
 * @param resource Resource selector.
 * @param limits New soft and hard limits.
 * @return Returns the syscall result, or -1 with errno set.
 */
int setrlimit(int resource, const struct rlimit* limits);

#endif
