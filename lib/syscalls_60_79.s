	.text
	# Inputs: PID, status, options, usage. Output: PID or -1.
	.globl __crunch_wait4
	.type __crunch_wait4, @function
__crunch_wait4:
	mov %rcx, %r10
	mov $61, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_wait4, .-__crunch_wait4

	# Inputs: key, count, flags. Output: identifier or -1.
	.globl __crunch_semget
	.type __crunch_semget, @function
__crunch_semget:
	mov $64, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_semget, .-__crunch_semget

	# Inputs: identifier, operations, count. Output: zero or -1.
	.globl __crunch_semop
	.type __crunch_semop, @function
__crunch_semop:
	mov $65, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_semop, .-__crunch_semop

	# Inputs: identifier, number, command, argument. Output: result or -1.
	.globl __crunch_semctl
	.type __crunch_semctl, @function
	.globl __crunch_semctl_value
	.type __crunch_semctl_value, @function
__crunch_semctl:
__crunch_semctl_value:
	mov %rcx, %r10
	mov $66, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_semctl, .-__crunch_semctl
	.size __crunch_semctl_value, .-__crunch_semctl_value

	# Inputs: key, flags. Output: identifier or -1.
	.globl __crunch_msgget
	.type __crunch_msgget, @function
__crunch_msgget:
	mov $68, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_msgget, .-__crunch_msgget

	# Inputs: identifier, message, size, flags. Output: zero or -1.
	.globl __crunch_msgsnd
	.type __crunch_msgsnd, @function
__crunch_msgsnd:
	mov %rcx, %r10
	mov $69, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_msgsnd, .-__crunch_msgsnd

	# Inputs: identifier, message, size, type, flags. Output: byte count or -1.
	.globl __crunch_msgrcv
	.type __crunch_msgrcv, @function
__crunch_msgrcv:
	mov %rcx, %r10
	mov $70, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_msgrcv, .-__crunch_msgrcv

	# Inputs: identifier, command, buffer. Output: result or -1.
	.globl __crunch_msgctl
	.type __crunch_msgctl, @function
__crunch_msgctl:
	mov $71, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_msgctl, .-__crunch_msgctl

	# Inputs: descriptor, command, argument. Output: result or -1.
	.globl __crunch_fcntl
	.type __crunch_fcntl, @function
	.globl __crunch_fcntl_ptr
	.type __crunch_fcntl_ptr, @function
__crunch_fcntl:
__crunch_fcntl_ptr:
	mov $72, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fcntl, .-__crunch_fcntl
	.size __crunch_fcntl_ptr, .-__crunch_fcntl_ptr

	# Inputs: descriptor, operation. Output: zero or -1.
	.globl __crunch_flock
	.type __crunch_flock, @function
__crunch_flock:
	mov $73, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_flock, .-__crunch_flock

	# Input: descriptor. Output: zero or -1.
	.globl __crunch_fsync
	.type __crunch_fsync, @function
__crunch_fsync:
	mov $74, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fsync, .-__crunch_fsync

	# Input: descriptor. Output: zero or -1.
	.globl __crunch_fdatasync
	.type __crunch_fdatasync, @function
__crunch_fdatasync:
	mov $75, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fdatasync, .-__crunch_fdatasync

	# Inputs: path, length. Output: zero or -1.
	.globl __crunch_truncate
	.type __crunch_truncate, @function
__crunch_truncate:
	mov $76, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_truncate, .-__crunch_truncate

	# Inputs: descriptor, length. Output: zero or -1.
	.globl __crunch_ftruncate
	.type __crunch_ftruncate, @function
__crunch_ftruncate:
	mov $77, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_ftruncate, .-__crunch_ftruncate

	# Inputs: descriptor, buffer, size. Output: byte count or -1.
	.globl __crunch_getdents
	.type __crunch_getdents, @function
__crunch_getdents:
	mov $78, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getdents, .-__crunch_getdents
	.section .note.GNU-stack,"",@progbits
