#ifndef CRUNCH_SYS_PTRACE_H
#define CRUNCH_SYS_PTRACE_H

#include <sys/types.h>

#macro PTRACE_TRACEME 0
#macro PTRACE_PEEKTEXT 1
#macro PTRACE_PEEKDATA 2
#macro PTRACE_POKETEXT 4
#macro PTRACE_POKEDATA 5
#macro PTRACE_CONT 7
#macro PTRACE_KILL 8
#macro PTRACE_SINGLESTEP 9
#macro PTRACE_ATTACH 16
#macro PTRACE_DETACH 17

/**
 * @brief Invokes Linux ptrace with four fixed arguments.
 * @param request Linux ptrace request.
 * @param process Process identifier; zero selects the caller where supported.
 * @param address Request-specific address.
 * @param data Request-specific data or output buffer.
 * @return The result, or -1 with errno set.
 */
long ptrace(long request, pid_t process, void* address, void* data);
/**
 * @brief Invokes Linux ptrace with integer data, such as a POKE word or signal.
 * @param request Linux ptrace request.
 * @param process Process identifier.
 * @param address Request-specific address.
 * @param data Request-specific integer data.
 * @return The result, or -1 with errno set.
 */
long ptrace_value(long request, pid_t process, void* address, unsigned long data);
#endif
