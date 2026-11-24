	.text
	# Inputs: directory, path, buffer, flags in edi, rsi, rdx, ecx.
	# Output: zero or negative Linux errno.
	.globl _crunch_fstatat
	.type _crunch_fstatat, @function
_crunch_fstatat:
	mov %rcx, %r10
	mov $262, %eax
	syscall
	ret
	.size _crunch_fstatat, .-_crunch_fstatat
	.section .note.GNU-stack,"",@progbits
