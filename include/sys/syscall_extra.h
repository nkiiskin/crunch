#ifndef CRUNCH_SYS_SYSCALL_EXTRA_H
#define CRUNCH_SYS_SYSCALL_EXTRA_H
#include <stddef.h>
#include <sys/types.h>
/**
 * @brief Invokes Linux modify_ldt with the raw operation and architecture-specific buffer.
 * @param operation Linux operation selector.
 * @param buffer Operation-specific buffer.
 * @param byte_count Length in bytes.
 * @return Returns the syscall result, or -1 with errno set.
 */
int modify_ldt(int operation, void* buffer, unsigned long byte_count);
/**
 * @brief Invokes obsolete _sysctl syscall 156 with a raw argument structure.
 * @param arguments Raw syscall argument structure.
 * @return Returns the syscall result, or -1 with errno set.
 */
int crunch_sysctl(void* arguments);
/**
 * @brief Invokes the Linux quota control syscall using command-specific data and privileges.
 * @param command Linux operation command.
 * @param special Filesystem or block device path.
 * @param id Quota identifier.
 * @param address Address or request-specific pointer.
 * @return Returns the syscall result, or -1 with errno set.
 */
int quotactl(int command, const char* special, int id, void* address);
/**
 * @brief Sends a signal to a Linux thread ID.
 * @param thread Linux thread ID.
 * @param signal Signal number, or zero to check existence.
 * @return The syscall result, or -1 with errno set.
 */
int tkill(pid_t thread, int signal);

/**
 * @brief Invokes syscall slot 205 with a raw descriptor; this legacy i386 interface is unimplemented on Linux x86-64.
 * @param descriptor Open file descriptor.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_set_thread_area(void* descriptor);

/**
 * @brief Invokes syscall slot 211 with a raw descriptor; this legacy i386 interface is unimplemented on Linux x86-64.
 * @param descriptor Open file descriptor.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_get_thread_area(void* descriptor);

/**
 * @brief Resolves a kernel directory-entry cookie where supported and authorized; the interface is primarily for profiling.
 * @param cookie Kernel directory-entry cookie.
 * @param buffer Writable output buffer.
 * @param size Buffer or bitmap size in bytes.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t lookup_dcookie(unsigned long cookie, char* buffer, size_t size);

/**
 * @brief Sets the calling thread clear-child-TID address and returns its thread ID.
 * @param address Futex word or mapping address.
 * @return The syscall result, or -1 with errno set.
 */
pid_t set_tid_address(int* address);

/**
 * @brief Invokes the internal Linux restart_syscall entry point.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_restart_syscall(void);

#endif
