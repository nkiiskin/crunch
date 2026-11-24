	.text
	# Inputs: descriptor in edi, buffer in rsi, count in rdx.
	# Output: byte count in rax, or -1 with errno set.
	.globl __crunch_write
	.type __crunch_write, @function
__crunch_write:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_write
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_write, .-__crunch_write
	.section .note.GNU-stack,"",@progbits
