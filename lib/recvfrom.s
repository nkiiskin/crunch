	.text
	# Inputs: descriptor, buffer, length, flags, address, address length pointer.
	# Output: received byte count or -1.
	.globl __crunch_recvfrom
	.type __crunch_recvfrom, @function
__crunch_recvfrom:
	mov %rcx, %r10
	mov $45, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_recvfrom, .-__crunch_recvfrom
	.section .note.GNU-stack,"",@progbits
