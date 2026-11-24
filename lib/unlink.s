	.text
	# Input: pathname in rdi. Output: zero or -1 with errno set.
	.globl __crunch_unlink
	.type __crunch_unlink, @function
__crunch_unlink:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_unlink
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_unlink, .-__crunch_unlink
	.section .note.GNU-stack,"",@progbits
