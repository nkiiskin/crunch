	.text
	# Inputs: descriptor in edi, stat buffer in rsi. Output: zero or negative Linux errno.
	.globl _crunch_fstat
	.type _crunch_fstat, @function
_crunch_fstat:
	mov $5, %eax
	syscall
	ret
	.size _crunch_fstat, .-_crunch_fstat
	.section .note.GNU-stack,"",@progbits
