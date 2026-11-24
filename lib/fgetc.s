	.text
	# Input: stream in rdi. Output: byte from 0 to 255, or EOF on end-of-file or error.
	.globl __crunch_fgetc
	.type __crunch_fgetc, @function
__crunch_fgetc:
	sub $24, %rsp
	mov %rdi, %rcx
	lea 7(%rsp), %rdi
	mov $1, %esi
	mov $1, %edx
	call __crunch_fread
	cmp $1, %rax
	jne .Lfgetc_failure
	movzbl 7(%rsp), %eax
	add $24, %rsp
	ret
.Lfgetc_failure:
	mov $-1, %eax
	add $24, %rsp
	ret
	.size __crunch_fgetc, .-__crunch_fgetc
	.section .note.GNU-stack,"",@progbits
