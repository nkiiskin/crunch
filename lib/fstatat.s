	.text
	# Inputs: directory, path, buffer, flags in edi, rsi, rdx, ecx.
	# Output: zero or -1 with errno set.
	.globl __crunch_fstatat
	.type __crunch_fstatat, @function
__crunch_fstatat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_fstatat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_fstatat, .-__crunch_fstatat
	.section .note.GNU-stack,"",@progbits
