	.text
	# Input: mmap arguments. Output: MAP_FAILED with EACCES.
	.globl _crunch_mmap
	.type _crunch_mmap, @function
_crunch_mmap:
	movl $13, errno(%rip)
	mov $-1, %rax
	ret
	.size _crunch_mmap, .-_crunch_mmap
	.section .note.GNU-stack,"",@progbits
