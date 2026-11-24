	.text
	# Inputs: character in edi and stream in rsi. Output: written byte or EOF on error.
	.globl __crunch_fputc
	.type __crunch_fputc, @function
__crunch_fputc:
	sub $24, %rsp
	movb %dil, 7(%rsp)
	mov %rsi, %rcx
	lea 7(%rsp), %rdi
	mov $1, %esi
	mov $1, %edx
	call __crunch_fwrite
	cmp $1, %rax
	jne .Lfputc_failure
	movzbl 7(%rsp), %eax
	add $24, %rsp
	ret
.Lfputc_failure:
	mov $-1, %eax
	add $24, %rsp
	ret
	.size __crunch_fputc, .-__crunch_fputc
	.section .note.GNU-stack,"",@progbits
