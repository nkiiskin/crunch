	.text
	# Finds name rsi in the NULL-terminated environment vector of byte strings at rdi.
	# Inputs: environment vector and name. Output: value pointer or NULL in rax.
	.globl __crunch_env_get
	.type __crunch_env_get, @function
__crunch_env_get:
	test %rdi, %rdi
	jz .Lmissing
	test %rsi, %rsi
	jz .Lmissing
	cmpb $0, (%rsi)
	je .Lmissing
	mov %rdi, %r8
	mov %rsi, %r9
.Lentry:
	mov (%r8), %r10
	test %r10, %r10
	jz .Lmissing
	mov %r9, %r11
.Lcompare:
	movzbl (%r11), %eax
	test %al, %al
	jz .Lname_end
	cmpb %al, (%r10)
	jne .Lnext
	inc %r10
	inc %r11
	jmp .Lcompare
.Lname_end:
	cmpb $'=', (%r10)
	jne .Lnext
	lea 1(%r10), %rax
	ret
.Lnext:
	add $8, %r8
	jmp .Lentry
.Lmissing:
	xor %eax, %eax
	ret
	.size __crunch_env_get, .-__crunch_env_get
	.section .note.GNU-stack,"",@progbits
