#ifndef CRUNCH_SYS_LEGACY_SYSCALLS_H
#define CRUNCH_SYS_LEGACY_SYSCALLS_H

/**
 * @brief Invokes obsolete Linux syscall 134.
 * @param path NUL-terminated path.
 * @return The result, or -1 with errno set.
 */
int uselib(const char* path);
struct ustat {
    int f_tfree;
    unsigned long f_tinode;
    char f_fname[6];
    char f_fpack[6];
};
/**
 * @brief Invokes obsolete Linux syscall 136.
 * @param device Raw Linux encoded device number.
 * @param buffer Writable output buffer.
 * @return The result, or -1 with errno set.
 */
int ustat(unsigned int device, struct ustat* buffer);
/**
 * @brief Invokes the obsolete Linux sysfs syscall with raw arguments.
 * @param option Linux operation selector.
 * @param argument1 First raw syscall argument.
 * @param argument2 Second raw syscall argument.
 * @return The result, or -1 with errno set.
 */
long crunch_sysfs(int option, unsigned long argument1, unsigned long argument2);
/**
 * @brief Looks up a filesystem type index with the legacy sysfs syscall.
 * @param name NUL-terminated filesystem type name.
 * @return Index, or -1 with errno set.
 */
int crunch_sysfs_index(const char* name);
/**
 * @brief Looks up a filesystem type name with the legacy sysfs syscall.
 * @param index Filesystem type index.
 * @param buffer Writable buffer of at least 256 bytes.
 * @return Zero, or -1 with errno set.
 */
int crunch_sysfs_name(int index, char* buffer);
/**
 * @brief Invokes obsolete syscall slot 180, unimplemented on Linux x86-64.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_nfsservctl(void);

/**
 * @brief Invokes obsolete STREAMS syscall slot 181, unimplemented on Linux x86-64.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_getpmsg(void);

/**
 * @brief Invokes obsolete STREAMS syscall slot 182, unimplemented on Linux x86-64.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_putpmsg(void);

/**
 * @brief Invokes reserved AFS syscall slot 183, unimplemented on Linux x86-64.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_afs_syscall(void);

/**
 * @brief Invokes reserved TUX syscall slot 184, unimplemented on Linux x86-64.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_tuxcall(void);

/**
 * @brief Invokes reserved security syscall slot 185, unimplemented on Linux x86-64.
 * @return The syscall result, or -1 with errno set.
 */
long crunch_security(void);

/**
 * @brief Invokes obsolete epoll_ctl_old syscall slot 214, unimplemented on Linux x86-64.
 * @param epoll Epoll descriptor.
 * @param operation Linux operation selector.
 * @param descriptor Open file descriptor.
 * @param event Operation-specific event data.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_epoll_ctl_old(int epoll, int operation, int descriptor, void* event);

/**
 * @brief Invokes obsolete epoll_wait_old syscall slot 215, unimplemented on Linux x86-64.
 * @param epoll Epoll descriptor.
 * @param events Writable completion event array.
 * @param maximum Maximum number of events.
 * @param timeout Optional timeout.
 * @return The syscall result, or -1 with errno set.
 */
int crunch_epoll_wait_old(int epoll, void* events, int maximum, int timeout);

#endif
