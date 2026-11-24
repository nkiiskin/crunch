	.text
	# Inputs: binary64 bits in rdi, output buffer of at least 32 bytes in rsi.
	# Output: number of bytes written, rounded to 17 significant decimal digits.
	.globl __crunch_printf_g17
	.hidden __crunch_printf_g17
	.type __crunch_printf_g17, @function
__crunch_printf_g17:
	push %rbp
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	sub $920, %rsp
	mov %rsi, %r12
	mov %rsi, 832(%rsp)
	mov %rdi, %rax
	shr $63, %rax
	test %eax, %eax
	jz .Lg_sign_done
	movb $45, (%r12)
	inc %r12
.Lg_sign_done:
	mov %rdi, %rax
	shr $52, %rax
	and $2047, %eax
	mov %eax, %ebp
	mov %rdi, %rax
	shl $12, %rax
	shr $12, %rax
	cmp $2047, %ebp
	jne .Lg_finite
	test %rax, %rax
	jnz .Lg_nan
	movb $105, (%r12)
	movb $110, 1(%r12)
	movb $102, 2(%r12)
	add $3, %r12
	jmp .Lg_done
.Lg_nan:
	movb $110, (%r12)
	movb $97, 1(%r12)
	movb $110, 2(%r12)
	add $3, %r12
	jmp .Lg_done
.Lg_finite:
	xor %r15d, %r15d
	test %ebp, %ebp
	jz .Lg_subnormal
	movabs $0x10000000000000, %rdx
	or %rdx, %rax
	mov %ebp, %r14d
	sub $1075, %r14
	jmp .Lg_digits
.Lg_subnormal:
	mov $-1074, %r14
	test %rax, %rax
	jnz .Lg_digits
	xor %r14d, %r14d
.Lg_digits:
	xor %r13d, %r13d
	mov $10, %ecx
.Lg_initial_digit:
	xor %edx, %edx
	div %rcx
	mov %dl, (%rsp,%r13)
	inc %r13
	test %rax, %rax
	jnz .Lg_initial_digit
	mov $2, %r9d
	mov %r14, %r10
	test %r14, %r14
	jns .Lg_scale_loop
	neg %r10
	mov %r10, %r15
	mov $5, %r9d
.Lg_scale_loop:
	test %r10, %r10
	jz .Lg_scaled
	xor %ebx, %ebx
	xor %r8d, %r8d
.Lg_multiply_digit:
	movzbl (%rsp,%rbx), %eax
	imul %r9d, %eax
	add %r8d, %eax
	xor %edx, %edx
	div %ecx
	mov %dl, (%rsp,%rbx)
	mov %eax, %r8d
	inc %rbx
	cmp %r13, %rbx
	jb .Lg_multiply_digit
	test %r8d, %r8d
	jz .Lg_multiply_done
	mov %r8b, (%rsp,%r13)
	inc %r13
.Lg_multiply_done:
	dec %r10
	jmp .Lg_scale_loop
.Lg_scaled:
	mov %r13, %r14
	dec %r14
	sub %r15, %r14
	mov %r13, %r10
	cmp $17, %r10
	jbe .Lg_copy_digits
	mov $17, %r10d
.Lg_copy_digits:
	xor %ebx, %ebx
.Lg_copy_loop:
	mov %r13, %rax
	dec %rax
	sub %rbx, %rax
	mov (%rsp,%rax), %al
	mov %al, 800(%rsp,%rbx)
	inc %rbx
	cmp %r10, %rbx
	jb .Lg_copy_loop
	cmp $17, %r13
	jbe .Lg_trim
	mov %r13, %rbx
	sub $18, %rbx
	movzbl (%rsp,%rbx), %eax
	cmp $5, %eax
	ja .Lg_round_up
	jb .Lg_trim
.Lg_sticky:
	test %rbx, %rbx
	jz .Lg_tie
	dec %rbx
	cmpb $0, (%rsp,%rbx)
	jne .Lg_round_up
	jmp .Lg_sticky
