	.text
	# Input: pointer to two int descriptors in rdi. Output: zero or negative errno.
	.globl _crunch_pipe
	.type _crunch_pipe, @function
_crunch_pipe:
	mov $22, %eax
	syscall
	ret
	.size _crunch_pipe, .-_crunch_pipe
	.section .note.GNU-stack,"",@progbits
