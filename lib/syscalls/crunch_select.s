	.text
	# Inputs: nfds, read, write, except, timeout in edi, rsi, rdx, rcx, r8.
	# Output: ready count, zero on timeout, or negative Linux errno.
	.globl _crunch_select
	.type _crunch_select, @function
_crunch_select:
	mov %rcx, %r10
	mov $23, %eax
	syscall
	ret
	.size _crunch_select, .-_crunch_select
	.section .note.GNU-stack,"",@progbits
