	.text
	# Inputs: socket, address, length pointer. Output: zero or -1.
	.globl __crunch_getsockname
	.type __crunch_getsockname, @function
__crunch_getsockname:
	mov $51, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getsockname, .-__crunch_getsockname
	.section .note.GNU-stack,"",@progbits
