	.text
	# Inputs: address and length. Output: zero or -1 with errno set.
	.globl __crunch_munmap
	.type __crunch_munmap, @function
__crunch_munmap:
	jmp _crunch_munmap
	.size __crunch_munmap, .-__crunch_munmap
	.section .note.GNU-stack,"",@progbits
