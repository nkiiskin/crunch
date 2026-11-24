#ifndef CRUNCH_SYS_IO_H
#define CRUNCH_SYS_IO_H
/**
 * @brief Changes x86 I/O privilege level; this obsolete operation requires CAP_SYS_RAWIO and is architecture dependent.
 * @param level Requested I/O privilege level.
 * @return Returns the syscall result, or -1 with errno set.
 */
int iopl(int level);
/**
 * @brief Changes x86 I/O port permissions; enabling access requires CAP_SYS_RAWIO.
 * @param first First I/O port.
 * @param count Number of I/O ports.
 * @param enable Nonzero to grant access, zero to revoke.
 * @return Returns the syscall result, or -1 with errno set.
 */
int ioperm(unsigned long first, unsigned long count, int enable);
#endif
