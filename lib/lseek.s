	.text
	# Inputs: descriptor in edi, offset in rsi, whence in edx.
	# Output: resulting offset in rax, or -1 with errno set.
	.globl __crunch_lseek
	.type __crunch_lseek, @function
__crunch_lseek:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_lseek
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_lseek, .-__crunch_lseek
	.section .note.GNU-stack,"",@progbits
