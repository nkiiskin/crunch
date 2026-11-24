	.text
	# Inputs: path in rdi, flags in esi, mode in edx.
	# Output: descriptor in eax, or -1 with errno set.
	.globl __crunch_open
	.type __crunch_open, @function
__crunch_open:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_open
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_open, .-__crunch_open
	.section .note.GNU-stack,"",@progbits
