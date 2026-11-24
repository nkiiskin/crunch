	.text
	# Inputs: old directory, old path, new directory, new path in edi, rsi, edx, rcx.
	# Output: zero or negative Linux errno.
	.globl _crunch_renameat
	.type _crunch_renameat, @function
_crunch_renameat:
	mov %rcx, %r10
	mov $264, %eax
	syscall
	ret
	.size _crunch_renameat, .-_crunch_renameat
	.section .note.GNU-stack,"",@progbits
