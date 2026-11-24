	.text
	# Inputs: string or NULL, delimiter string, state pointer. Output: token or NULL.
	.globl __crunch_strtok_r
	.type __crunch_strtok_r, @function
__crunch_strtok_r:
	test %rdi, %rdi
	jnz .Lskip_delimiters
	mov (%rdx), %rdi
.Lskip_delimiters:
	movzbl (%rdi), %eax
	test %eax, %eax
	je .Lnone
	mov %rsi, %rcx
.Lskip_check:
	movzbl (%rcx), %r8d
	test %r8d, %r8d
	je .Ltoken
	inc %rcx
	cmp %r8d, %eax
	jne .Lskip_check
	inc %rdi
	jmp .Lskip_delimiters
.Ltoken:
	mov %rdi, %r9
.Lscan:
	movzbl (%rdi), %eax
	test %eax, %eax
	je .Lend
	mov %rsi, %rcx
.Lscan_check:
	movzbl (%rcx), %r8d
	test %r8d, %r8d
	je .Ladvance
	inc %rcx
	cmp %r8d, %eax
	jne .Lscan_check
	movb $0, (%rdi)
	inc %rdi
	jmp .Lend
.Ladvance:
	inc %rdi
	jmp .Lscan
.Lend:
	mov %rdi, (%rdx)
	mov %r9, %rax
	ret
.Lnone:
	mov %rdi, (%rdx)
	xor %eax, %eax
	ret
	.size __crunch_strtok_r, .-__crunch_strtok_r
	.section .note.GNU-stack,"",@progbits
