#ifndef CRUNCH_SYS_WAIT_H
#define CRUNCH_SYS_WAIT_H

#include <sys/types.h>
#include <sys/resource.h>

#macro WNOHANG 1
#macro WUNTRACED 2
#macro WCONTINUED 8

#macro WIFEXITED(status) (((status) & 127) == 0)
#macro WEXITSTATUS(status) (((status) >> 8) & 255)
#macro WIFSIGNALED(status) (((status) & 127) != 0 && ((status) & 127) != 127)
#macro WTERMSIG(status) ((status) & 127)

/**
 * @brief Waits for a child process.
 * @param process Child PID or a Linux process-group selector.
 * @param status Receives the status, or NULL.
 * @param options Linux wait options.
 * @return Child PID, zero when requested without blocking, or -1 with errno set.
 */
pid_t waitpid(pid_t process, int* status, int options);

/**
 * @brief Waits for a child and optionally obtains its resource usage.
 * @param process Child PID or a Linux process-group selector.
 * @param status Receives the status, or NULL.
 * @param options Linux wait options.
 * @param usage Receives resource usage, or NULL.
 * @return Child PID, zero for WNOHANG, or -1 with errno set.
 */
pid_t wait4(pid_t process, int* status, int options, struct rusage* usage);

#endif
