	.text
	# Inputs: path in rdi, mode in rsi. Output: stream pointer or null with errno set.
	.globl __crunch_fopen
	.type __crunch_fopen, @function
__crunch_fopen:
	push %r12
	push %r13
	push %r14
	mov %rdi, %r12
	test %rdi, %rdi
	jz .Lfopen_invalid
	test %rsi, %rsi
	jz .Lfopen_invalid
	movzbl (%rsi), %eax
	inc %rsi
	xor %r13d, %r13d
	cmp $'r', %al
	je .Lfopen_options
	mov $577, %r13d
	cmp $'w', %al
	je .Lfopen_options
	mov $1089, %r13d
	cmp $'a', %al
	jne .Lfopen_invalid
.Lfopen_options:
	xor %ecx, %ecx
.Lfopen_option:
	movzbl (%rsi), %edx
	inc %rsi
	test %edx, %edx
	jz .Lfopen_allocate
	cmp $'+', %dl
	je .Lfopen_plus
	cmp $'b', %dl
	je .Lfopen_binary
	cmp $'e', %dl
	je .Lfopen_cloexec
	cmp $'x', %dl
	je .Lfopen_exclusive
	jmp .Lfopen_invalid
.Lfopen_plus:
	test $1, %ecx
	jnz .Lfopen_invalid
	or $1, %ecx
	and $-4, %r13d
	or $2, %r13d
	jmp .Lfopen_option
.Lfopen_binary:
	test $2, %ecx
	jnz .Lfopen_invalid
	or $2, %ecx
	jmp .Lfopen_option
.Lfopen_cloexec:
	test $4, %ecx
	jnz .Lfopen_invalid
	or $4, %ecx
	or $524288, %r13d
	jmp .Lfopen_option
.Lfopen_exclusive:
	cmp $'w', %al
	jne .Lfopen_invalid
	test $8, %ecx
	jnz .Lfopen_invalid
	or $8, %ecx
	or $128, %r13d
	jmp .Lfopen_option
.Lfopen_allocate:
	mov $16, %edi
	call __crunch_malloc
	test %rax, %rax
	jz .Lfopen_done
	mov %rax, %r14
	mov %r12, %rdi
	mov %r13d, %esi
	mov $438, %edx
	call __crunch_open
	cmp $-1, %eax
	je .Lfopen_open_failed
	mov %eax, (%r14)
	mov %r13d, 4(%r14)
	movl $0, 8(%r14)
	movl $1, 12(%r14)
	mov %r14, %rax
	jmp .Lfopen_done
.Lfopen_open_failed:
	call _crunch_errno_location
	mov (%rax), %r13d
	mov %r14, %rdi
	call __crunch_free
	call _crunch_errno_location
	mov %r13d, (%rax)
	xor %eax, %eax
	jmp .Lfopen_done
.Lfopen_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	xor %eax, %eax
.Lfopen_done:
	pop %r14
	pop %r13
	pop %r12
	ret
	.size __crunch_fopen, .-__crunch_fopen
	.section .note.GNU-stack,"",@progbits
