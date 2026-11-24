	.text
	# Inputs: path and mode. Output: zero or -1 with errno set.
	.globl __crunch_access
	.type __crunch_access, @function
__crunch_access:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_access
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_access, .-__crunch_access
	.section .note.GNU-stack,"",@progbits
