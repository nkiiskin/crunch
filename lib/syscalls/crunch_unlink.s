	.text
	# Input: pathname in rdi. Output: zero or negative Linux errno.
	.globl _crunch_unlink
	.type _crunch_unlink, @function
_crunch_unlink:
	mov $87, %eax
	syscall
	ret
	.size _crunch_unlink, .-_crunch_unlink
	.section .note.GNU-stack,"",@progbits
