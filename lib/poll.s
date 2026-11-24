	.text
	# Inputs: pollfd pointer, count, timeout. Output: ready count or -1 with errno set.
	.globl __crunch_poll
	.type __crunch_poll, @function
__crunch_poll:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_poll
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_poll, .-__crunch_poll
	.section .note.GNU-stack,"",@progbits
