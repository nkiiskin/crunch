	.text
	# Inputs: pathname in rdi, mode in esi. Output: zero or -1 with errno set.
	.globl __crunch_mkdir
	.type __crunch_mkdir, @function
__crunch_mkdir:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_mkdir
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_mkdir, .-__crunch_mkdir
	.section .note.GNU-stack,"",@progbits
