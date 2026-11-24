	.text
	# Inputs: buffer pointer, capacity pointer, and stream. Output: byte count or -1.
	.globl __crunch_getline
	.type __crunch_getline, @function
__crunch_getline:
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	mov %rdi, %r12
	mov %rsi, %r13
	mov %rdx, %r14
	xor %ebx, %ebx
	test %r12, %r12
	je .Lgetline_invalid
	test %r13, %r13
	je .Lgetline_invalid
	test %r14, %r14
	je .Lgetline_invalid
	mov (%r12), %r15
	test %r15, %r15
	je .Lgetline_initial
	cmpq $0, (%r13)
	jne .Lgetline_ready
.Lgetline_initial:
	mov %r15, %rdi
	mov $128, %esi
	call _crunch_realloc
	test %rax, %rax
	je .Lgetline_failure
	mov %rax, %r15
	mov %rax, (%r12)
	movq $128, (%r13)
.Lgetline_ready:
	movb $0, (%r15)
.Lgetline_read:
	lea 1(%rbx), %rax
	cmp (%r13), %rax
	jb .Lgetline_character
	mov (%r13), %rsi
	cmp $128, %rsi
	jae .Lgetline_double
	mov $128, %esi
	jmp .Lgetline_grow
.Lgetline_double:
	add %rsi, %rsi
	jc .Lgetline_overflow
	js .Lgetline_overflow
.Lgetline_grow:
	mov %r15, %rdi
	push %rsi
	sub $8, %rsp
	call _crunch_realloc
	add $8, %rsp
	pop %rsi
	test %rax, %rax
	je .Lgetline_failure
	mov %rax, %r15
	mov %rax, (%r12)
	mov %rsi, (%r13)
.Lgetline_character:
	mov %r14, %rdi
	call __crunch_fgetc
	cmp $-1, %eax
	je .Lgetline_eof
	mov %al, (%r15,%rbx)
	inc %rbx
	movb $0, (%r15,%rbx)
	cmp $10, %eax
	jne .Lgetline_read
	jmp .Lgetline_success
.Lgetline_eof:
	mov %r14, %rdi
	call __crunch_ferror
	test %eax, %eax
	jne .Lgetline_failure
	test %rbx, %rbx
	je .Lgetline_failure
.Lgetline_success:
	mov %rbx, %rax
	jmp .Lgetline_done
.Lgetline_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	jmp .Lgetline_failure
.Lgetline_overflow:
	call _crunch_errno_location
	movl $75, (%rax)
.Lgetline_failure:
	mov $-1, %rax
.Lgetline_done:
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	ret
	.size __crunch_getline, .-__crunch_getline
	.section .note.GNU-stack,"",@progbits
