	.text
	# Inputs: old address, old size, new size, flags, and optional new address
	# in rdi, rsi, rdx, ecx, and r8. Output: address or -1 with errno set.
	.globl _crunch_mremap
	.hidden _crunch_mremap
	.type _crunch_mremap, @function
_crunch_mremap:
	mov %rcx, %r10
	mov $25, %eax
	syscall
	jmp __crunch_syscall_result
	.size _crunch_mremap, .-_crunch_mremap
	.section .note.GNU-stack,"",@progbits
