	.text
	# Input: none. Output: child PID, zero in child, or negative Linux errno.
	.globl _crunch_fork
	.type _crunch_fork, @function
_crunch_fork:
	mov $57, %eax
	syscall
	ret
	.size _crunch_fork, .-_crunch_fork
	.section .note.GNU-stack,"",@progbits
