	.text
	# Inputs: string and substring pointers. Output: match pointer or NULL.
	.globl __crunch_strstr
	.type __crunch_strstr, @function
__crunch_strstr:
	cmpb $0, (%rsi)
	je .Lfound
.Lsearch:
	cmpb $0, (%rdi)
	je .Lnone
	mov %rdi, %rcx
	mov %rsi, %rdx
.Lcompare:
	movzbl (%rdx), %eax
	test %eax, %eax
	je .Lfound
	cmpb %al, (%rcx)
	jne .Lnext
	inc %rcx
	inc %rdx
	jmp .Lcompare
.Lnext:
	inc %rdi
	jmp .Lsearch
.Lfound:
	mov %rdi, %rax
	ret
.Lnone:
	xor %eax, %eax
	ret
	.size __crunch_strstr, .-__crunch_strstr
	.section .note.GNU-stack,"",@progbits
