	.text
	# Inputs: path in rdi, access mode in esi. Output: zero or negative Linux errno.
	.globl _crunch_access
	.type _crunch_access, @function
_crunch_access:
	mov $21, %eax
	syscall
	ret
	.size _crunch_access, .-_crunch_access
	.section .note.GNU-stack,"",@progbits
