	.text
	# Inputs: path in rdi and stat buffer in rsi.
	# Output: zero or -1 with errno set.
	.globl __crunch_stat
	.type __crunch_stat, @function
__crunch_stat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_stat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_stat, .-__crunch_stat
	.section .note.GNU-stack,"",@progbits
