	.text
	# Copies the NUL-terminated string at rsi to rdi.
	# Inputs: destination and source strings. Output: destination pointer in rax.
	.globl __crunch_strcpy
	.type __crunch_strcpy, @function
__crunch_strcpy:
	mov %rdi, %rax
	# Align source reads to avoid crossing an inaccessible page.
.Lalign:
	test $7, %rsi
	jz .Lwords
	movb (%rsi), %cl
	movb %cl, (%rdi)
	test %cl, %cl
	jz .Ldone
	inc %rsi
	inc %rdi
	jmp .Lalign
.Lwords:
	movabsq $0x0101010101010101, %r8
	movabsq $0x8080808080808080, %r9
.Lloop:
	mov (%rsi), %rdx
	mov %rdx, %rcx
	sub %r8, %rcx
	mov %rdx, %r10
	not %r10
	and %r10, %rcx
	test %r9, %rcx
	jnz .Ltail
	mov %rdx, (%rdi)
	add $8, %rsi
	add $8, %rdi
	jmp .Lloop
.Ltail:
	movb (%rsi), %cl
	movb %cl, (%rdi)
	inc %rsi
	inc %rdi
	test %cl, %cl
	jnz .Ltail
.Ldone:
	ret
	.size __crunch_strcpy, .-__crunch_strcpy
	.section .note.GNU-stack,"",@progbits
