	.text
	# Writes the IEEE-754 double bit pattern in rdi with six fractional digits.
	# Inputs: double bits in rdi, stream in rsi, writer in rdx. Output: count or -1.
	.globl __crunch_printf_real
	.hidden __crunch_printf_real
	.type __crunch_printf_real, @function
__crunch_printf_real:
	sub $104, %rsp
	mov %rsi, (%rsp)
	mov %rdx, 8(%rsp)
	mov %rdi, %rax
	bt $63, %rax
	setc %r8b
	movzbl %r8b, %r8d
	btr $63, %rax
	movq %rax, %xmm0
	ucomisd %xmm0, %xmm0
	jp .Linvalid
	movabs $0x42a05ef39b200000, %rax
	movq %rax, %xmm1
	ucomisd %xmm1, %xmm0
	jae .Linvalid
	movabs $0x412e848000000000, %rax
	movq %rax, %xmm1
	mulsd %xmm1, %xmm0
	movabs $0x3fe0000000000000, %rax
	movq %rax, %xmm1
	addsd %xmm1, %xmm0
	cvttsd2siq %xmm0, %rax
	xor %edx, %edx
	mov $1000000, %rcx
	div %rcx
	mov %rax, %r10
	mov %rdx, %r9
	lea 104(%rsp), %rsi
	mov $6, %r11d
.Lfraction:
	mov %r9, %rax
	xor %edx, %edx
	mov $10, %rcx
	div %rcx
	add $48, %dl
	dec %rsi
	mov %dl, (%rsi)
	mov %rax, %r9
	dec %r11d
	jnz .Lfraction
	dec %rsi
	movb $46, (%rsi)
	mov %r10, %rax
.Lintpart:
	xor %edx, %edx
	mov $10, %rcx
	div %rcx
	add $48, %dl
	dec %rsi
	mov %dl, (%rsi)
	test %rax, %rax
	jnz .Lintpart
	test %r8d, %r8d
	jz .Lready
	dec %rsi
	movb $45, (%rsi)
.Lready:
	mov %rsi, %rdi
	lea 104(%rsp), %rsi
	sub %rdi, %rsi
	mov (%rsp), %rdx
	call *8(%rsp)
	add $104, %rsp
	ret
.Linvalid:
	add $104, %rsp
	mov $-1, %rax
	ret
	.size __crunch_printf_real, .-__crunch_printf_real
	.section .note.GNU-stack,"",@progbits
