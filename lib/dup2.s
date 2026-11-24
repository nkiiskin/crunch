	.text
	# Inputs: old and new descriptors. Output: descriptor or -1 with errno set.
	.globl __crunch_dup2
	.type __crunch_dup2, @function
__crunch_dup2:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_dup2
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_dup2, .-__crunch_dup2
	.section .note.GNU-stack,"",@progbits
