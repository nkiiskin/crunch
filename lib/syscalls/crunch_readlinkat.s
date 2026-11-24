	.text
	# Inputs: directory, path, buffer, count in edi, rsi, rdx, rcx.
	# Output: byte count or negative Linux errno.
	.globl _crunch_readlinkat
	.type _crunch_readlinkat, @function
_crunch_readlinkat:
	mov %rcx, %r10
	mov $267, %eax
	syscall
	ret
	.size _crunch_readlinkat, .-_crunch_readlinkat
	.section .note.GNU-stack,"",@progbits
