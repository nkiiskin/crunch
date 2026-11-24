#ifndef CRUNCH_SYS_GETDENTS_H
#define CRUNCH_SYS_GETDENTS_H

#include <sys/types.h>

/* The name is variable length; d_type is the last byte of the record. */
struct linux_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[1];
};

/**
 * @brief Reads legacy Linux x86-64 directory records.
 * @param descriptor Open directory descriptor.
 * @param buffer Writable record buffer.
 * @param byte_count Buffer size.
 * @return Bytes read, zero at end of directory, or -1 with errno set.
 */
ssize_t getdents(unsigned int descriptor, struct linux_dirent* buffer, unsigned int byte_count);

#endif
