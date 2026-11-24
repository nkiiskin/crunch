#ifndef CRUNCH_UNISTD_H
#define CRUNCH_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#ifndef NULL
#macro NULL 0
#endif

#macro STDIN_FILENO 0
#macro STDOUT_FILENO 1
#macro STDERR_FILENO 2
#macro F_OK 0
#macro X_OK 1
#macro W_OK 2
#macro R_OK 4

/**
 * @brief Sleeps for a number of seconds.
 * @param seconds Requested duration.
 * @return Zero on completion, or remaining seconds rounded up on interruption.
 */
unsigned int sleep(unsigned int seconds);

/**
 * @brief Waits until a signal is caught.
 * @return -1 with errno set to EINTR after a handler returns.
 */
int pause(void);

/**
 * @brief Schedules or cancels a real-time SIGALRM.
 * @param seconds Delay in seconds, or zero to cancel.
 * @return Remaining seconds from the previous alarm, or zero.
 */
unsigned int alarm(unsigned int seconds);

/**
 * @brief Requests a new Linux program break.
 * @param address Requested end of the data segment.
 * @return Zero, or -1 with errno set to ENOMEM.
 * @note Direct changes can conflict with other users of the process break.
 */
int brk(void* address);

/**
 * @brief Replaces the process image with a program and explicit environment.
 * @param path NUL-terminated executable path.
 * @param arguments Null-terminated argument vector.
 * @param environment Null-terminated environment vector.
 * @return Only returns on failure, with -1 and errno set.
 */
int execve(const char* path, char* const* arguments, char* const* environment);

/**
 * @brief Terminates the process without running cleanup functions.
 * @param status Process exit status.
 * @return Does not return.
 */
void _exit(int status);

#ifndef SEEK_SET
#macro SEEK_SET 0
#macro SEEK_CUR 1
#macro SEEK_END 2
#endif

/**
 * @brief Checks access using the process's real user and group IDs.
 * @param path NUL-terminated path.
 * @param mode F_OK or a combination of R_OK, W_OK, and X_OK.
 * @return Zero on success, or -1 with errno set.
 * @note This check is not a safe authorization check before a separate open.
 */
int access(const char* path, int mode);

/**
 * @brief Duplicates a descriptor using the lowest available number.
 * @param descriptor Open descriptor.
 * @return New descriptor, or -1 with errno set.
 */
int dup(int descriptor);

/**
 * @brief Duplicates a descriptor onto a specified descriptor.
 * @param descriptor Open source descriptor.
 * @param new_descriptor Destination descriptor.
 * @return Destination descriptor, or -1 with errno set.
 */
int dup2(int descriptor, int new_descriptor);

/**
 * @brief Creates a pipe, with read and write descriptors in that order.
 * @param descriptors Writable storage for two int values.
 * @return Zero on success, or -1 with errno set.
 */
int pipe(int* descriptors);

/**
 * @brief Reads a directory-relative symbolic link target without a terminator.
 * @param directory Open directory descriptor or AT_FDCWD.
 * @param path NUL-terminated link path.
 * @param buffer Writable destination.
 * @param byte_count Maximum bytes to copy.
 * @return Bytes copied, or -1 with errno set.
 */
ssize_t readlinkat(int directory, const char* path, char* buffer, size_t byte_count);

/**
 * @brief Removes a directory-relative entry.
 * @param directory Open directory descriptor or AT_FDCWD.
 * @param path NUL-terminated path.
 * @param flags Zero or AT_REMOVEDIR.
 * @return Zero on success, or -1 with errno set.
 */
int unlinkat(int directory, const char* path, int flags);

/**
 * @brief Creates a symbolic link at a directory-relative path.
 * @param target NUL-terminated target stored in the link.
 * @param directory Open directory descriptor or AT_FDCWD.
 * @param path NUL-terminated link path.
 * @return Zero on success, or -1 with errno set.
 */
int symlinkat(const char* target, int directory, const char* path);

/**
 * @brief Reads bytes from a file descriptor.
 * @param descriptor Open file descriptor.
 * @param buffer Writable destination.
 * @param byte_count Maximum bytes to read.
 * @return Bytes read, zero at EOF, or -1 with errno set.
 */
ssize_t read(int descriptor, void* buffer, size_t byte_count);

/**
 * @brief Writes bytes to a file descriptor.
 * @param descriptor Open file descriptor.
 * @param buffer Source bytes.
 * @param byte_count Number of bytes to write.
 * @return Bytes written, or -1 with errno set.
 */
