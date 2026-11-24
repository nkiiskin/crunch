#ifndef CRUNCH_SYS_FSUID_H
#define CRUNCH_SYS_FSUID_H

#include <sys/types.h>
/**
 * @brief Attempts to set the filesystem user ID.
 * @param user User ID.
 * @return The value described above; errno is not modified.
 */
uid_t setfsuid(uid_t user);
/**
 * @brief Attempts to set the filesystem group ID.
 * @param group Group ID.
 * @return The value described above; errno is not modified.
 */
gid_t setfsgid(gid_t group);
#endif
