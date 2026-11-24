#ifndef CRUNCH_SYS_FILE_H
#define CRUNCH_SYS_FILE_H

#macro LOCK_SH 1
#macro LOCK_EX 2
#macro LOCK_NB 4
#macro LOCK_UN 8

/**
 * @brief Applies or removes a Linux advisory file lock.
 * @param descriptor Open file descriptor.
 * @param operation LOCK_SH, LOCK_EX, or LOCK_UN, optionally with LOCK_NB.
 * @return Zero, or -1 with errno set.
 */
int flock(int descriptor, int operation);

#endif
