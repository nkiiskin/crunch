	.text
	# Input: NUL-terminated string. Output: allocated copy or NULL.
	.globl __crunch_strdup
	.type __crunch_strdup, @function
__crunch_strdup:
	push %r12
	push %r13
	sub $8, %rsp
	mov %rdi, %r12
	call __crunch_strlen
	lea 1(%rax), %r13
	mov %r13, %rdi
	call __crunch_malloc
	test %rax, %rax
	jz .Ldone
	mov %rax, %rdi
	mov %r12, %rsi
	mov %r13, %rdx
	call __crunch_memcpy
.Ldone:
	add $8, %rsp
	pop %r13
	pop %r12
	ret
	.size __crunch_strdup, .-__crunch_strdup
	.section .note.GNU-stack,"",@progbits
