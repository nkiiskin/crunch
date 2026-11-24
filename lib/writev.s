	.text
	# Inputs: descriptor, vectors, count. Output: byte count or -1.
	.globl __crunch_writev
	.type __crunch_writev, @function
__crunch_writev:
	mov $20, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_writev, .-__crunch_writev
	.section .note.GNU-stack,"",@progbits
