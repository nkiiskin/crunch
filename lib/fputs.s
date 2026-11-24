	.text
	# Inputs: NUL-terminated string in rdi, stream in rsi. Output: zero or EOF on error.
	.globl __crunch_fputs
	.type __crunch_fputs, @function
__crunch_fputs:
	push %r12
	push %r13
	push %r14
	mov %rdi, %r12
	mov %rsi, %r13
	test %r12, %r12
	jz .Lfputs_invalid
	test %r13, %r13
	jz .Lfputs_invalid
	call __crunch_strlen
	mov %rax, %r14
	mov %r12, %rdi
	mov $1, %esi
	mov %r14, %rdx
	mov %r13, %rcx
	call __crunch_fwrite
	cmp %r14, %rax
	jne .Lfputs_failure
	xor %eax, %eax
	jmp .Lfputs_done
.Lfputs_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	test %r13, %r13
	jz .Lfputs_failure
	orl $2, 8(%r13)
.Lfputs_failure:
	mov $-1, %eax
.Lfputs_done:
	pop %r14
	pop %r13
	pop %r12
	ret
	.size __crunch_fputs, .-__crunch_fputs
	.section .note.GNU-stack,"",@progbits
