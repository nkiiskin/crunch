#ifndef CRUNCH_SCHED_H
#define CRUNCH_SCHED_H
#include <sys/types.h>
#include <time.h>

#macro SCHED_OTHER 0
#macro SCHED_FIFO 1
#macro SCHED_RR 2
#macro SCHED_BATCH 3
#macro SCHED_IDLE 5
#macro SCHED_RESET_ON_FORK 1073741824

struct sched_param {
    int sched_priority;
};

#macro CSIGNAL 255
#macro CLONE_VM 256
#macro CLONE_FS 512
#macro CLONE_FILES 1024
#macro CLONE_SIGHAND 2048
#macro CLONE_PIDFD 4096
#macro CLONE_PTRACE 8192
#macro CLONE_VFORK 16384
#macro CLONE_PARENT 32768
#macro CLONE_THREAD 65536
#macro CLONE_NEWNS 131072
#macro CLONE_SYSVSEM 262144
#macro CLONE_SETTLS 524288
#macro CLONE_PARENT_SETTID 1048576
#macro CLONE_CHILD_CLEARTID 2097152
#macro CLONE_DETACHED 4194304
#macro CLONE_UNTRACED 8388608
#macro CLONE_CHILD_SETTID 16777216
#macro CLONE_NEWCGROUP 33554432
#macro CLONE_NEWUTS 67108864
#macro CLONE_NEWIPC 134217728
#macro CLONE_NEWUSER 268435456
#macro CLONE_NEWPID 536870912
#macro CLONE_NEWNET 1073741824
#macro CLONE_IO 2147483648U

typedef int (*clone_function_t)(void*);

/**
 * @brief Yields the calling thread's processor.
 * @return Zero, or -1 with errno set.
 */
int sched_yield(void);
/**
 * @brief Sets scheduling parameters for a thread; process zero selects the caller.
 * @param process Thread ID, or zero for the caller.
 * @param parameter Scheduling parameter structure.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sched_setparam(pid_t process, const struct sched_param* parameter);
/**
 * @brief Reads scheduling parameters for a thread; process zero selects the caller.
 * @param process Thread ID, or zero for the caller.
 * @param parameter Scheduling parameter structure.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sched_getparam(pid_t process, struct sched_param* parameter);
/**
 * @brief Sets scheduling policy and parameters; elevated policies can require privileges.
 * @param process Thread ID, or zero for the caller.
 * @param policy Linux scheduling policy.
 * @param parameter Scheduling parameter structure.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sched_setscheduler(pid_t process, int policy, const struct sched_param* parameter);
/**
 * @brief Reads a thread's scheduling policy; process zero selects the caller.
 * @param process Thread ID, or zero for the caller.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sched_getscheduler(pid_t process);
/**
 * @brief Returns the maximum priority for the specified scheduling policy.
 * @param policy Linux scheduling policy.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sched_get_priority_max(int policy);
/**
 * @brief Returns the minimum priority for the specified scheduling policy.
 * @param policy Linux scheduling policy.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sched_get_priority_min(int policy);
/**
 * @brief Reads the round-robin scheduling quantum for a thread.
 * @param process Thread ID, or zero for the caller.
 * @param interval Receives the scheduling quantum.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sched_rr_get_interval(pid_t process, struct timespec* interval);

/**
 * @brief Creates a Linux child that invokes a function on its own stack.
 * @param function Child entry function.
 * @param stack Top of a writable downward-growing child stack.
 * @param flags Linux clone flags and exit signal.
 * @param argument Argument passed to the child function.
 * @return Child ID, or -1 with errno set.
 * @note This fixed-argument interface does not support optional TID or TLS arguments.
 */
int clone(clone_function_t function, void* stack, int flags, void* argument);

/**
 * @brief Sets a thread CPU affinity from a Linux CPU bitmap of size bytes; process zero selects the caller.
 * @param process Thread ID, or zero for the calling thread.
 * @param size Buffer or bitmap size in bytes.
 * @param mask CPU bitmap.
 * @return The syscall result, or -1 with errno set.
 */
int sched_setaffinity(pid_t process, size_t size, const void* mask);

/**
 * @brief Gets a thread CPU affinity into a Linux CPU bitmap of size bytes; process zero selects the caller.
 * @param process Thread ID, or zero for the calling thread.
 * @param size Buffer or bitmap size in bytes.
 * @param mask CPU bitmap.
 * @return The syscall result, or -1 with errno set.
 */
int sched_getaffinity(pid_t process, size_t size, void* mask);

#endif
