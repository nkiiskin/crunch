	.text
	# Inputs: directory, path, buffer, count in edi, rsi, rdx, rcx.
	# Output: byte count or -1 with errno set.
	.globl __crunch_readlinkat
	.type __crunch_readlinkat, @function
__crunch_readlinkat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_readlinkat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_readlinkat, .-__crunch_readlinkat
	.section .note.GNU-stack,"",@progbits
