	.text
	# Inputs: buffer. Output: result or -1 with errno set.
	.globl __crunch_times
	.type __crunch_times, @function
__crunch_times:
	mov $100, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_times, .-__crunch_times

	# Inputs: request, process, address, data. Output: result or -1 with errno set.
	.globl __crunch_ptrace
	.type __crunch_ptrace, @function
__crunch_ptrace:
	mov %rcx, %r10
	mov $101, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_ptrace, .-__crunch_ptrace

	# Inputs: request, process, address, integer data. Output: result or -1 with errno set.
	.globl __crunch_ptrace_value
	.type __crunch_ptrace_value, @function
__crunch_ptrace_value:
	jmp __crunch_ptrace
	.size __crunch_ptrace_value, .-__crunch_ptrace_value

	# Inputs: none. Output: result.
	.globl __crunch_getuid
	.type __crunch_getuid, @function
__crunch_getuid:
	mov $102, %eax
	syscall
	ret
	.size __crunch_getuid, .-__crunch_getuid

	# Inputs: type, buffer, length. Output: result or -1 with errno set.
	.globl __crunch_klogctl
	.type __crunch_klogctl, @function
__crunch_klogctl:
	mov $103, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_klogctl, .-__crunch_klogctl

	# Inputs: none. Output: result.
	.globl __crunch_getgid
	.type __crunch_getgid, @function
__crunch_getgid:
	mov $104, %eax
	syscall
	ret
	.size __crunch_getgid, .-__crunch_getgid

	# Inputs: user. Output: result or -1 with errno set.
	.globl __crunch_setuid
	.type __crunch_setuid, @function
__crunch_setuid:
	mov $105, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setuid, .-__crunch_setuid

	# Inputs: group. Output: result or -1 with errno set.
	.globl __crunch_setgid
	.type __crunch_setgid, @function
__crunch_setgid:
	mov $106, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setgid, .-__crunch_setgid

	# Inputs: none. Output: result.
	.globl __crunch_geteuid
	.type __crunch_geteuid, @function
__crunch_geteuid:
	mov $107, %eax
	syscall
	ret
	.size __crunch_geteuid, .-__crunch_geteuid

	# Inputs: none. Output: result.
	.globl __crunch_getegid
	.type __crunch_getegid, @function
__crunch_getegid:
	mov $108, %eax
	syscall
	ret
	.size __crunch_getegid, .-__crunch_getegid

	# Inputs: process, group. Output: result or -1 with errno set.
	.globl __crunch_setpgid
	.type __crunch_setpgid, @function
__crunch_setpgid:
	mov $109, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setpgid, .-__crunch_setpgid

	# Inputs: none. Output: result.
	.globl __crunch_getppid
	.type __crunch_getppid, @function
__crunch_getppid:
	mov $110, %eax
	syscall
	ret
	.size __crunch_getppid, .-__crunch_getppid

	# Inputs: none. Output: result.
	.globl __crunch_getpgrp
	.type __crunch_getpgrp, @function
__crunch_getpgrp:
	mov $111, %eax
	syscall
	ret
	.size __crunch_getpgrp, .-__crunch_getpgrp

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_setsid
	.type __crunch_setsid, @function
__crunch_setsid:
	mov $112, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setsid, .-__crunch_setsid

	# Inputs: real, effective. Output: result or -1 with errno set.
	.globl __crunch_setreuid
	.type __crunch_setreuid, @function
__crunch_setreuid:
	mov $113, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setreuid, .-__crunch_setreuid

	# Inputs: real, effective. Output: result or -1 with errno set.
	.globl __crunch_setregid
	.type __crunch_setregid, @function
__crunch_setregid:
	mov $114, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setregid, .-__crunch_setregid

	# Inputs: count, groups. Output: result or -1 with errno set.
	.globl __crunch_getgroups
	.type __crunch_getgroups, @function
__crunch_getgroups:
	mov $115, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getgroups, .-__crunch_getgroups

	# Inputs: count, groups. Output: result or -1 with errno set.
	.globl __crunch_setgroups
	.type __crunch_setgroups, @function
__crunch_setgroups:
	mov $116, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setgroups, .-__crunch_setgroups

	# Inputs: real, effective, saved. Output: result or -1 with errno set.
	.globl __crunch_setresuid
	.type __crunch_setresuid, @function
__crunch_setresuid:
	mov $117, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setresuid, .-__crunch_setresuid

	# Inputs: real, effective, saved. Output: result or -1 with errno set.
	.globl __crunch_getresuid
	.type __crunch_getresuid, @function
__crunch_getresuid:
	mov $118, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getresuid, .-__crunch_getresuid

	# Inputs: real, effective, saved. Output: result or -1 with errno set.
	.globl __crunch_setresgid
	.type __crunch_setresgid, @function
__crunch_setresgid:
	mov $119, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setresgid, .-__crunch_setresgid

	# Inputs: real, effective, saved. Output: result or -1 with errno set.
	.globl __crunch_getresgid
	.type __crunch_getresgid, @function
__crunch_getresgid:
	mov $120, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getresgid, .-__crunch_getresgid

	# Inputs: process. Output: result or -1 with errno set.
	.globl __crunch_getpgid
	.type __crunch_getpgid, @function
