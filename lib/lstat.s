	.text
	# Inputs: path in rdi, stat buffer in rsi. Output: zero or -1 with errno set.
	.globl __crunch_lstat
	.type __crunch_lstat, @function
__crunch_lstat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_lstat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_lstat, .-__crunch_lstat
	.section .note.GNU-stack,"",@progbits
