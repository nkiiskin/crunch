	.text
# Formats the tagged variadic values described by the compiler's stack-slot ABI.
# Inputs: rdi points at the format slot, rsi is the argument count; preceding
# stack slots contain value/tag pairs (1 byte/int, 2 long, 3 real, 4 byte pointer,
# 5 unsigned int, 6 unsigned long and 7 binary80 snapshot pointer).
# Output: character count in rax, or -1 for an invalid format or write failure.
	.globl __crunch_printf
	.type __crunch_printf, @function
__crunch_printf:
	xor %r9d, %r9d
	jmp .Lprintf_common
	.size __crunch_printf, .-__crunch_printf
	# Inputs: format slot in rdi and argument count in rsi.
	# Output: character count, or -1 on error; enables unsigned conversions.
	.globl __crunch_printf_v6
	.ifndef CRUNCH_SHARED
	.hidden __crunch_printf_v6
	.endif
	.type __crunch_printf_v6, @function
__crunch_printf_v6:
	mov $1, %r9d
	jmp .Lprintf_common
	.size __crunch_printf_v6, .-__crunch_printf_v6
	# Inputs: format slot in rdi and argument count in rsi.
	# Output: character count or -1; accepts representable signed/unsigned counterparts.
	.globl __crunch_printf_v26
	.ifndef CRUNCH_SHARED
	.hidden __crunch_printf_v26
	.endif
	.type __crunch_printf_v26, @function
__crunch_printf_v26:
	mov $3, %r9d
	jmp .Lprintf_common
	.size __crunch_printf_v26, .-__crunch_printf_v26
	# Inputs: format slot in rdi and argument count in rsi.
	# Output: character count or -1; also enables %.17g.
	.globl __crunch_printf_v28
	.ifndef CRUNCH_SHARED
	.hidden __crunch_printf_v28
	.endif
	.type __crunch_printf_v28, @function
__crunch_printf_v28:
	mov $7, %r9d
	jmp .Lprintf_common
	.size __crunch_printf_v28, .-__crunch_printf_v28
	# Inputs: format slot in rdi and argument count in rsi.
	# Output: character count or -1; also enables binary80 formatting.
	.globl __crunch_printf_v34
	.ifndef CRUNCH_SHARED
	.hidden __crunch_printf_v34
	.endif
	.type __crunch_printf_v34, @function
__crunch_printf_v34:
	mov $23, %r9d
	jmp .Lprintf_common
	.size __crunch_printf_v34, .-__crunch_printf_v34
	# Inputs: format slot in rdi and argument count in rsi.
	# Output: character count or -1; also enables dynamic string precision.
	.globl __crunch_printf_v36
	.ifndef CRUNCH_SHARED
	.hidden __crunch_printf_v36
	.endif
	.type __crunch_printf_v36, @function
__crunch_printf_v36:
	mov $55, %r9d
	jmp .Lprintf_common
	.size __crunch_printf_v36, .-__crunch_printf_v36
	# Inputs: format slot in rdi and argument count in rsi.
	# Output: character count or -1; also enables bounded binary80 precision.
	.globl __crunch_printf_v42
	.ifndef CRUNCH_SHARED
	.hidden __crunch_printf_v42
	.endif
	.type __crunch_printf_v42, @function
__crunch_printf_v42:
	mov $119, %r9d
.Lprintf_common:
	xor %edx, %edx
	lea __crunch_write_stdout_count(%rip), %rcx
	xor %r8d, %r8d
	jmp _crunch_format
	.size __crunch_printf_v42, .-__crunch_printf_v42

	# Inputs: format in rdi, va_list in rsi. Output: count or -1.
	.globl __crunch_vprintf
	.type __crunch_vprintf, @function
__crunch_vprintf:
	sub $24, %rsp
	mov %rdi, (%rsp)
	mov %rsp, %rdi
	xor %edx, %edx
	lea __crunch_write_stdout_count(%rip), %rcx
	xor %r8d, %r8d
	mov $127, %r9d
	call _crunch_format
	add $24, %rsp
	ret
	.size __crunch_vprintf, .-__crunch_vprintf

	# Inputs: format slot in rdi, count in rsi, context in rdx, writer in rcx,
	# optional padder in r8, and format capability flags in r9d.
	# A null padder selects the generic writer-based padder.
	# Output: character count or -1 on invalid format or write failure.
	.globl _crunch_format
	.hidden _crunch_format
	.type _crunch_format, @function
