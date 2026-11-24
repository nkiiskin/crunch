	.text
	# Inputs: buffer or null, size. Output: buffer or null with errno set.
	.globl __crunch_getcwd
	.type __crunch_getcwd, @function
__crunch_getcwd:
	push %r12
	push %r13
	push %r14
	mov %rdi, %r12
	mov %rsi, %r13
	xor %r14d, %r14d
	test %r12, %r12
	jz .Lallocate
	test %r13, %r13
	jz .Linvalid
	jmp .Lread
.Lallocate:
	mov $1, %r14d
	test %r13, %r13
	jnz .Lmalloc
	mov $2, %r14d
	mov $128, %r13d
.Lmalloc:
	mov %r13, %rdi
	call __crunch_malloc
	test %rax, %rax
	jz .Ldone
	mov %rax, %r12
.Lread:
	mov %r12, %rdi
	mov %r13, %rsi
	mov $79, %eax
	syscall
	cmp $-4095, %rax
	jae .Lerror
	cmpb $47, (%r12)
	jne .Lunreachable
	mov %r12, %rax
	jmp .Ldone
.Lunreachable:
	mov $-2, %rax
.Lerror:
	cmp $2, %r14d
	jne .Lrelease
	cmp $-34, %rax
	jne .Lrelease
	mov %r12, %rdi
	call __crunch_free
	add %r13, %r13
	jc .Lnomem
	jmp .Lmalloc
.Lrelease:
	mov %rax, %r13
	test %r14d, %r14d
	jz .Lseterror
	mov %r12, %rdi
	call __crunch_free
.Lseterror:
	mov %r13, %rax
	call __crunch_syscall_result
	xor %eax, %eax
	jmp .Ldone
.Linvalid:
	mov $-22, %r13
	jmp .Lseterror
.Lnomem:
	mov $-12, %r13
	jmp .Lseterror
.Ldone:
	pop %r14
	pop %r13
	pop %r12
	ret
	.size __crunch_getcwd, .-__crunch_getcwd
	.section .note.GNU-stack,"",@progbits
