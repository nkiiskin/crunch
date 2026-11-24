	.text
	# Inputs: target, directory, path in rdi, esi, rdx.
	# Output: zero or negative Linux errno.
	.globl _crunch_symlinkat
	.type _crunch_symlinkat, @function
_crunch_symlinkat:
	mov $266, %eax
	syscall
	ret
	.size _crunch_symlinkat, .-_crunch_symlinkat
	.section .note.GNU-stack,"",@progbits