_crunch_format:
	push %rbp
	mov %rsp, %rbp
	push %r12
	push %r13
	push %r14
	push %r15
	push %rbx
	sub $5208, %rsp
	mov %rdx, %rbx
	mov %rcx, 16(%rsp)
	mov %r9d, 144(%rsp)
	test %r8, %r8
	jnz .Lpadder_ready
	lea .Lformat_pad(%rip), %r8
.Lpadder_ready:
	mov %r8, 24(%rsp)
	mov (%rdi), %r12
	lea -8(%rdi), %r13
	testb $8, %r9b
	jz .Lformat_arguments_ready
	mov %rsi, %r13
.Lformat_arguments_ready:
	mov %rsi, %r14
	xor %r15d, %r15d
	test %r12, %r12
	jz .Lerror
.Lnext:
	movzbl (%r12), %eax
	test %al, %al
	jz .Ldone
	inc %r12
	cmp $37, %al
	je .Lconversion
.Lcharacter:
	mov %al, (%rsp)
	mov %rsp, %rdi
	mov $1, %esi
	mov %rbx, %rdx
	call *16(%rsp)
	jmp .Lresult
.Lconversion:
	movzbl (%r12), %eax
	cmp $37, %al
	jne .Lflags_start
	inc %r12
	jmp .Lcharacter
.Lflags_start:
	movq $0, 32(%rsp)
	movq $0, 40(%rsp)
.Lflags:
	cmp $45, %al
	je .Lflag_left
	cmp $48, %al
	je .Lflag_zero
	jmp .Lwidth
.Lflag_left:
	orq $1, 40(%rsp)
	jmp .Lflag_next
.Lflag_zero:
	orq $2, 40(%rsp)
.Lflag_next:
	inc %r12
	movzbl (%r12), %eax
	jmp .Lflags
.Lwidth:
	cmp $48, %al
	jb .Lconversion_type
	cmp $57, %al
	ja .Lconversion_type
	mov 32(%rsp), %rcx
	imul $10, %rcx
	sub $48, %eax
	add %rax, %rcx
	cmp $2147483647, %rcx
	ja .Lerror
	mov %rcx, 32(%rsp)
	inc %r12
	movzbl (%r12), %eax
	jmp .Lwidth
.Lconversion_type:
	cmp $46, %al
	jne .Lconversion_value
	cmpb $42, 1(%r12)
	jne .Lfixed_precision
	cmpb $115, 2(%r12)
	jne .Lerror
	testb $8, 144(%rsp)
	jz .Ltagged_precision
	call .Lva_gp
	jmp .Ldynamic_precision_ready
.Ltagged_precision:
	testb $32, 144(%rsp)
	jz .Lerror
	test %r14, %r14
	jz .Lerror
	dec %r14
	mov (%r13), %rdi
	mov -8(%r13), %rcx
	sub $16, %r13
	cmp $1, %rcx
	jne .Lerror
.Ldynamic_precision_ready:
	mov %edi, 120(%rsp)
	orq $16, 40(%rsp)
	add $2, %r12
	mov $115, %al
	jmp .Lconversion_value
.Lfixed_precision:
	testb $64, 144(%rsp)
	jz .Llegacy_fixed_precision
	lea 1(%r12), %r10
	xor %ecx, %ecx
.Lextended_precision_digits:
	movzbl (%r10), %eax
	cmp $48, %al
	jb .Lextended_precision_end
	cmp $57, %al
	ja .Lextended_precision_end
	imul $10, %ecx
	sub $48, %eax
	add %eax, %ecx
	cmp $100, %ecx
	ja .Lerror
	inc %r10
	jmp .Lextended_precision_digits
.Lextended_precision_end:
	cmpb $76, (%r10)
	jne .Llegacy_fixed_precision
	cmpb $102, 1(%r10)
	jne .Llegacy_fixed_precision
	mov %ecx, 120(%rsp)
	orq $32, 40(%rsp)
	mov %r10, %r12
	mov $76, %al
	jmp .Lconversion_value
