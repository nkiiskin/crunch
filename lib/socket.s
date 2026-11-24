	.text
	# Inputs: domain, type, protocol. Output: descriptor or -1.
	.globl __crunch_socket
	.type __crunch_socket, @function
__crunch_socket:
	mov $41, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_socket, .-__crunch_socket
	.section .note.GNU-stack,"",@progbits
