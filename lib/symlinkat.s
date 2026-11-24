	.text
	# Inputs: target, directory, path in rdi, esi, rdx.
	# Output: zero or -1 with errno set.
	.globl __crunch_symlinkat
	.type __crunch_symlinkat, @function
__crunch_symlinkat:
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_symlinkat
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
	.size __crunch_symlinkat, .-__crunch_symlinkat
	.section .note.GNU-stack,"",@progbits
