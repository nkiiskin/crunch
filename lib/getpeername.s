	.text
	# Inputs: socket, address, length pointer. Output: zero or -1.
	.globl __crunch_getpeername
	.type __crunch_getpeername, @function
__crunch_getpeername:
	mov $52, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getpeername, .-__crunch_getpeername
	.section .note.GNU-stack,"",@progbits
