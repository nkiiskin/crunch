	.text
	# Input: none. Output: anonymous read/write stream or NULL with errno set.
	.globl __crunch_tmpfile
	.type __crunch_tmpfile, @function
__crunch_tmpfile:
	push %r12
	push %r13
	sub $8, %rsp
	mov $16, %edi
	call __crunch_malloc
	test %rax, %rax
	jz .Ldone
	mov %rax, %r12
	lea .Lname(%rip), %rdi
	mov $1, %esi
	mov $319, %eax
	syscall
	cmp $-4095, %rax
	jae .Lfailed
	mov %eax, (%r12)
	movl $2, 4(%r12)
	movl $0, 8(%r12)
	movl $1, 12(%r12)
	mov %r12, %rax
	jmp .Ldone
.Lfailed:
	neg %eax
	mov %eax, %r13d
	mov %r12, %rdi
	call __crunch_free
	call _crunch_errno_location
	mov %r13d, (%rax)
	xor %eax, %eax
.Ldone:
	add $8, %rsp
	pop %r13
	pop %r12
	ret
	.size __crunch_tmpfile, .-__crunch_tmpfile
	.section .rodata
.Lname:
	.asciz "crunch-tmpfile"
	.section .note.GNU-stack,"",@progbits
