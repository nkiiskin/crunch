#ifndef CRUNCH_SYS_PRCTL_H
#define CRUNCH_SYS_PRCTL_H

#macro PR_GET_PDEATHSIG 2
#macro PR_SET_PDEATHSIG 1
#macro PR_GET_DUMPABLE 3
#macro PR_SET_DUMPABLE 4
#macro PR_SET_NAME 15
#macro PR_GET_NAME 16
#macro PR_SET_NO_NEW_PRIVS 38
#macro PR_GET_NO_NEW_PRIVS 39
/**
 * @brief Invokes Linux prctl with five fixed arguments.
 * @param option Linux operation selector.
 * @param argument2 Second raw argument.
 * @param argument3 Third raw argument.
 * @param argument4 Fourth raw argument.
 * @param argument5 Fifth raw argument.
 * @return Returns the syscall result, or -1 with errno set.
 */
int prctl(int option, unsigned long argument2, unsigned long argument3,
          unsigned long argument4, unsigned long argument5);
/**
 * @brief Invokes prctl with a pointer as its second argument.
 * @param option Linux operation selector.
 * @param argument2 Pointer argument.
 * @param argument3 Third raw argument.
 * @param argument4 Fourth raw argument.
 * @param argument5 Fifth raw argument.
 * @return The result, or -1 with errno set.
 */
int prctl_ptr(int option, void* argument2, unsigned long argument3,
              unsigned long argument4, unsigned long argument5);
#endif
