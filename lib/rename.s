	.text
	# Inputs: old pathname in rdi, new pathname in rsi. Output: zero or -1 with errno set.
	.globl __crunch_rename
	.type __crunch_rename, @function
__crunch_rename:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_rename
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_rename, .-__crunch_rename
	.section .note.GNU-stack,"",@progbits
