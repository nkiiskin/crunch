	.text
	# Inputs: buffer in rdi, capacity in esi, stream in rdx. Output: buffer or null.
	.globl __crunch_fgets
	.type __crunch_fgets, @function
__crunch_fgets:
	push %r12
	push %r13
	push %r14
	push %r15
	sub $8, %rsp
	mov %rdi, %r12
	mov %rdx, %r13
	mov %esi, %r14d
	xor %r15d, %r15d
	test %r12, %r12
	jz .Lfgets_invalid
	test %r13, %r13
	jz .Lfgets_invalid
	test %r14d, %r14d
	jle .Lfgets_invalid
	sub $1, %r14d
.Lfgets_read:
	cmp %r14d, %r15d
	jae .Lfgets_success
	mov %r13, %rdi
	call __crunch_fgetc
	cmp $-1, %eax
	je .Lfgets_stop
	mov %al, (%r12,%r15)
	inc %r15
	cmp $10, %eax
	jne .Lfgets_read
.Lfgets_success:
	movb $0, (%r12,%r15)
	mov %r12, %rax
	jmp .Lfgets_done
.Lfgets_stop:
	testl $1, 8(%r13)
	jz .Lfgets_failed
	test %r15, %r15
	jnz .Lfgets_success
.Lfgets_failed:
	xor %eax, %eax
	jmp .Lfgets_done
.Lfgets_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	test %r13, %r13
	jz .Lfgets_failed
	orl $2, 8(%r13)
	jmp .Lfgets_failed
.Lfgets_done:
	add $8, %rsp
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	ret
	.size __crunch_fgets, .-__crunch_fgets
	.section .note.GNU-stack,"",@progbits
