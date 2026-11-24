	.text
	# Inputs: old pathname in rdi, new pathname in rsi. Output: zero or negative Linux errno.
	.globl _crunch_rename
	.type _crunch_rename, @function
_crunch_rename:
	mov $82, %eax
	syscall
	ret
	.size _crunch_rename, .-_crunch_rename
	.section .note.GNU-stack,"",@progbits
