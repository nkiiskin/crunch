	.text
	# Inputs: descriptor in edi, buffer in rsi, count in rdx.
	# Output: byte count in rax, or -1 with errno set.
	.globl __crunch_read
	.type __crunch_read, @function
__crunch_read:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_read
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_read, .-__crunch_read
	.section .note.GNU-stack,"",@progbits
