	.data
	.globl remap_calls
remap_calls:
	.long 0
	.text
	# Input: mremap arguments. Output: MAP_FAILED with ENOMEM.
	.globl _crunch_mremap
	.type _crunch_mremap, @function
_crunch_mremap:
	addl $1, remap_calls(%rip)
	movl $12, errno(%rip)
	mov $-1, %rax
	ret
	.size _crunch_mremap, .-_crunch_mremap
	.section .note.GNU-stack,"",@progbits
