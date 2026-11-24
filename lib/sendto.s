	.text
	# Inputs: socket, buffer, length, flags, address, address length.
	# Output: sent byte count or -1.
	.globl __crunch_sendto
	.type __crunch_sendto, @function
__crunch_sendto:
	mov %rcx, %r10
	mov $44, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sendto, .-__crunch_sendto
	.section .note.GNU-stack,"",@progbits
