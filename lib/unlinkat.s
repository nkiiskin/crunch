	.text
	# Inputs: directory, path, flags in edi, rsi, edx.
	# Output: zero or -1 with errno set.
	.globl __crunch_unlinkat
	.type __crunch_unlinkat, @function
__crunch_unlinkat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_unlinkat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_unlinkat, .-__crunch_unlinkat
	.section .note.GNU-stack,"",@progbits
