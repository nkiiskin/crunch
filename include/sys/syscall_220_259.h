#ifndef CRUNCH_SYS_SYSCALL_220_259_H
#define CRUNCH_SYS_SYSCALL_220_259_H

#include <sys/types.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

struct itimerspec { struct timespec it_interval; struct timespec it_value; };
struct crunch_mq_attr { long mq_flags; long mq_maxmsg; long mq_msgsize; long mq_curmsgs; long __reserved[4]; };
union crunch_syscall_word { void* pointer; unsigned long value; };

#macro CLOCK_REALTIME 0
#macro CLOCK_MONOTONIC 1
#macro TIMER_ABSTIME 1
#macro EPOLL_CTL_ADD 1
#macro EPOLL_CTL_DEL 2
#macro EPOLL_CTL_MOD 3
#macro EPOLLIN 1
#macro EPOLLOUT 4
#macro P_ALL 0
#macro P_PID 1
#macro P_PGID 2
#macro WEXITED 4
#macro WNOWAIT 16777216
#macro IOPRIO_WHO_PROCESS 1
#macro IN_MODIFY 2
#macro IN_CREATE 256

/**
 * @brief Performs an atomic semaphore operation with an optional relative timeout.
 * @param id Semaphore set ID.
 * @param operations Operation array.
 * @param count Number of operations.
 * @param timeout Relative timeout or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int semtimedop(int id, struct sembuf* operations, size_t count, const struct timespec* timeout);

/**
 * @brief Invokes the raw fadvise64 syscall; unlike posix_fadvise, errors return -1 and set errno.
 * @param descriptor Open descriptor.
 * @param offset Starting byte.
 * @param length Advised length.
 * @param advice Linux POSIX_FADV advice.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_fadvise64(int descriptor, off_t offset, off_t length, int advice);

/**
 * @brief Creates a kernel POSIX timer. The event uses the Linux kernel sigevent layout, not a libc object; NULL requests SIGALRM.
 * @param clock Clock ID.
 * @param event Kernel sigevent or NULL.
 * @param timer Receives kernel timer ID.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_timer_create(int clock, const void* event, int* timer);

/**
 * @brief Sets a kernel POSIX timer.
 * @param timer Kernel timer ID.
 * @param flags Timer flags.
 * @param value New timer state.
 * @param previous Previous state or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_timer_settime(int timer, int flags, const struct itimerspec* value, struct itimerspec* previous);

/**
 * @brief Reads a kernel POSIX timer.
 * @param timer Kernel timer ID.
 * @param value Current timer state.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_timer_gettime(int timer, struct itimerspec* value);

/**
 * @brief Returns the timer overrun count.
 * @param timer Kernel timer ID.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_timer_getoverrun(int timer);

/**
 * @brief Deletes a kernel POSIX timer.
 * @param timer Kernel timer ID.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_timer_delete(int timer);

/**
 * @brief Sets a clock; privileges may be required.
 * @param clock Clock ID.
 * @param value New clock time.
 * @return The syscall result, or -1 with errno set.
 */
int clock_settime(int clock, const struct timespec* value);

/**
 * @brief Reads a clock using the Linux syscall.
 * @param clock Clock ID.
 * @param value Receives clock time.
 * @return The syscall result, or -1 with errno set.
 */
int clock_gettime(int clock, struct timespec* value);

/**
 * @brief Reads clock resolution.
 * @param clock Clock ID.
 * @param value Receives resolution or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int clock_getres(int clock, struct timespec* value);

/**
 * @brief Invokes the raw clock_nanosleep syscall; unlike libc clock_nanosleep, errors return -1 and set errno.
 * @param clock Clock ID.
 * @param flags TIMER_ABSTIME or zero.
 * @param requested Requested time.
 * @param remaining Remaining time or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_clock_nanosleep(int clock, int flags, const struct timespec* requested, struct timespec* remaining);

/**
 * @brief Terminates all threads in the process without running finalizers.
 * @param status Process exit status.
 * @return Does not return.
 */
void noreturn crunch_exit_group(int status);

