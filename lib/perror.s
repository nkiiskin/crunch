	.text
	# Input: optional prefix. Output: no value; writes errno description to stderr.
	.globl __crunch_perror
	.type __crunch_perror, @function
__crunch_perror:
	push %r12
	push %r13
	push %r14
	mov %rdi, %r12
	call _crunch_errno_location
	mov (%rax), %r13d
	mov _crunch_stderr@GOTPCREL(%rip), %r14
	mov (%r14), %r14
	test %r12, %r12
	jz .Lperror_message
	cmpb $0, (%r12)
	je .Lperror_message
	mov %r12, %rdi
	mov %r14, %rsi
	call __crunch_fputs
	lea .Lperror_separator(%rip), %rdi
	mov %r14, %rsi
	call __crunch_fputs
.Lperror_message:
	mov %r13d, %edi
	call __crunch_strerror
	mov %rax, %rdi
	mov %r14, %rsi
	call __crunch_fputs
	mov $10, %edi
	mov %r14, %rsi
	call __crunch_fputc
	call _crunch_errno_location
	mov %r13d, (%rax)
	pop %r14
	pop %r13
	pop %r12
	ret
	.size __crunch_perror, .-__crunch_perror
	.section .rodata
.Lperror_separator:
	.asciz ": "
	.section .note.GNU-stack,"",@progbits
