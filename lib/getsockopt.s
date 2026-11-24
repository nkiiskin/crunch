	.text
	# Inputs: socket, level, option, value, length pointer. Output: zero or -1.
	.globl __crunch_getsockopt
	.type __crunch_getsockopt, @function
__crunch_getsockopt:
	mov %rcx, %r10
	mov $55, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getsockopt, .-__crunch_getsockopt
	.section .note.GNU-stack,"",@progbits
