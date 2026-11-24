	.text
	# Inputs: pollfd pointer, count, timeout in rdi, rsi, edx.
	# Output: ready count, zero on timeout, or negative Linux errno.
	.globl _crunch_poll
	.type _crunch_poll, @function
_crunch_poll:
	mov $7, %eax
	syscall
	ret
	.size _crunch_poll, .-_crunch_poll
	.section .note.GNU-stack,"",@progbits
