	.text
	# Inputs: descriptor, buffer, byte count in edi, rsi, rdx.
	# Output: byte count or negative Linux errno.
	.globl _crunch_getdents64
	.type _crunch_getdents64, @function
_crunch_getdents64:
	mov $217, %eax
	syscall
	ret
	.size _crunch_getdents64, .-_crunch_getdents64
	.section .note.GNU-stack,"",@progbits
