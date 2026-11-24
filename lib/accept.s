	.text
	# Inputs: descriptor, address, length pointer. Output: descriptor or -1.
	.globl __crunch_accept
	.type __crunch_accept, @function
__crunch_accept:
	mov $43, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_accept, .-__crunch_accept
	.section .note.GNU-stack,"",@progbits
