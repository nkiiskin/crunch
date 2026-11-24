	.text
	# Input: descriptor in edi. Output: new descriptor or negative Linux errno.
	.globl _crunch_dup
	.type _crunch_dup, @function
_crunch_dup:
	mov $32, %eax
	syscall
	ret
	.size _crunch_dup, .-_crunch_dup
	.section .note.GNU-stack,"",@progbits
