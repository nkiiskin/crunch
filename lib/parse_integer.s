	.text
	# Inputs: text in rdi, optional end pointer in rsi, base in edx,
	# signed-result flag (0 or 8) in ecx. Output: result in rax.
	.globl _crunch_parse_integer
	.hidden _crunch_parse_integer
	.type _crunch_parse_integer, @function
_crunch_parse_integer:
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	mov %rdi, %rbx
	mov %rdi, %r12
	mov %rsi, %r13
	mov %edx, %r14d
	mov %ecx, %r15d
	test %r14d, %r14d
	jz .Lspace
	cmp $2, %r14d
	jl .Linvalid_base
	cmp $36, %r14d
	jg .Linvalid_base
.Lspace:
	movzbl (%rbx), %eax
	cmp $32, %eax
	je .Lskip_space
	lea -9(%rax), %ecx
	cmp $4, %ecx
	ja .Lsign
.Lskip_space:
	inc %rbx
	jmp .Lspace
.Lsign:
	cmp $45, %al
	jne .Lplus
	or $1, %r15d
	inc %rbx
	jmp .Lbase
.Lplus:
	cmp $43, %al
	jne .Lbase
	inc %rbx
.Lbase:
	test %r14d, %r14d
	jnz .Lcheck_hex
	mov $10, %r14d
	cmpb $48, (%rbx)
	jne .Lstart_digits
	mov $8, %r14d
	movzbl 1(%rbx), %eax
	or $32, %eax
	cmp $120, %eax
	jne .Lstart_digits
	mov $16, %r14d
.Lcheck_hex:
	cmp $16, %r14d
	jne .Lstart_digits
	cmpb $48, (%rbx)
	jne .Lstart_digits
	movzbl 1(%rbx), %eax
	or $32, %eax
	cmp $120, %eax
	jne .Lstart_digits
	movzbl 2(%rbx), %eax
	lea -48(%rax), %ecx
	cmp $9, %ecx
	jbe .Lhex_prefix
	or $32, %eax
	sub $97, %eax
	cmp $5, %eax
	ja .Lstart_digits
.Lhex_prefix:
	add $2, %rbx
.Lstart_digits:
	xor %r8d, %r8d
.Ldigits:
	movzbl (%rbx), %ecx
	lea -48(%rcx), %eax
	cmp $9, %eax
	jbe .Ldigit
	or $32, %ecx
	sub $97, %ecx
	cmp $25, %ecx
	ja .Lend
	lea 10(%rcx), %eax
.Ldigit:
	cmp %r14d, %eax
	jae .Lend
	or $2, %r15d
	test $4, %r15d
	jnz .Lnext_digit
	mov %eax, %ecx
	mov %r8, %rax
	mul %r14
	test %rdx, %rdx
	jnz .Loverflow
	add %rcx, %rax
	jc .Loverflow
	mov %rax, %r8
	jmp .Lnext_digit
.Loverflow:
	or $4, %r15d
.Lnext_digit:
	inc %rbx
	jmp .Ldigits
.Lend:
	test $2, %r15d
	jnz .Lend_pointer
	mov %r12, %rbx
.Lend_pointer:
	test %r13, %r13
	jz .Lresult
	mov %rbx, (%r13)
.Lresult:
	test $8, %r15d
	jz .Lcheck_overflow
	movabs $0x7fffffffffffffff, %rax
	test $1, %r15d
	jz .Lcheck_signed_limit
	inc %rax
.Lcheck_signed_limit:
	cmp %rax, %r8
	jbe .Lcheck_overflow
	or $4, %r15d
.Lcheck_overflow:
	test $4, %r15d
	jz .Lvalue
	call _crunch_errno_location
	movl $34, (%rax)
	mov $-1, %rax
	test $8, %r15d
	jz .Lreturn
	movabs $0x7fffffffffffffff, %rax
	test $1, %r15d
	jz .Lreturn
	inc %rax
	jmp .Lreturn
.Lvalue:
	mov %r8, %rax
	test $1, %r15d
	jz .Lreturn
	neg %rax
	jmp .Lreturn
.Linvalid_base:
	test %r13, %r13
	jz .Linvalid_errno
	mov %r12, (%r13)
.Linvalid_errno:
	call _crunch_errno_location
	movl $22, (%rax)
	xor %eax, %eax
.Lreturn:
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	ret
	.size _crunch_parse_integer, .-_crunch_parse_integer
	.section .note.GNU-stack,"",@progbits
