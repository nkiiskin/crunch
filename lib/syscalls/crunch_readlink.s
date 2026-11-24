	.text
	# Inputs: path, buffer, byte count in rdi, rsi, rdx.
	# Output: byte count or negative Linux errno.
	.globl _crunch_readlink
	.type _crunch_readlink, @function
_crunch_readlink:
	mov $89, %eax
	syscall
	ret
	.size _crunch_readlink, .-_crunch_readlink
	.section .note.GNU-stack,"",@progbits
