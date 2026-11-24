#ifndef CRUNCH_SYS_REBOOT_H
#define CRUNCH_SYS_REBOOT_H
#macro LINUX_REBOOT_CMD_RESTART 0x01234567
#macro LINUX_REBOOT_CMD_HALT 0xcdef0123U
#macro LINUX_REBOOT_CMD_POWER_OFF 0x4321fedc
#macro LINUX_REBOOT_CMD_RESTART2 0xa1b2c3d4U
/**
 * @brief Invokes Linux reboot with the required magic values and an optional restart argument.
 * @param command Linux operation command.
 * @param argument Operation-specific value or pointer cast to unsigned long.
 * @return Returns the syscall result, or -1 with errno set.
 */
int crunch_reboot(unsigned int command, const char* argument);
#endif
