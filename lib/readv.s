	.text
	# Inputs: descriptor, vector array, count. Output: byte count or -1.
	.globl __crunch_readv
	.type __crunch_readv, @function
__crunch_readv:
	mov $19, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_readv, .-__crunch_readv
	.section .note.GNU-stack,"",@progbits
