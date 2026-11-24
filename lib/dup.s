	.text
	# Input: descriptor. Output: new descriptor or -1 with errno set.
	.globl __crunch_dup
	.type __crunch_dup, @function
__crunch_dup:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_dup
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_dup, .-__crunch_dup
	.section .note.GNU-stack,"",@progbits
