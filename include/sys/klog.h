#ifndef CRUNCH_SYS_KLOG_H
#define CRUNCH_SYS_KLOG_H

/**
 * @brief Invokes the Linux syslog system call.
 * @param type Linux operation selector.
 * @param buffer Writable output buffer.
 * @param length Buffer length in bytes.
 * @return The result, or -1 with errno set.
 */
int klogctl(int type, char* buffer, int length);
#endif