.Llegacy_fixed_precision:
	testb $4, 144(%rsp)
	jz .Lerror
	cmpb $49, 1(%r12)
	jne .Lerror
	cmpb $55, 2(%r12)
	jne .Lerror
	cmpb $103, 3(%r12)
	je .Lprecision_ready
	testb $16, 144(%rsp)
	jz .Lerror
	cmpb $76, 3(%r12)
	jne .Lerror
	cmpb $103, 4(%r12)
	jne .Lerror
.Lprecision_ready:
	add $3, %r12
	movzbl (%r12), %eax
	orq $8, 40(%rsp)
.Lconversion_value:
	inc %r12
	testb $8, 144(%rsp)
	jnz .Lva_argument
	test %r14, %r14
	jz .Lerror
	dec %r14
	mov (%r13), %rdi
	mov -8(%r13), %rcx
	sub $16, %r13
.Ldispatch:
	cmp $100, %al
	je .Lint
	cmp $117, %al
	je .Luint
	cmp $120, %al
	je .Luint
	cmp $108, %al
	je .Llong
	cmp $76, %al
	je .Lextended
	cmp $122, %al
	je .Lsize
	cmp $102, %al
	je .Lreal
	cmp $103, %al
	je .Lreal_precise
	cmp $115, %al
	je .Lstring
	cmp $99, %al
	jne .Lerror
	cmp $1, %rcx
	je .Lcharacter_arg
	cmp $5, %rcx
	jne .Lerror
	testb $2, 144(%rsp)
	jz .Lerror
	test %edi, %edi
	js .Lerror
.Lcharacter_arg:
	mov %dil, 48(%rsp)
	lea 48(%rsp), %rdi
	mov $1, %esi
	jmp .Lfield
.Lint:
	cmp $1, %rcx
	je .Lsigned_arg
	cmp $5, %rcx
	jne .Lerror
	testb $2, 144(%rsp)
	jz .Lerror
	test %edi, %edi
	js .Lerror
.Lsigned_arg:
	movslq %edi, %rdi
	jmp .Lnumber
.Lsize:
	movzbl (%r12), %eax
	cmp $117, %al
	jne .Lerror
	jmp .Llong_unsigned
.Llong:
	movzbl (%r12), %eax
	cmp $108, %al
	jne .Llong_type
	inc %r12
	movzbl (%r12), %eax
.Llong_type:
	cmp $100, %al
	je .Llong_signed
	cmp $117, %al
	je .Llong_unsigned
	cmp $120, %al
	jne .Lerror
.Llong_unsigned:
	inc %r12
	cmp $6, %rcx
	je .Llong_unsigned_arg
	cmp $2, %rcx
	jne .Lerror
	testb $2, 144(%rsp)
	jz .Lerror
	test %rdi, %rdi
	js .Lerror
.Llong_unsigned_arg:
	cmpl $0, 144(%rsp)
	je .Lerror
	cmp $120, %al
	je .Lhex_number
	jmp .Lunsigned_number
.Llong_signed:
	inc %r12
	cmp $2, %rcx
	je .Llong_signed_arg
	cmp $6, %rcx
	jne .Lerror
	testb $2, 144(%rsp)
	jz .Lerror
	test %rdi, %rdi
	js .Lerror
.Llong_signed_arg:
	jmp .Lnumber
.Luint:
	cmp $5, %rcx
	je .Lunsigned_arg
	cmp $1, %rcx
	jne .Lerror
	testb $2, 144(%rsp)
	jz .Lerror
	test %edi, %edi
	js .Lerror
.Lunsigned_arg:
	cmpl $0, 144(%rsp)
	je .Lerror
	mov %edi, %edi
	cmp $120, %al
	je .Lhex_number
.Lunsigned_number:
	orq $4, 40(%rsp)
	lea 48(%rsp), %rsi
	lea .Lcapture(%rip), %rdx
	call __crunch_printf_unsigned
	jmp .Lnumber_result
.Lhex_number:
	orq $4, 40(%rsp)
	lea 48(%rsp), %rsi
	lea .Lcapture(%rip), %rdx
	call __crunch_printf_hex
	jmp .Lnumber_result
.Lnumber:
	orq $4, 40(%rsp)
	lea 48(%rsp), %rsi
	lea .Lcapture(%rip), %rdx
	call __crunch_printf_number
	jmp .Lnumber_result
.Lreal:
	cmp $3, %rcx
	jne .Lerror
	orq $4, 40(%rsp)
	lea 48(%rsp), %rsi
	lea .Lcapture(%rip), %rdx
	call __crunch_printf_real
	jmp .Lnumber_result
