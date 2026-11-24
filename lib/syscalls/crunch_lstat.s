	.text
	# Inputs: path in rdi, stat buffer in rsi. Output: zero or negative Linux errno.
	.globl _crunch_lstat
	.type _crunch_lstat, @function
_crunch_lstat:
	mov $6, %eax
	syscall
	ret
	.size _crunch_lstat, .-_crunch_lstat
	.section .note.GNU-stack,"",@progbits