/**
 * @brief Waits for events. Each Linux x86-64 kernel event is 12 bytes: a 32-bit event mask followed by eight data bytes; the data is unaligned.
 * @param epoll Epoll descriptor.
 * @param events Writable array of 12-byte Linux epoll events.
 * @param maximum Maximum events.
 * @param timeout Timeout in milliseconds.
 * @return The syscall result, or -1 with errno set.
 */
int epoll_wait(int epoll, void* events, int maximum, int timeout);

/**
 * @brief Controls an epoll instance. The event uses the packed 12-byte Linux x86-64 kernel layout.
 * @param epoll Epoll descriptor.
 * @param operation EPOLL_CTL operation.
 * @param descriptor Target descriptor.
 * @param event 12-byte Linux epoll event or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int epoll_ctl(int epoll, int operation, int descriptor, const void* event);

/**
 * @brief Sends a signal to a specific thread in a thread group.
 * @param process Thread group ID.
 * @param thread Thread ID.
 * @param signal Signal number.
 * @return The syscall result, or -1 with errno set.
 */
int tgkill(pid_t process, pid_t thread, int signal);

/**
 * @brief Sets file access and modification times.
 * @param path Path.
 * @param times Two timestamps or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int utimes(const char* path, const struct timeval* times);

/**
 * @brief Invokes the obsolete vserver syscall slot, unimplemented on Linux x86-64; policy may return a different error.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_vserver(void);

/**
 * @brief Sets NUMA policy for a memory range; kernel support and permissions vary.
 * @param address Memory address.
 * @param length Length in bytes.
 * @param mode Memory policy.
 * @param nodes Node bitmap or NULL.
 * @param maxnode Bitmap size in bits.
 * @param flags Linux policy flags.
 * @return The syscall result, or -1 with errno set.
 */
long mbind(void* address, unsigned long length, int mode, const unsigned long* nodes, unsigned long maxnode, unsigned int flags);

/**
 * @brief Sets the calling thread's NUMA policy.
 * @param mode Memory policy.
 * @param nodes Node bitmap or NULL.
 * @param maxnode Bitmap size in bits.
 * @return The syscall result, or -1 with errno set.
 */
long set_mempolicy(int mode, const unsigned long* nodes, unsigned long maxnode);

/**
 * @brief Queries the calling thread's NUMA policy.
 * @param mode Receives policy or NULL.
 * @param nodes Receives node bitmap or NULL.
 * @param maxnode Bitmap size in bits.
 * @param address Optional queried address.
 * @param flags Linux policy flags.
 * @return The syscall result, or -1 with errno set.
 */
long get_mempolicy(int* mode, unsigned long* nodes, unsigned long maxnode, void* address, unsigned long flags);

/**
 * @brief Invokes raw mq_open. Unlike the POSIX libc wrapper, the kernel queue name must omit its leading slash.
 * @param name Kernel queue name without the leading slash.
 * @param flags Open flags.
 * @param mode Creation permissions.
 * @param attributes Creation attributes or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_mq_open(const char* name, int flags, mode_t mode, const struct crunch_mq_attr* attributes);

/**
 * @brief Unlinks a queue using the raw kernel name, without a leading slash.
 * @param name Kernel queue name without the leading slash.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_mq_unlink(const char* name);

/**
 * @brief Sends a POSIX message using an optional absolute timeout.
 * @param queue Queue descriptor.
 * @param message Message bytes.
 * @param length Message length.
 * @param priority Message priority.
 * @param timeout Absolute timeout or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_mq_timedsend(int queue, const char* message, size_t length, unsigned int priority, const struct timespec* timeout);

/**
 * @brief Receives a POSIX message using an optional absolute timeout.
 * @param queue Queue descriptor.
 * @param message Writable message buffer.
 * @param length Buffer size.
 * @param priority Receives priority or NULL.
 * @param timeout Absolute timeout or NULL.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t crunch_mq_timedreceive(int queue, char* message, size_t length, unsigned int* priority, const struct timespec* timeout);

/**
 * @brief Registers or removes notification. Non-NULL events use the Linux kernel sigevent layout.
 * @param queue Queue descriptor.
 * @param event Kernel sigevent or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_mq_notify(int queue, const void* event);

/**
 * @brief Queries or changes queue flags via the raw kernel ABI.
 * @param queue Queue descriptor.
 * @param attributes New flags or NULL.
 * @param previous Previous attributes or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_mq_getsetattr(int queue, const struct crunch_mq_attr* attributes, struct crunch_mq_attr* previous);

/**
 * @brief Loads a kernel image; this privileged operation can alter system boot state.
 * @param entry Entry address.
 * @param count Segment count.
 * @param segments Kernel segment array.
 * @param flags Linux kexec flags.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_kexec_load(unsigned long entry, unsigned long count, const void* segments, unsigned long flags);

/**
 * @brief Invokes the five-argument Linux waitid syscall, including its optional resource usage output.
 * @param type P_ALL, P_PID or P_PGID.
 * @param id Child selector.
 * @param information Receives child information.
 * @param options Wait flags.
 * @param usage Receives resource usage or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_waitid(int type, pid_t id, siginfo_t* information, int options, struct rusage* usage);

/**
 * @brief Adds a key to a Linux keyring.
 * @param type Key type.
 * @param description Key description.
 * @param payload Key payload.
 * @param length Payload size.
 * @param keyring Destination keyring ID.
 * @return The syscall result, or -1 with errno set.
 */
