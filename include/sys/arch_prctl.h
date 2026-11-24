#ifndef CRUNCH_SYS_ARCH_PRCTL_H
#define CRUNCH_SYS_ARCH_PRCTL_H

#macro ARCH_SET_GS 0x1001
#macro ARCH_SET_FS 0x1002
#macro ARCH_GET_FS 0x1003
#macro ARCH_GET_GS 0x1004
#macro ARCH_GET_CPUID 0x1011
#macro ARCH_SET_CPUID 0x1012
/**
 * @brief Invokes x86-64 arch_prctl.
 * @param operation Linux operation selector.
 * @param argument Operation-specific value or pointer cast to unsigned long.
 * @return Returns the syscall result, or -1 with errno set.
 */
int arch_prctl(int operation, unsigned long argument);
/**
 * @brief Invokes arch_prctl with a pointer argument.
 * @param operation Linux operation selector.
 * @param argument Pointer argument.
 * @return The result, or -1 with errno set.
 */
int arch_prctl_ptr(int operation, void* argument);
#endif
