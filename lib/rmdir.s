	.text
	# Input: pathname in rdi. Output: zero or -1 with errno set.
	.globl __crunch_rmdir
	.type __crunch_rmdir, @function
__crunch_rmdir:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_rmdir
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_rmdir, .-__crunch_rmdir
	.section .note.GNU-stack,"",@progbits
