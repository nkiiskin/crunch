	.text
	# Inputs: old directory, old path, new directory, new path in edi, rsi, edx, rcx.
	# Output: zero or -1 with errno set.
	.globl __crunch_renameat
	.type __crunch_renameat, @function
__crunch_renameat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_renameat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_renameat, .-__crunch_renameat
	.section .note.GNU-stack,"",@progbits
