	.text
	# Inputs: descriptor in edi, stat buffer in rsi. Output: zero or -1 with errno set.
	.globl __crunch_fstat
	.type __crunch_fstat, @function
__crunch_fstat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_fstat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_fstat, .-__crunch_fstat
	.section .note.GNU-stack,"",@progbits
