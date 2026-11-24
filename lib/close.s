	.text
	# Input: descriptor in edi. Output: zero or -1 with errno set.
	.globl __crunch_close
	.type __crunch_close, @function
__crunch_close:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_close
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_close, .-__crunch_close
	.section .note.GNU-stack,"",@progbits
