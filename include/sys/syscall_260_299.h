#ifndef CRUNCH_SYS_SYSCALL_260_299_H
#define CRUNCH_SYS_SYSCALL_260_299_H

#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/syscall_220_259.h>
#include <signal.h>
#include <poll.h>
#include <time.h>

struct crunch_pselect_sigmask { const sigset_t* mask; size_t size; };
struct crunch_mmsghdr { struct msghdr msg_hdr; unsigned int msg_len; };

#macro SPLICE_F_MOVE 1
#macro SPLICE_F_NONBLOCK 2
#macro SPLICE_F_MORE 4
#macro SYNC_FILE_RANGE_WAIT_BEFORE 1
#macro SYNC_FILE_RANGE_WRITE 2
#macro SYNC_FILE_RANGE_WAIT_AFTER 4
#macro TFD_NONBLOCK 2048
#macro TFD_CLOEXEC 524288
#macro TFD_TIMER_ABSTIME 1
#macro SFD_NONBLOCK 2048
#macro SFD_CLOEXEC 524288
#macro EFD_SEMAPHORE 1
#macro EFD_NONBLOCK 2048
#macro EFD_CLOEXEC 524288
#macro EPOLL_CLOEXEC 524288
#macro IN_NONBLOCK 2048
#macro IN_CLOEXEC 524288
#macro UTIME_NOW 1073741823
#macro UTIME_OMIT 1073741822

/**
 * @brief Changes file ownership relative to a directory.
 * @param directory Directory descriptor or AT_FDCWD.
 * @param path Relative or absolute path.
 * @param owner Owner ID or -1.
 * @param group Group ID or -1.
 * @param flags Linux AT flags.
 * @return The syscall result, or -1 with errno set.
 */
int fchownat(int directory, const char* path, uid_t owner, gid_t group, int flags);

/**
 * @brief Changes file timestamps relative to a directory.
 * @param directory Directory descriptor or AT_FDCWD.
 * @param path Relative or absolute path.
 * @param times Two timestamps or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int futimesat(int directory, const char* path, const struct timeval* times);

/**
 * @brief Creates a hard link relative to directory descriptors.
 * @param old_directory Source directory descriptor.
 * @param old_path Source path.
 * @param new_directory Destination directory descriptor.
 * @param new_path Destination path.
 * @param flags Linux AT flags.
 * @return The syscall result, or -1 with errno set.
 */
int linkat(int old_directory, const char* old_path, int new_directory, const char* new_path, int flags);

/**
 * @brief Invokes the original three-argument Linux fchmodat syscall; flags are not supported.
 * @param directory Directory descriptor or AT_FDCWD.
 * @param path Relative or absolute path.
 * @param mode File mode.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_fchmodat(int directory, const char* path, mode_t mode);

/**
 * @brief Invokes the original three-argument Linux faccessat syscall; flags are not supported.
 * @param directory Directory descriptor or AT_FDCWD.
 * @param path Relative or absolute path.
 * @param mode Access mode.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_faccessat(int directory, const char* path, int mode);

/**
 * @brief Invokes raw pselect6. The signals descriptor contains a pointer to an eight-byte Crunch sigset_t and its size, 8. The kernel may modify timeout, including on interruption.
 * @param count Highest descriptor plus one.
 * @param readfds Read set or NULL.
 * @param writefds Write set or NULL.
 * @param exceptfds Exception set or NULL.
 * @param timeout Writable timeout or NULL.
 * @param signals Kernel signal-mask descriptor or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_pselect6(int count, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timespec* timeout, const struct crunch_pselect_sigmask* signals);

/**
 * @brief Invokes raw ppoll with an eight-byte kernel signal set. The kernel may modify timeout, including on interruption.
 * @param descriptors Poll descriptor array.
 * @param count Number of descriptors.
 * @param timeout Writable timeout or NULL.
 * @param mask Temporary signal mask or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_ppoll(struct pollfd* descriptors, nfds_t count, struct timespec* timeout, const sigset_t* mask);

/**
 * @brief Unshares selected process resources; namespace operations may require privileges.
 * @param flags Linux namespace and sharing flags.
 * @return The syscall result, or -1 with errno set.
 */
int unshare(int flags);

/**
 * @brief Registers a Linux robust-futex list for the calling thread.
 * @param head Linux robust-list head.
 * @param length Head structure size.
 * @return The syscall result, or -1 with errno set.
 */
