	.text
	# Input: pointer to two int descriptors. Output: zero or -1 with errno set.
	.globl __crunch_pipe
	.type __crunch_pipe, @function
__crunch_pipe:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_pipe
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_pipe, .-__crunch_pipe
	.section .note.GNU-stack,"",@progbits
