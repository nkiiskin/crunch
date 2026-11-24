	.text
	# Inputs: directory, path, flags, mode in edi, rsi, edx, ecx.
	# Output: descriptor or -1 with errno set.
	.globl __crunch_openat
	.type __crunch_openat, @function
__crunch_openat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_openat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_openat, .-__crunch_openat
	.section .note.GNU-stack,"",@progbits
