	.text
	# Inputs: directory, path, mode in edi, rsi, edx.
	# Output: zero or negative Linux errno.
	.globl _crunch_mkdirat
	.type _crunch_mkdirat, @function
_crunch_mkdirat:
	mov $258, %eax
	syscall
	ret
	.size _crunch_mkdirat, .-_crunch_mkdirat
	.section .note.GNU-stack,"",@progbits
