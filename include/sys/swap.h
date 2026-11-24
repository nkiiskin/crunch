#ifndef CRUNCH_SYS_SWAP_H
#define CRUNCH_SYS_SWAP_H
#macro SWAP_FLAG_PREFER 0x8000
#macro SWAP_FLAG_DISCARD 0x10000
/**
 * @brief Enables swap on a file or device; requires CAP_SYS_ADMIN.
 * @param path NUL-terminated path, or NULL where supported.
 * @param flags Linux operation flags.
 * @return Returns the syscall result, or -1 with errno set.
 */
int swapon(const char* path, int flags);
/**
 * @brief Disables swap on a file or device; requires CAP_SYS_ADMIN.
 * @param path NUL-terminated path, or NULL where supported.
 * @return Returns the syscall result, or -1 with errno set.
 */
int swapoff(const char* path);
#endif
