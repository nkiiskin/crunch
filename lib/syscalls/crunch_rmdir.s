	.text
	# Input: pathname in rdi. Output: zero or negative Linux errno.
	.globl _crunch_rmdir
	.type _crunch_rmdir, @function
_crunch_rmdir:
	mov $84, %eax
	syscall
	ret
	.size _crunch_rmdir, .-_crunch_rmdir
	.section .note.GNU-stack,"",@progbits
