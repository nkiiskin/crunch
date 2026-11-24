	.text
	# Inputs: directory, path, mode in edi, rsi, edx.
	# Output: zero or -1 with errno set.
	.globl __crunch_mkdirat
	.type __crunch_mkdirat, @function
__crunch_mkdirat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_mkdirat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_mkdirat, .-__crunch_mkdirat
	.section .note.GNU-stack,"",@progbits
