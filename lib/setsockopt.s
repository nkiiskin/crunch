	.text
	# Inputs: socket, level, option, value, length. Output: zero or -1.
	.globl __crunch_setsockopt
	.type __crunch_setsockopt, @function
__crunch_setsockopt:
	mov %rcx, %r10
	mov $54, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setsockopt, .-__crunch_setsockopt
	.section .note.GNU-stack,"",@progbits