.Lextended:
	testb $16, 144(%rsp)
	jz .Lerror
	cmp $7, %rcx
	jne .Lerror
	movzbl (%r12), %eax
	inc %r12
	xor %edx, %edx
	cmp $102, %al
	je .Lextended_format
	cmp $103, %al
	jne .Lerror
	testb $8, 40(%rsp)
	jz .Lerror
	mov $1, %edx
.Lextended_format:
	test %edx, %edx
	jnz .Lextended_precision_valid
	testb $8, 40(%rsp)
	jnz .Lerror
.Lextended_precision_valid:
	mov $6, %ecx
	testb $32, 40(%rsp)
	jz .Lextended_precision_set
	mov 120(%rsp), %ecx
.Lextended_precision_set:
	movzwl 8(%rdi), %eax
	and $32767, %eax
	cmp $32767, %eax
	je .Lextended_special
	orq $4, 40(%rsp)
.Lextended_special:
	lea 152(%rsp), %rsi
	call _crunch_format_extended
	test %rax, %rax
	js .Lerror
	lea 152(%rsp), %rdi
	mov %rax, %rsi
	jmp .Lfield
.Lreal_precise:
	testb $8, 40(%rsp)
	jz .Lerror
	cmp $3, %rcx
	jne .Lerror
	mov %rdi, %rax
	shr $52, %rax
	and $2047, %eax
	cmp $2047, %eax
	je .Lreal_precise_call
	orq $4, 40(%rsp)
.Lreal_precise_call:
	lea 48(%rsp), %rsi
	call __crunch_printf_g17
.Lnumber_result:
	test %rax, %rax
	js .Lerror
	lea 48(%rsp), %rdi
	mov %rax, %rsi
	jmp .Lfield
.Lstring:
	cmp $4, %rcx
	jne .Lerror
	test %rdi, %rdi
	jz .Lerror
	testb $16, 40(%rsp)
	jz .Lstring_unbounded
	movslq 120(%rsp), %rcx
	test %rcx, %rcx
	js .Lstring_unbounded
	xor %esi, %esi
.Lstring_bounded_loop:
	cmp %rcx, %rsi
	jae .Lfield
	cmpb $0, (%rdi,%rsi)
	je .Lfield
	inc %rsi
	jmp .Lstring_bounded_loop
.Lstring_unbounded:
	mov %rdi, 8(%rsp)
	call __crunch_strlen
	mov 8(%rsp), %rdi
	mov %rax, %rsi
.Lfield:
	mov %rdi, 8(%rsp)
	mov %rsi, 136(%rsp)
	mov 32(%rsp), %rax
	sub %rsi, %rax
	ja .Lpadding_ready
	xor %eax, %eax
.Lpadding_ready:
	mov %rax, 32(%rsp)
	add %rsi, %rax
	mov %rax, 128(%rsp)
	add %r15, %rax
	cmp $2147483647, %rax
	ja .Lerror
	testb $1, 40(%rsp)
	jnz .Lwrite_value
	cmpq $0, 32(%rsp)
	je .Lwrite_value
	mov $32, %esi
	mov 40(%rsp), %rax
	and $6, %eax
	cmp $6, %eax
	jne .Lpad_before
	mov $48, %esi
	cmpb $45, (%rdi)
	jne .Lpad_before
	mov $1, %esi
	mov %rbx, %rdx
	call *16(%rsp)
	test %rax, %rax
	js .Lerror
	incq 8(%rsp)
	decq 136(%rsp)
	mov $48, %esi
.Lpad_before:
	mov 32(%rsp), %rdi
	mov %rbx, %rdx
	mov 16(%rsp), %rcx
	call *24(%rsp)
	test %rax, %rax
	js .Lerror
.Lwrite_value:
	mov 8(%rsp), %rdi
	mov 136(%rsp), %rsi
	mov %rbx, %rdx
	call *16(%rsp)
	test %rax, %rax
	js .Lerror
	testb $1, 40(%rsp)
	jz .Lfield_done
	cmpq $0, 32(%rsp)
	je .Lfield_done
	mov 32(%rsp), %rdi
	mov $32, %esi
	mov %rbx, %rdx
	mov 16(%rsp), %rcx
	call *24(%rsp)
	test %rax, %rax
	js .Lerror