.Lg_tie:
	testb $1, 816(%rsp)
	jz .Lg_trim
.Lg_round_up:
	mov $17, %ebx
.Lg_carry:
	dec %rbx
	incb 800(%rsp,%rbx)
	cmpb $10, 800(%rsp,%rbx)
	jb .Lg_trim
	movb $0, 800(%rsp,%rbx)
	test %rbx, %rbx
	jnz .Lg_carry
	movb $1, 800(%rsp)
	inc %r14
.Lg_trim:
	cmp $1, %r10
	jbe .Lg_select
	cmpb $0, 799(%rsp,%r10)
	jne .Lg_select
	dec %r10
	jmp .Lg_trim
.Lg_select:
	cmp $-4, %r14
	jl .Lg_scientific
	cmp $17, %r14
	jge .Lg_scientific
	test %r14, %r14
	js .Lg_small_fixed
	xor %ebx, %ebx
.Lg_fixed_integer:
	cmp %r10, %rbx
	jae .Lg_fixed_zero
	mov 800(%rsp,%rbx), %al
	add $48, %al
	jmp .Lg_fixed_digit
.Lg_fixed_zero:
	mov $48, %al
.Lg_fixed_digit:
	mov %al, (%r12)
	inc %r12
	inc %rbx
	cmp %r14, %rbx
	jle .Lg_fixed_integer
	cmp %r10, %rbx
	jae .Lg_done
	movb $46, (%r12)
	inc %r12
	jmp .Lg_fraction
.Lg_small_fixed:
	movb $48, (%r12)
	movb $46, 1(%r12)
	add $2, %r12
	mov %r14, %rbx
	neg %rbx
	dec %rbx
.Lg_leading_zero:
	test %rbx, %rbx
	jz .Lg_fraction_start
	movb $48, (%r12)
	inc %r12
	dec %rbx
	jmp .Lg_leading_zero
.Lg_fraction_start:
	xor %ebx, %ebx
.Lg_fraction:
	mov 800(%rsp,%rbx), %al
	add $48, %al
	mov %al, (%r12)
	inc %r12
	inc %rbx
	cmp %r10, %rbx
	jb .Lg_fraction
	jmp .Lg_done
.Lg_scientific:
	mov 800(%rsp), %al
	add $48, %al
	mov %al, (%r12)
	inc %r12
	mov $1, %ebx
	cmp %r10, %rbx
	jae .Lg_exponent
	movb $46, (%r12)
	inc %r12
.Lg_scientific_fraction:
	mov 800(%rsp,%rbx), %al
	add $48, %al
	mov %al, (%r12)
	inc %r12
	inc %rbx
	cmp %r10, %rbx
	jb .Lg_scientific_fraction
.Lg_exponent:
	movb $101, (%r12)
	movb $43, 1(%r12)
	test %r14, %r14
	jns .Lg_exponent_sign
	movb $45, 1(%r12)
	neg %r14
.Lg_exponent_sign:
	add $2, %r12
	mov %r14, %rax
	xor %ebx, %ebx
	mov $10, %ecx
.Lg_exponent_digits:
	xor %edx, %edx
	div %rcx
	add $48, %dl
	mov %dl, 840(%rsp,%rbx)
	inc %rbx
	test %rax, %rax
	jnz .Lg_exponent_digits
	cmp $2, %rbx
	jae .Lg_exponent_output
	movb $48, (%r12)
	inc %r12
.Lg_exponent_output:
	dec %rbx
	mov 840(%rsp,%rbx), %al
	mov %al, (%r12)
	inc %r12
	test %rbx, %rbx
	jnz .Lg_exponent_output
.Lg_done:
	mov %r12, %rax
	sub 832(%rsp), %rax
	add $920, %rsp
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	pop %rbp
	ret
	.size __crunch_printf_g17, .-__crunch_printf_g17
	.section .note.GNU-stack,"",@progbits
