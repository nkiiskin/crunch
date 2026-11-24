	.text
	# Inputs: stream in rdi, offset in rsi, whence in edx. Output: zero or -1 with errno set.
	.globl __crunch_fseek
	.type __crunch_fseek, @function
__crunch_fseek:
	push %r12
	test %rdi, %rdi
	jz .Lfseek_invalid
	mov %rdi, %r12
	cmp $1, %edx
	jne .Lfseek_call
	testl $4, 8(%r12)
	jz .Lfseek_call
	sub $1, %rsi
	jo .Lfseek_overflow
.Lfseek_call:
	mov (%rdi), %edi
	call __crunch_lseek
	cmp $-1, %rax
	je .Lfseek_failure
	andl $-65286, 8(%r12)
	xor %eax, %eax
	pop %r12
	ret
.Lfseek_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	jmp .Lfseek_failure
.Lfseek_overflow:
	call _crunch_errno_location
	movl $75, (%rax)
.Lfseek_failure:
	mov $-1, %eax
	pop %r12
	ret
	.size __crunch_fseek, .-__crunch_fseek
	.section .note.GNU-stack,"",@progbits
