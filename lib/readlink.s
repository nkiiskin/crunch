	.text
	# Inputs: path, buffer, byte count in rdi, rsi, rdx.
	# Output: byte count or -1 with errno set.
	.globl __crunch_readlink
	.type __crunch_readlink, @function
__crunch_readlink:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_readlink
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_readlink, .-__crunch_readlink
	.section .note.GNU-stack,"",@progbits
