	.text
	# Computes a real power using x87 logarithm and scaling.
	# Input: base in xmm0, exponent in xmm1. Output: result in xmm0.
	.globl __crunch_pow
	.globl _crunch_ir_pow_helper
	.type _crunch_ir_pow_helper, @function
	.ifndef CRUNCH_SHARED
	.hidden __crunch_pow
	.hidden _crunch_ir_pow_helper
	.endif
	.type __crunch_pow, @function
__crunch_pow:
_crunch_ir_pow_helper:
	movq %xmm0, %r8
	movq %xmm1, %r9
	movabs $0x7fffffffffffffff, %rax
	mov %r8, %r10
	and %rax, %r10
	mov %r9, %r11
	and %rax, %r11
	test %r11, %r11
	jz .Lone
	movabs $0x3ff0000000000000, %rax
	cmp %rax, %r8
	je .Lone
	movabs $0x7ff0000000000000, %rax
	cmp %rax, %r10
	ja .Lnan
	cmp %rax, %r11
	ja .Lnan
	je .Linfinite_exponent

	# Classify a finite exponent as even integer, odd integer, or noninteger.
	xor %edx, %edx
	mov %r11, %rax
	shr $52, %rax
	sub $1023, %eax
	js .Lnoninteger
	cmp $52, %eax
	ja .Lexponent_classified
	movabs $0x000fffffffffffff, %rdi
	and %r11, %rdi
	movabs $0x0010000000000000, %rsi
	or %rsi, %rdi
	mov $52, %ecx
	sub %eax, %ecx
	mov $1, %rsi
	shl %cl, %rsi
	dec %rsi
	test %rsi, %rdi
	jnz .Lnoninteger
	shr %cl, %rdi
	and $1, %edi
	mov %edi, %edx
	jmp .Lexponent_classified
.Lnoninteger:
	mov $2, %edx
.Lexponent_classified:
	xor %esi, %esi
	test %r8, %r8
	jns .Lsign_classified
	cmp $1, %edx
	jne .Lsign_classified
	movabs $0x8000000000000000, %rsi
.Lsign_classified:
	test %r10, %r10
	jz .Lzero_base
	movabs $0x7ff0000000000000, %rax
	cmp %rax, %r10
	je .Linfinite_base
	test %r8, %r8
	jns .Lcalculate
	cmp $2, %edx
	je .Lnan
.Lcalculate:
	sub $16, %rsp
	mov %r10, (%rsp)
	mov %r9, 8(%rsp)
	fldl 8(%rsp)
	fldl (%rsp)
	fyl2x
	fld %st(0)
	frndint
	fxch %st(1)
	fsub %st(1), %st
	f2xm1
	fld1
	faddp
	fscale
	fstp %st(1)
	fstpl (%rsp)
	mov (%rsp), %rax
	add $16, %rsp
	or %rsi, %rax
	movq %rax, %xmm0
	ret
.Lzero_base:
	test %r9, %r9
	js .Lsigned_infinity
	jmp .Lsigned_zero
.Linfinite_base:
	test %r9, %r9
	js .Lsigned_zero
.Lsigned_infinity:
	movabs $0x7ff0000000000000, %rax
	or %rsi, %rax
	movq %rax, %xmm0
	ret
.Lsigned_zero:
	movq %rsi, %xmm0
	ret
.Linfinite_exponent:
	movabs $0x3ff0000000000000, %rax
	cmp %rax, %r10
	je .Lone
	jb .Lsmall_infinite_exponent
	test %r9, %r9
	js .Lzero
	jmp .Linfinity
.Lsmall_infinite_exponent:
	test %r9, %r9
	js .Linfinity
.Lzero:
	pxor %xmm0, %xmm0
	ret
.Linfinity:
	movabs $0x7ff0000000000000, %rax
	movq %rax, %xmm0
	ret
.Lone:
	movabs $0x3ff0000000000000, %rax
	movq %rax, %xmm0
	ret
.Lnan:
	movabs $0x7ff8000000000000, %rax
	movq %rax, %xmm0
	ret
	.size __crunch_pow, .-__crunch_pow
	.size _crunch_ir_pow_helper, .-_crunch_ir_pow_helper
	.section .note.GNU-stack,"",@progbits
