	.text
	# Inputs: nfds, read, write, except, timeout. Output: ready count or -1 with errno.
	.globl __crunch_select
	.type __crunch_select, @function
__crunch_select:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_select
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_select, .-__crunch_select
	.section .note.GNU-stack,"",@progbits
