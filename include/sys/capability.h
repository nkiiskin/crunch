#ifndef CRUNCH_SYS_CAPABILITY_H
#define CRUNCH_SYS_CAPABILITY_H

#include <sys/types.h>
#macro _LINUX_CAPABILITY_VERSION_1 0x19980330
#macro _LINUX_CAPABILITY_VERSION_2 0x20071026
#macro _LINUX_CAPABILITY_VERSION_3 0x20080522

struct __user_cap_header_struct {
    unsigned int version;
    int pid;
};
struct __user_cap_data_struct {
    unsigned int effective;
    unsigned int permitted;
    unsigned int inheritable;
};
typedef struct __user_cap_header_struct* cap_user_header_t;
typedef struct __user_cap_data_struct* cap_user_data_t;

/**
 * @brief Reads Linux thread capabilities using the kernel header and data layout.
 * @param header Linux capability header.
 * @param data Request-specific data or output buffer.
 * @return The result, or -1 with errno set.
 */
int capget(cap_user_header_t header, cap_user_data_t data);
/**
 * @brief Sets Linux thread capabilities using the kernel header and data layout.
 * @param header Linux capability header.
 * @param data Request-specific data or output buffer.
 * @return The result, or -1 with errno set.
 */
int capset(cap_user_header_t header, const struct __user_cap_data_struct* data);
#endif