int set_robust_list(void* head, size_t length);

/**
 * @brief Queries a thread's Linux robust-futex list.
 * @param process Thread ID or zero for the caller.
 * @param head Receives robust-list head.
 * @param length Receives structure size.
 * @return The syscall result, or -1 with errno set.
 */
int get_robust_list(pid_t process, void** head, size_t* length);

/**
 * @brief Transfers bytes between descriptors, at least one of which must be a pipe.
 * @param input Input descriptor.
 * @param input_offset Input offset or NULL.
 * @param output Output descriptor.
 * @param output_offset Output offset or NULL.
 * @param length Maximum bytes.
 * @param flags Linux splice flags.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t splice(int input, off_t* input_offset, int output, off_t* output_offset, size_t length, unsigned int flags);

/**
 * @brief Duplicates bytes between pipes without consuming the input.
 * @param input Input pipe descriptor.
 * @param output Output pipe descriptor.
 * @param length Maximum bytes.
 * @param flags Linux splice flags.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t tee(int input, int output, size_t length, unsigned int flags);

/**
 * @brief Synchronizes part of a file using Linux sync_file_range.
 * @param descriptor Open descriptor.
 * @param offset Starting offset.
 * @param length Length in bytes.
 * @param flags Linux sync-file-range flags.
 * @return The syscall result, or -1 with errno set.
 */
int sync_file_range(int descriptor, off_t offset, off_t length, unsigned int flags);

/**
 * @brief Maps or copies user buffers into a pipe.
 * @param descriptor Pipe descriptor.
 * @param vectors I/O vector array.
 * @param count Number of vectors.
 * @param flags Linux splice flags.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t vmsplice(int descriptor, const struct iovec* vectors, unsigned long count, unsigned int flags);

/**
 * @brief Queries or migrates pages across NUMA nodes; privileges and kernel support vary.
 * @param process Process ID or zero.
 * @param count Number of pages.
 * @param pages Page address array.
 * @param nodes Destination node array or NULL.
 * @param status Receives per-page status.
 * @param flags Linux migration flags.
 * @return The syscall result, or -1 with errno set.
 */
long move_pages(pid_t process, unsigned long count, void** pages, const int* nodes, int* status, int flags);

/**
 * @brief Changes file timestamps with nanosecond precision.
 * @param directory Directory descriptor or AT_FDCWD.
 * @param path Relative or absolute path.
 * @param times Two timestamps or NULL.
 * @param flags Linux AT flags.
 * @return The syscall result, or -1 with errno set.
 */
int utimensat(int directory, const char* path, const struct timespec* times, int flags);

/**
 * @brief Waits for epoll events with an optional eight-byte kernel signal mask. Events use the packed 12-byte Linux x86-64 layout.
 * @param epoll Epoll descriptor.
 * @param events Writable array of 12-byte Linux epoll events.
 * @param maximum Maximum events.
 * @param timeout Timeout in milliseconds.
 * @param mask Temporary signal mask or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int epoll_pwait(int epoll, void* events, int maximum, int timeout, const sigset_t* mask);

/**
 * @brief Invokes the original signalfd syscall without flags.
 * @param descriptor Existing signalfd or -1.
 * @param mask Eight-byte signal mask.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_signalfd(int descriptor, const sigset_t* mask);

/**
 * @brief Creates a timer descriptor.
 * @param clock Clock ID.
 * @param flags Linux TFD flags.
 * @return The syscall result, or -1 with errno set.
 */
int timerfd_create(int clock, int flags);

/**
 * @brief Invokes the original eventfd syscall without flags.
 * @param value Initial counter value.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_eventfd(unsigned int value);

/**
 * @brief Allocates or manipulates disk space in a file.
 * @param descriptor Open descriptor.
 * @param mode Linux allocation flags.
 * @param offset Starting offset.
 * @param length Length in bytes.
 * @return The syscall result, or -1 with errno set.
 */
int fallocate(int descriptor, int mode, off_t offset, off_t length);

/**
 * @brief Arms or disarms a timer descriptor.
 * @param descriptor Timer descriptor.
 * @param flags Linux timer flags.
 * @param value New timer state.
 * @param previous Previous state or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int timerfd_settime(int descriptor, int flags, const struct itimerspec* value, struct itimerspec* previous);

/**
 * @brief Reads timer descriptor state.
 * @param descriptor Timer descriptor.
 * @param value Receives timer state.
 * @return The syscall result, or -1 with errno set.
 */
