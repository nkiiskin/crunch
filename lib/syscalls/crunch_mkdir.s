	.text
	# Inputs: pathname in rdi, mode in esi. Output: zero or negative Linux errno.
	.globl _crunch_mkdir
	.type _crunch_mkdir, @function
_crunch_mkdir:
	mov $83, %eax
	syscall
	ret
	.size _crunch_mkdir, .-_crunch_mkdir
	.section .note.GNU-stack,"",@progbits
