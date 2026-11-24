	.bss
	.balign 8
.Lsave:
	.zero 8
	.text
	# Inputs: string or NULL and delimiter string. Output: token or NULL.
	.globl __crunch_strtok
	.type __crunch_strtok, @function
__crunch_strtok:
	test %rdi, %rdi
	jnz .Lskip
	mov .Lsave(%rip), %rdi
	test %rdi, %rdi
	jz .Lnull
.Lskip:
	movb (%rdi), %al
	test %al, %al
	jz .Lend
	mov %rsi, %rcx
.Lskip_check:
	movb (%rcx), %dl
	test %dl, %dl
	jz .Ltoken
	cmp %dl, %al
	je .Lskip_advance
	inc %rcx
	jmp .Lskip_check
.Lskip_advance:
	inc %rdi
	jmp .Lskip
.Ltoken:
	mov %rdi, %r8
.Lscan:
	inc %rdi
	movb (%rdi), %al
	test %al, %al
	jz .Llast
	mov %rsi, %rcx
.Lscan_check:
	movb (%rcx), %dl
	test %dl, %dl
	jz .Lscan
	cmp %dl, %al
	je .Ldelimiter
	inc %rcx
	jmp .Lscan_check
.Ldelimiter:
	movb $0, (%rdi)
	inc %rdi
	mov %rdi, .Lsave(%rip)
	mov %r8, %rax
	ret
.Llast:
	movq $0, .Lsave(%rip)
	mov %r8, %rax
	ret
.Lend:
	movq $0, .Lsave(%rip)
.Lnull:
	xor %eax, %eax
	ret
	.size __crunch_strtok, .-__crunch_strtok
	.section .note.GNU-stack,"",@progbits
