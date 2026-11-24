	.text
	# Input: path. Output: zero or -1.
	.globl __crunch_chdir
	.type __crunch_chdir, @function
__crunch_chdir:
	mov $80, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_chdir, .-__crunch_chdir

	# Input: descriptor. Output: zero or -1.
	.globl __crunch_fchdir
	.type __crunch_fchdir, @function
__crunch_fchdir:
	mov $81, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fchdir, .-__crunch_fchdir

	# Inputs: path, mode. Output: descriptor or -1.
	.globl __crunch_creat
	.type __crunch_creat, @function
__crunch_creat:
	mov $85, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_creat, .-__crunch_creat

	# Inputs: old path, new path. Output: zero or -1.
	.globl __crunch_link
	.type __crunch_link, @function
__crunch_link:
	mov $86, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_link, .-__crunch_link

	# Inputs: target, link path. Output: zero or -1.
	.globl __crunch_symlink
	.type __crunch_symlink, @function
__crunch_symlink:
	mov $88, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_symlink, .-__crunch_symlink

	# Inputs: path, owner, group. Output: zero or -1.
	.globl __crunch_chown
	.type __crunch_chown, @function
__crunch_chown:
	mov $92, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_chown, .-__crunch_chown

	# Inputs: descriptor, owner, group. Output: zero or -1.
	.globl __crunch_fchown
	.type __crunch_fchown, @function
__crunch_fchown:
	mov $93, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fchown, .-__crunch_fchown

	# Inputs: path, owner, group. Output: zero or -1.
	.globl __crunch_lchown
	.type __crunch_lchown, @function
__crunch_lchown:
	mov $94, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_lchown, .-__crunch_lchown

	# Input: new mask. Output: previous mask.
	.globl __crunch_umask
	.type __crunch_umask, @function
__crunch_umask:
	mov $95, %eax
	syscall
	ret
	.size __crunch_umask, .-__crunch_umask

	# Inputs: timeval and timezone pointers. Output: zero or -1.
	.globl __crunch_gettimeofday
	.type __crunch_gettimeofday, @function
__crunch_gettimeofday:
	mov $96, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_gettimeofday, .-__crunch_gettimeofday

	# Inputs: resource, limits pointer. Output: zero or -1.
	.globl __crunch_getrlimit
	.type __crunch_getrlimit, @function
__crunch_getrlimit:
	mov $97, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getrlimit, .-__crunch_getrlimit

	# Inputs: selector, usage pointer. Output: zero or -1.
	.globl __crunch_getrusage
	.type __crunch_getrusage, @function
__crunch_getrusage:
	mov $98, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getrusage, .-__crunch_getrusage

	# Input: sysinfo pointer. Output: zero or -1.
	.globl __crunch_sysinfo
	.type __crunch_sysinfo, @function
__crunch_sysinfo:
	mov $99, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sysinfo, .-__crunch_sysinfo
	.section .note.GNU-stack,"",@progbits
