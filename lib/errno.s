	.bss
	.balign 4
	.globl errno
	.ifdef CRUNCH_SHARED
	.protected errno
	.endif
	.type errno, @object
	.size errno, 4
errno:
	.zero 4

	.text
	# Returns the address of the process-wide error number, initially zero.
	# Input: none. Output: pointer to writable int errno storage in rax.
	.globl __crunch___errno_location
	.type __crunch___errno_location, @function
	.globl _crunch_errno_location
	.hidden _crunch_errno_location
	.type _crunch_errno_location, @function
__crunch___errno_location:
_crunch_errno_location:
	lea errno(%rip), %rax
	ret
	.size __crunch___errno_location, .-__crunch___errno_location
	.size _crunch_errno_location, .-_crunch_errno_location

	# Converts raw Linux syscall errors to the standard-library error convention.
	# Input: raw syscall result in rax. Output: unchanged success result or -1;
	# on error, sets errno to the positive Linux error number.
	.globl __crunch_syscall_result
	.hidden __crunch_syscall_result
	.type __crunch_syscall_result, @function
__crunch_syscall_result:
	cmp $-4095, %rax
	jb .Lsuccess
	neg %eax
	mov %eax, errno(%rip)
	mov $-1, %rax
.Lsuccess:
	ret
	.size __crunch_syscall_result, .-__crunch_syscall_result
	.section .note.GNU-stack,"",@progbits