long add_key(const char* type, const char* description, const void* payload, size_t length, int keyring);

/**
 * @brief Requests a key from a Linux keyring.
 * @param type Key type.
 * @param description Key description.
 * @param callout Callout information or NULL.
 * @param keyring Destination keyring ID.
 * @return The syscall result, or -1 with errno set.
 */
long request_key(const char* type, const char* description, const char* callout, int keyring);

/**
 * @brief Invokes raw keyctl with machine-word arguments.
 * @param operation Key control operation.
 * @param arg2 Operation argument.
 * @param arg3 Operation argument.
 * @param arg4 Operation argument.
 * @param arg5 Operation argument.
 * @return The syscall result, or -1 with errno set.
 * @note Use union crunch_syscall_word to pass pointer-valued arguments.
 */
long crunch_keyctl(int operation, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);

/**
 * @brief Sets I/O scheduling priority.
 * @param which Selector kind.
 * @param who Selector ID.
 * @param priority Encoded I/O priority.
 * @return The syscall result, or -1 with errno set.
 */
int ioprio_set(int which, int who, int priority);

/**
 * @brief Gets encoded I/O scheduling priority.
 * @param which Selector kind.
 * @param who Selector ID.
 * @return The syscall result, or -1 with errno set.
 */
int ioprio_get(int which, int who);

/**
 * @brief Creates an inotify descriptor.
 * @return The syscall result, or -1 with errno set.
 */
int inotify_init(void);

/**
 * @brief Adds or updates an inotify watch.
 * @param descriptor Inotify descriptor.
 * @param path Watched path.
 * @param mask Event mask.
 * @return The syscall result, or -1 with errno set.
 */
int inotify_add_watch(int descriptor, const char* path, unsigned int mask);

/**
 * @brief Removes an inotify watch.
 * @param descriptor Inotify descriptor.
 * @param watch Watch descriptor.
 * @return The syscall result, or -1 with errno set.
 */
int inotify_rm_watch(int descriptor, int watch);

/**
 * @brief Migrates process pages between NUMA nodes; permissions may be required.
 * @param process Process ID.
 * @param maxnode Bitmap size in bits.
 * @param from Source node bitmap.
 * @param to Destination node bitmap.
 * @return The syscall result, or -1 with errno set.
 */
long migrate_pages(pid_t process, unsigned long maxnode, const unsigned long* from, const unsigned long* to);

/**
 * @brief Creates a special file relative to a directory. The device argument is the raw Linux kernel encoding, not libc dev_t.
 * @param directory Directory descriptor or AT_FDCWD.
 * @param path Path relative to directory.
 * @param mode File type and mode.
 * @param device Raw Linux 32-bit device number.
 * @return The syscall result, or -1 with errno set.
 */
int mknodat(int directory, const char* path, mode_t mode, unsigned int device);

#endif
