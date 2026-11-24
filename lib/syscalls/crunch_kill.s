	.text
	# Inputs: process selector in edi, signal in esi. Output: zero or negative errno.
	.globl _crunch_kill
	.type _crunch_kill, @function
_crunch_kill:
	mov $62, %eax
	syscall
	ret
	.size _crunch_kill, .-_crunch_kill
	.section .note.GNU-stack,"",@progbits