ssize_t write(int descriptor, const void* buffer, size_t byte_count);

/**
 * @brief Reads bytes at an offset without changing the file position.
 * @param descriptor Open file descriptor.
 * @param buffer Writable destination.
 * @param byte_count Maximum bytes to read.
 * @param offset File offset in bytes.
 * @return Bytes read, zero at EOF, or -1 with errno set.
 */
ssize_t pread(int descriptor, void* buffer, size_t byte_count, off_t offset);

/**
 * @brief Writes bytes at an offset without changing the file position.
 * @param descriptor Open file descriptor.
 * @param buffer Source bytes.
 * @param byte_count Number of bytes to write.
 * @param offset File offset in bytes.
 * @return Bytes written, or -1 with errno set.
 */
ssize_t pwrite(int descriptor, const void* buffer, size_t byte_count, off_t offset);

/**
 * @brief Changes the file offset of an open descriptor.
 * @param descriptor Open file descriptor.
 * @param offset Signed byte offset relative to whence.
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END.
 * @return Resulting offset, or -1 with errno set.
 */
off_t lseek(int descriptor, off_t offset, int whence);

/**
 * @brief Closes a file descriptor.
 * @param descriptor Open file descriptor.
 * @return Zero on success, or -1 with errno set.
 */
int close(int descriptor);

/**
 * @brief Flushes a file's data and metadata to stable storage.
 * @param descriptor Open file descriptor.
 * @return Zero, or -1 with errno set.
 */
int fsync(int descriptor);

/**
 * @brief Flushes data and required metadata to stable storage.
 * @param descriptor Open file descriptor.
 * @return Zero, or -1 with errno set.
 */
int fdatasync(int descriptor);

/**
 * @brief Changes the length of a file named by a path.
 * @param path NUL-terminated path.
 * @param length New length in bytes.
 * @return Zero, or -1 with errno set.
 */
int truncate(const char* path, off_t length);

/**
 * @brief Changes the length of an open file.
 * @param descriptor Open file descriptor.
 * @param length New length in bytes.
 * @return Zero, or -1 with errno set.
 */
int ftruncate(int descriptor, off_t length);

/**
 * @brief Gets the absolute current working directory.
 * @param buffer Writable destination, or NULL to allocate one.
 * @param size Destination size; zero selects automatic sizing only for NULL.
 * @return Buffer on success, or NULL with errno set.
 * @note Free an automatically allocated buffer with free.
 */
char* getcwd(char* buffer, size_t size);

/**
 * @brief Reads a symbolic link target without appending a NUL byte.
 * @param path NUL-terminated symbolic link path.
 * @param buffer Writable destination.
 * @param byte_count Maximum bytes to copy.
 * @return Bytes copied, or -1 with errno set.
 */
ssize_t readlink(const char* path, char* buffer, size_t byte_count);

/**
 * @brief Removes a non-directory filesystem entry.
 * @param path NUL-terminated path.
 * @return Zero on success, or -1 with errno set.
 */
int unlink(const char* path);

/**
 * @brief Changes the current working directory.
 * @param path NUL-terminated directory path.
 * @return Zero, or -1 with errno set.
 */
int chdir(const char* path);

/**
 * @brief Changes the current working directory to an open directory.
 * @param descriptor Open directory descriptor.
 * @return Zero, or -1 with errno set.
 */
int fchdir(int descriptor);

/**
 * @brief Creates a hard link.
 * @param oldpath Existing path.
 * @param newpath Path for the new link.
 * @return Zero, or -1 with errno set.
 */
int link(const char* oldpath, const char* newpath);

/**
 * @brief Creates a symbolic link.
 * @param target NUL-terminated link target.
 * @param path Path for the new link.
 * @return Zero, or -1 with errno set.
 */
int symlink(const char* target, const char* path);

/**
 * @brief Changes ownership of a file, following symbolic links.
 * @param path NUL-terminated path.
 * @param owner New owner, or (uid_t)-1 to leave unchanged.
 * @param group New group, or (gid_t)-1 to leave unchanged.
 * @return Zero, or -1 with errno set.
 */
int chown(const char* path, uid_t owner, gid_t group);

/**
 * @brief Changes ownership of an open file.
 * @param descriptor Open file descriptor.
 * @param owner New owner, or (uid_t)-1 to leave unchanged.
 * @param group New group, or (gid_t)-1 to leave unchanged.
 * @return Zero, or -1 with errno set.
 */
