	.text
	# Inputs: process selector, status pointer, options. Output: PID or -1.
	.globl __crunch_waitpid
	.type __crunch_waitpid, @function
__crunch_waitpid:
	xor %r10d, %r10d
	mov $61, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_waitpid, .-__crunch_waitpid
	.section .note.GNU-stack,"",@progbits
