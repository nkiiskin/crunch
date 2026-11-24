	.text
	# Inputs: old and new descriptors in edi and esi. Output: descriptor or negative errno.
	.globl _crunch_dup2
	.type _crunch_dup2, @function
_crunch_dup2:
	mov $33, %eax
	syscall
	ret
	.size _crunch_dup2, .-_crunch_dup2
	.section .note.GNU-stack,"",@progbits