int fchown(int descriptor, uid_t owner, gid_t group);

/**
 * @brief Changes ownership without following the final symbolic link.
 * @param path NUL-terminated path.
 * @param owner New owner, or (uid_t)-1 to leave unchanged.
 * @param group New group, or (gid_t)-1 to leave unchanged.
 * @return Zero, or -1 with errno set.
 */
int lchown(const char* path, uid_t owner, gid_t group);

/**
 * @brief Removes an empty directory.
 * @param path NUL-terminated path.
 * @return Zero on success, or -1 with errno set.
 */
int rmdir(const char* path);

/**
 * @brief Creates a child process using the Linux fork syscall.
 * @return Child PID in the parent, zero in the child, or -1 with errno set.
 * @note Crunch provides no atfork handlers; reap child processes separately.
 */
pid_t fork(void);

/**
 * @brief Creates a child while suspending the parent until exec or exit.
 * @return Child PID in the parent, zero in the child, or -1 with errno set.
 * @note The child shares the parent's address space and stack; call only execve or _exit.
 */
pid_t vfork(void);

/**
 * @brief Gets the current process ID.
 * @return Current process ID.
 */
pid_t getpid(void);

/**
 * @brief Returns the calling process's real user ID.
 * @return The value described above; errno is not modified.
 */
uid_t getuid(void);
/**
 * @brief Returns the calling process's real group ID.
 * @return The value described above; errno is not modified.
 */
gid_t getgid(void);
/**
 * @brief Returns the calling process's effective user ID.
 * @return The value described above; errno is not modified.
 */
uid_t geteuid(void);
/**
 * @brief Returns the calling process's effective group ID.
 * @return The value described above; errno is not modified.
 */
gid_t getegid(void);
/**
 * @brief Sets user IDs according to Linux credential and privilege rules.
 * @param user User ID.
 * @return The result, or -1 with errno set.
 */
int setuid(uid_t user);
/**
 * @brief Sets group IDs according to Linux credential and privilege rules.
 * @param group Group ID.
 * @return The result, or -1 with errno set.
 */
int setgid(gid_t group);
/**
 * @brief Returns the parent process ID.
 * @return The value described above; errno is not modified.
 */
pid_t getppid(void);
/**
 * @brief Returns the calling process's process group ID.
 * @return The value described above; errno is not modified.
 */
pid_t getpgrp(void);
/**
 * @brief Sets the process group for a process, subject to Linux session and permission rules.
 * @param process Process identifier; zero selects the caller where supported.
 * @param group Group ID.
 * @return The result, or -1 with errno set.
 */
int setpgid(pid_t process, pid_t group);
/**
 * @brief Returns a process group ID; process zero selects the caller.
 * @param process Process identifier; zero selects the caller where supported.
 * @return The result, or -1 with errno set.
 */
pid_t getpgid(pid_t process);
/**
 * @brief Creates a session and process group, subject to Linux session rules.
 * @return The result, or -1 with errno set.
 */
pid_t setsid(void);
/**
 * @brief Returns a process session ID; process zero selects the caller.
 * @param process Process identifier; zero selects the caller where supported.
 * @return The result, or -1 with errno set.
 */
pid_t getsid(pid_t process);
/**
 * @brief Sets the real and effective user IDs; (uid_t)-1 leaves an ID unchanged.
 * @param real Real ID or its output pointer.
 * @param effective Effective ID or its output pointer.
 * @return The result, or -1 with errno set.
 */
int setreuid(uid_t real, uid_t effective);
/**
 * @brief Sets the real and effective group IDs; (gid_t)-1 leaves an ID unchanged.
 * @param real Real ID or its output pointer.
 * @param effective Effective ID or its output pointer.
 * @return The result, or -1 with errno set.
 */
int setregid(gid_t real, gid_t effective);
/**
 * @brief Reads supplementary groups.
 * @param count Number of group entries.
 * @param groups Supplementary group array.
 * @return The result, or -1 with errno set.
 */
int getgroups(int count, gid_t* groups);
/**
 * @brief Sets supplementary groups, subject to capabilities and user namespace restrictions.
 * @param count Number of group entries.
 * @param groups Supplementary group array.
 * @return The result, or -1 with errno set.
 */
int setgroups(size_t count, const gid_t* groups);
/**
 * @brief Sets real, effective and saved user IDs; (uid_t)-1 leaves an ID unchanged.
 * @param real Real ID or its output pointer.
 * @param effective Effective ID or its output pointer.
 * @param saved Saved ID or its output pointer.
 * @return The result, or -1 with errno set.
 */
