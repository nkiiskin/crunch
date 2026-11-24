#ifndef CRUNCH_SYS_MOUNT_H
#define CRUNCH_SYS_MOUNT_H
#macro MS_RDONLY 1
#macro MS_NOSUID 2
#macro MS_NODEV 4
#macro MS_NOEXEC 8
#macro MS_REMOUNT 32
#macro MS_BIND 4096
#macro MNT_FORCE 1
#macro MNT_DETACH 2
#macro MNT_EXPIRE 4
/**
 * @brief Mounts a filesystem in the calling mount namespace, subject to filesystem rules and privileges.
 * @param source Source path or filesystem-specific name.
 * @param target Mount target path.
 * @param filesystem Filesystem type name.
 * @param flags Linux operation flags.
 * @param data Filesystem-specific data.
 * @return Returns the syscall result, or -1 with errno set.
 */
int mount(const char* source, const char* target, const char* filesystem,
          unsigned long flags, const void* data);
/**
 * @brief Unmounts a filesystem using Linux MNT_* flags, subject to privileges.
 * @param target Mount target path.
 * @param flags Linux operation flags.
 * @return Returns the syscall result, or -1 with errno set.
 */
int umount2(const char* target, int flags);
/**
 * @brief Changes the root mount in the calling mount namespace; requires privileges and strict mount-point conditions.
 * @param new_root New root mount path.
 * @param put_old Directory for the old root mount.
 * @return Returns the syscall result, or -1 with errno set.
 */
int pivot_root(const char* new_root, const char* put_old);
#endif
