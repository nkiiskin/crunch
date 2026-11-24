	.text
	# Inputs: process selector and signal. Output: zero or -1 with errno set.
	.globl __crunch_kill
	.type __crunch_kill, @function
__crunch_kill:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_kill
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_kill, .-__crunch_kill
	.section .note.GNU-stack,"",@progbits
