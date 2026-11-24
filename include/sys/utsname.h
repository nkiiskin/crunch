#ifndef CRUNCH_SYS_UTSNAME_H
#define CRUNCH_SYS_UTSNAME_H

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/**
 * @brief Reads Linux kernel and machine identification.
 * @param name Writable identification structure.
 * @return Zero on success, or -1 with errno set.
 */
int uname(struct utsname* name);

#endif