int setresuid(uid_t real, uid_t effective, uid_t saved);
/**
 * @brief Writes the real, effective and saved user IDs to three writable pointers.
 * @param real Real ID or its output pointer.
 * @param effective Effective ID or its output pointer.
 * @param saved Saved ID or its output pointer.
 * @return The result, or -1 with errno set.
 */
int getresuid(uid_t* real, uid_t* effective, uid_t* saved);
/**
 * @brief Sets real, effective and saved group IDs; (gid_t)-1 leaves an ID unchanged.
 * @param real Real ID or its output pointer.
 * @param effective Effective ID or its output pointer.
 * @param saved Saved ID or its output pointer.
 * @return The result, or -1 with errno set.
 */
int setresgid(gid_t real, gid_t effective, gid_t saved);
/**
 * @brief Writes the real, effective and saved group IDs to three writable pointers.
 * @param real Real ID or its output pointer.
 * @param effective Effective ID or its output pointer.
 * @param saved Saved ID or its output pointer.
 * @return The result, or -1 with errno set.
 */
int getresgid(gid_t* real, gid_t* effective, gid_t* saved);
/**
 * @brief Virtually hangs up the calling process's controlling terminal; typically requires CAP_SYS_TTY_CONFIG.
 * @return Returns the syscall result, or -1 with errno set.
 */
int vhangup(void);
/**
 * @brief Changes the calling process root directory, normally requiring CAP_SYS_CHROOT.
 * @param path NUL-terminated path, or NULL where supported.
 * @return Returns the syscall result, or -1 with errno set.
 */
int chroot(const char* path);
/**
 * @brief Requests writeback of filesystem data and metadata.
 * @return No value is returned.
 */
void sync(void);
/**
 * @brief Enables process accounting at path or disables it with NULL; requires privilege.
 * @param path NUL-terminated path, or NULL where supported.
 * @return Returns the syscall result, or -1 with errno set.
 */
int acct(const char* path);
/**
 * @brief Sets the hostname of the calling UTS namespace; requires CAP_SYS_ADMIN in its user namespace.
 * @param name Name or module path.
 * @param length Length in bytes.
 * @return Returns the syscall result, or -1 with errno set.
 */
int sethostname(const char* name, size_t length);
/**
 * @brief Sets the NIS domain name of the calling UTS namespace; requires CAP_SYS_ADMIN in its user namespace.
 * @param name Name or module path.
 * @param length Length in bytes.
 * @return Returns the syscall result, or -1 with errno set.
 */
int setdomainname(const char* name, size_t length);

#macro FILE_READ 0
#macro FILE_WRITE 577
#macro FILE_APPEND 1089


/**
 * @brief Opens a file using one of the FILE_* modes.
 * @param path NUL-terminated path to open.
 * @param mode FILE_READ, FILE_WRITE, or FILE_APPEND.
 * @return File descriptor on success, or -1 with errno set to the positive Linux error.
 */
long file_open(const char* path, int mode);

/**
 * @brief Reads bytes from an open file.
 * @param descriptor Descriptor returned by file_open.
 * @param buffer Writable buffer receiving the bytes.
 * @param byte_count Maximum number of bytes to read.
 * @return Bytes read, zero at end of file, or -1 with positive errno set.
 */
long file_read(long descriptor, char* buffer, long byte_count);

/**
 * @brief Writes bytes to an open file.
 * @param descriptor Descriptor returned by file_open.
 * @param buffer Bytes to write.
 * @param byte_count Number of bytes requested.
 * @return Bytes written, or -1 with errno set to the positive Linux error.
 */
long file_write(long descriptor, const char* buffer, long byte_count);

/**
 * @brief Closes an open file descriptor.
 * @param descriptor Descriptor returned by file_open.
 * @return Zero on success, or -1 with errno set to the positive Linux error.
 */
long file_close(long descriptor);


/**
 * @brief Returns the calling Linux thread ID.
 * @return The syscall result, or -1 with errno set.
 */
pid_t gettid(void);

/**
 * @brief Schedules file data for read-ahead without changing the file offset.
 * @param descriptor Open file descriptor.
 * @param offset File offset in bytes.
 * @param byte_count Length in bytes.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t readahead(int descriptor, off_t offset, size_t byte_count);

#endif /* CRUNCH_UNISTD_H */