__crunch_getpgid:
	mov $121, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getpgid, .-__crunch_getpgid

	# Inputs: user. Output: result.
	.globl __crunch_setfsuid
	.type __crunch_setfsuid, @function
__crunch_setfsuid:
	mov $122, %eax
	syscall
	ret
	.size __crunch_setfsuid, .-__crunch_setfsuid

	# Inputs: group. Output: result.
	.globl __crunch_setfsgid
	.type __crunch_setfsgid, @function
__crunch_setfsgid:
	mov $123, %eax
	syscall
	ret
	.size __crunch_setfsgid, .-__crunch_setfsgid

	# Inputs: process. Output: result or -1 with errno set.
	.globl __crunch_getsid
	.type __crunch_getsid, @function
__crunch_getsid:
	mov $124, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getsid, .-__crunch_getsid

	# Inputs: header, data. Output: result or -1 with errno set.
	.globl __crunch_capget
	.type __crunch_capget, @function
__crunch_capget:
	mov $125, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_capget, .-__crunch_capget

	# Inputs: header, data. Output: result or -1 with errno set.
	.globl __crunch_capset
	.type __crunch_capset, @function
__crunch_capset:
	mov $126, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_capset, .-__crunch_capset

	# Inputs: set. Output: result or -1 with errno set.
	.globl __crunch_sigpending
	.type __crunch_sigpending, @function
__crunch_sigpending:
	mov $8, %esi
	mov $127, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sigpending, .-__crunch_sigpending

	# Inputs: set, information, timeout. Output: result or -1 with errno set.
	.globl __crunch_sigtimedwait
	.type __crunch_sigtimedwait, @function
__crunch_sigtimedwait:
	mov $8, %r10
	mov $128, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sigtimedwait, .-__crunch_sigtimedwait

	# Inputs: process, signal, information. Output: result or -1 with errno set.
	.globl __crunch_rt_sigqueueinfo
	.type __crunch_rt_sigqueueinfo, @function
__crunch_rt_sigqueueinfo:
	mov $129, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_rt_sigqueueinfo, .-__crunch_rt_sigqueueinfo

	# Inputs: mask. Output: result or -1 with errno set.
	.globl __crunch_sigsuspend
	.type __crunch_sigsuspend, @function
__crunch_sigsuspend:
	mov $8, %esi
	mov $130, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sigsuspend, .-__crunch_sigsuspend

	# Inputs: stack, previous. Output: result or -1 with errno set.
	.globl __crunch_sigaltstack
	.type __crunch_sigaltstack, @function
__crunch_sigaltstack:
	mov $131, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sigaltstack, .-__crunch_sigaltstack

	# Inputs: path, times. Output: result or -1 with errno set.
	.globl __crunch_utime
	.type __crunch_utime, @function
__crunch_utime:
	mov $132, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_utime, .-__crunch_utime

	# Inputs: path, mode, device. Output: result or -1 with errno set.
	.globl __crunch_mknod
	.type __crunch_mknod, @function
__crunch_mknod:
	mov $133, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mknod, .-__crunch_mknod

	# Inputs: path. Output: result or -1 with errno set.
	.globl __crunch_uselib
	.type __crunch_uselib, @function
__crunch_uselib:
	mov $134, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_uselib, .-__crunch_uselib

	# Inputs: persona. Output: result or -1 with errno set.
	.globl __crunch_personality
	.type __crunch_personality, @function
__crunch_personality:
	mov $135, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_personality, .-__crunch_personality

	# Inputs: device, buffer. Output: result or -1 with errno set.
	.globl __crunch_ustat
	.type __crunch_ustat, @function
__crunch_ustat:
	mov $136, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_ustat, .-__crunch_ustat

	# Inputs: path, buffer. Output: result or -1 with errno set.
	.globl __crunch_statfs
	.type __crunch_statfs, @function
__crunch_statfs:
	mov $137, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_statfs, .-__crunch_statfs

	# Inputs: descriptor, buffer. Output: result or -1 with errno set.
	.globl __crunch_fstatfs
	.type __crunch_fstatfs, @function
__crunch_fstatfs:
	mov $138, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fstatfs, .-__crunch_fstatfs

	# Inputs: option, argument1, argument2. Output: result or -1 with errno set.
	.globl __crunch_crunch_sysfs
	.type __crunch_crunch_sysfs, @function
__crunch_crunch_sysfs:
	mov $139, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_sysfs, .-__crunch_crunch_sysfs

	# Input: filesystem name. Output: index or -1 with errno set.
	.globl __crunch_crunch_sysfs_index
	.type __crunch_crunch_sysfs_index, @function
__crunch_crunch_sysfs_index:
	mov %rdi, %rsi
	mov $1, %edi
	mov $139, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_sysfs_index, .-__crunch_crunch_sysfs_index

	# Inputs: index, output buffer. Output: zero or -1 with errno set.
	.globl __crunch_crunch_sysfs_name
	.type __crunch_crunch_sysfs_name, @function
__crunch_crunch_sysfs_name:
	mov %rsi, %rdx
	mov %edi, %esi
	mov $2, %edi
	mov $139, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_sysfs_name, .-__crunch_crunch_sysfs_name

	.section .note.GNU-stack,"",@progbits
