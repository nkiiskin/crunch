	.text
	# Inputs: address, length, vector. Output: zero or -1.
	.globl __crunch_mincore
	.type __crunch_mincore, @function
__crunch_mincore:
	mov $27, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mincore, .-__crunch_mincore
	.section .note.GNU-stack,"",@progbits
