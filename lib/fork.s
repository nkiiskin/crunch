	.text
	# Input: none. Output: child PID, zero in child, or -1 with errno set.
	.globl __crunch_fork
	.type __crunch_fork, @function
__crunch_fork:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_fork
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_fork, .-__crunch_fork
	.section .note.GNU-stack,"",@progbits