int timerfd_gettime(int descriptor, struct itimerspec* value);

/**
 * @brief Accepts a connection with descriptor flags.
 * @param descriptor Listening socket.
 * @param address Peer address or NULL.
 * @param length Address capacity or NULL.
 * @param flags SOCK_NONBLOCK and/or SOCK_CLOEXEC.
 * @return The syscall result, or -1 with errno set.
 */
int accept4(int descriptor, struct sockaddr* address, socklen_t* length, int flags);

/**
 * @brief Creates or updates a signal descriptor with flags and an eight-byte kernel signal mask.
 * @param descriptor Existing signalfd or -1.
 * @param mask Eight-byte signal mask.
 * @param flags Linux SFD flags.
 * @return The syscall result, or -1 with errno set.
 */
int signalfd(int descriptor, const sigset_t* mask, int flags);

/**
 * @brief Creates an event counter with descriptor flags.
 * @param value Initial counter value.
 * @param flags Linux EFD flags.
 * @return The syscall result, or -1 with errno set.
 */
int eventfd(unsigned int value, int flags);

/**
 * @brief Creates an epoll descriptor with flags.
 * @param flags EPOLL_CLOEXEC or zero.
 * @return The syscall result, or -1 with errno set.
 */
int epoll_create1(int flags);

/**
 * @brief Duplicates a descriptor, optionally setting close-on-exec.
 * @param old_descriptor Existing descriptor.
 * @param new_descriptor Target descriptor.
 * @param flags O_CLOEXEC or zero.
 * @return The syscall result, or -1 with errno set.
 */
int dup3(int old_descriptor, int new_descriptor, int flags);

/**
 * @brief Creates a pipe with descriptor flags.
 * @param descriptors Receives two pipe descriptors.
 * @param flags Linux pipe flags.
 * @return The syscall result, or -1 with errno set.
 */
int pipe2(int* descriptors, int flags);

/**
 * @brief Creates an inotify descriptor with flags.
 * @param flags Linux IN_NONBLOCK and IN_CLOEXEC flags.
 * @return The syscall result, or -1 with errno set.
 */
int inotify_init1(int flags);

/**
 * @brief Reads vectors at a 64-bit offset without changing the file position. The wrapper splits the offset into kernel low and high words.
 * @param descriptor Open descriptor.
 * @param vectors Writable I/O vector array.
 * @param count Number of vectors.
 * @param offset File offset.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t preadv(int descriptor, const struct iovec* vectors, int count, off_t offset);

/**
 * @brief Writes vectors at a 64-bit offset without changing the file position. The wrapper splits the offset into kernel low and high words.
 * @param descriptor Open descriptor.
 * @param vectors Source I/O vector array.
 * @param count Number of vectors.
 * @param offset File offset.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t pwritev(int descriptor, const struct iovec* vectors, int count, off_t offset);

/**
 * @brief Queues signal information to a specific thread.
 * @param process Thread group ID.
 * @param thread Thread ID.
 * @param signal Signal number.
 * @param information Kernel-layout signal information.
 * @return The syscall result, or -1 with errno set.
 */
int rt_tgsigqueueinfo(pid_t process, pid_t thread, int signal, const siginfo_t* information);

/**
 * @brief Opens a performance event. The caller must provide a kernel-compatible perf_event_attr including its size; policy often restricts access.
 * @param attributes Linux perf_event_attr bytes.
 * @param process Process ID or -1.
 * @param cpu CPU or -1.
 * @param group Group descriptor or -1.
 * @param flags Linux perf event flags.
 * @return The syscall result, or -1 with errno set.
 */
int perf_event_open(const void* attributes, pid_t process, int cpu, int group, unsigned long flags);

/**
 * @brief Receives multiple messages. Each message uses the Linux x86-64 mmsghdr layout. Linux may modify timeout.
 * @param descriptor Socket descriptor.
 * @param messages Writable message array.
 * @param count Number of messages.
 * @param flags Linux message flags.
 * @param timeout Writable timeout or NULL.
 * @return The syscall result, or -1 with errno set.
 */
int recvmmsg(int descriptor, struct crunch_mmsghdr* messages, unsigned int count, unsigned int flags, struct timespec* timeout);

#endif