.Lfield_done:
	add 128(%rsp), %r15
	jmp .Lnext
.Lresult:
	test %rax, %rax
	js .Lerror
	add %rax, %r15
	cmp $2147483647, %r15
	ja .Lerror
	jmp .Lnext
.Ldone:
	mov %r15, %rax
	jmp .Lreturn
.Lerror:
	mov $-1, %rax
.Lreturn:
	add $5208, %rsp
	pop %rbx
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbp
	ret

	# Consume one argument from a System V AMD64 va_list.
.Lva_argument:
	mov $1, %ecx
	cmp $100, %al
	je .Lva_integer
	cmp $99, %al
	je .Lva_integer
	mov $5, %ecx
	cmp $117, %al
	je .Lva_integer
	cmp $120, %al
	je .Lva_integer
	mov $4, %ecx
	cmp $115, %al
	je .Lva_integer
	mov $3, %ecx
	cmp $102, %al
	je .Lva_real
	cmp $103, %al
	je .Lva_real
	cmp $76, %al
	je .Lva_extended
	cmp $108, %al
	je .Lva_long
	cmp $122, %al
	jne .Lerror
	cmpb $117, (%r12)
	jne .Lerror
	mov $6, %ecx
	jmp .Lva_integer
.Lva_long:
	movzbl (%r12), %r10d
	cmp $108, %r10b
	jne .Lva_long_type
	movzbl 1(%r12), %r10d
.Lva_long_type:
	mov $2, %ecx
	cmp $100, %r10b
	je .Lva_integer
	mov $6, %ecx
	cmp $117, %r10b
	je .Lva_integer
	cmp $120, %r10b
	jne .Lerror
.Lva_integer:
	call .Lva_gp
	jmp .Ldispatch
.Lva_real:
	call .Lva_fp
	jmp .Ldispatch
.Lva_extended:
	testb $16, 144(%rsp)
	jz .Lerror
	mov 8(%r13), %rdi
	add $15, %rdi
	and $-16, %rdi
	lea 16(%rdi), %r10
	mov %r10, 8(%r13)
	mov $7, %ecx
	jmp .Ldispatch
.Lva_gp:
	mov (%r13), %r10d
	cmp $48, %r10d
	jae .Lva_stack
	mov 16(%r13), %r11
	add %r10, %r11
	addl $8, (%r13)
	mov (%r11), %rdi
	ret
.Lva_fp:
	mov 4(%r13), %r10d
	cmp $176, %r10d
	jae .Lva_stack
	mov 16(%r13), %r11
	add %r10, %r11
	addl $16, 4(%r13)
	mov (%r11), %rdi
	ret
.Lva_stack:
	mov 8(%r13), %r11
	leaq 8(%r11), %r10
	mov %r10, 8(%r13)
	mov (%r11), %rdi
	ret
	.size _crunch_format, .-_crunch_format

	# Numeric helpers emit at most 21 bytes into the formatter's scratch buffer.
.Lcapture:
	mov %rsi, %rax
	mov %rdi, %r8
	mov %rdx, %rdi
	mov %rsi, %rcx
	mov %r8, %rsi
	rep movsb
	ret

	# Inputs: count in rdi, padding byte in sil, context in rdx, writer in rcx.
	# Output: padding bytes written, or -1 on write failure.
.Lformat_pad:
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	sub $256, %rsp
	mov %rdi, %r12
	mov %rdi, %r13
	mov %rcx, %r14
	mov %rdx, %r15
	mov %esi, %eax
	mov %rsp, %rdi
	mov $256, %ecx
	rep stosb
.Lpad_loop:
	test %r12, %r12
	jz .Lpad_done
	mov %r12, %rbx
	cmp $256, %rbx
	jbe .Lpad_chunk
	mov $256, %ebx
.Lpad_chunk:
	mov %rsp, %rdi
	mov %rbx, %rsi
	mov %r15, %rdx
	call *%r14
	cmp %rbx, %rax
	jne .Lpad_error
	sub %rbx, %r12
	jmp .Lpad_loop
.Lpad_done:
	mov %r13, %rax
	jmp .Lpad_return
.Lpad_error:
	mov $-1, %rax
.Lpad_return:
	add $256, %rsp
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	ret
	.section .note.GNU-stack,"",@progbits
