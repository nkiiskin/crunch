	.text
	# Inputs: socket, direction. Output: zero or -1.
	.globl __crunch_shutdown
	.type __crunch_shutdown, @function
__crunch_shutdown:
	mov $48, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_shutdown, .-__crunch_shutdown
	.section .note.GNU-stack,"",@progbits
