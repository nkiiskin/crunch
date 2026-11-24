	.text
	# Inputs: System V mode and variable arguments from variadic_call.bc.
	# Output: one when the arguments and vector register count match.
	.globl variadic_probe
	.type variadic_probe, @function
variadic_probe:
	cmp $0, %edi
	je .Lsingle
	cmp $1, %edi
	je .Lmany
	cmp $2, %edi
	je .Laggregate
	cmp $4, %edi
	je .Linteger
	cmp $5, %edi
	je .Lbig
	cmp $6, %edi
	je .Lpromotions
	jmp .Lfailed
.Lsingle:
	cmp $1, %al
	jne .Lfailed
	cmp $-5, %esi
	jne .Lfailed
	movq %xmm0, %rdx
	movabs $0x3ff8000000000000, %rcx
	cmp %rcx, %rdx
	jne .Lfailed
	jmp .Lpassed
.Lmany:
	cmp $8, %al
	jne .Lfailed
	cmp $1, %esi
	jne .Lfailed
	cmp $5, %r9d
	jne .Lfailed
	cmpl $6, 8(%rsp)
	jne .Lfailed
	cmpl $7, 16(%rsp)
	jne .Lfailed
	movq %xmm7, %rdx
	movabs $0x4020000000000000, %rcx
	cmp %rcx, %rdx
	jne .Lfailed
	mov 24(%rsp), %rdx
	movabs $0x4022000000000000, %rcx
	cmp %rcx, %rdx
	jne .Lfailed
	jmp .Lpassed
.Laggregate:
	cmp $2, %al
	jne .Lfailed
	cmp $42, %rsi
	jne .Lfailed
	movq %xmm0, %rdx
	movabs $0x3ff8000000000000, %rcx
	cmp %rcx, %rdx
	jne .Lfailed
	movq %xmm1, %rdx
	movabs $0x4004000000000000, %rcx
	cmp %rcx, %rdx
	jne .Lfailed
	jmp .Lpassed
.Linteger:
	cmp $0, %al
	jne .Lfailed
	cmp $-5, %esi
	jne .Lfailed
	jmp .Lpassed
.Lbig:
	cmp $0, %al
	jne .Lfailed
	cmp $7, %esi
	jne .Lfailed
	cmpq $10, 8(%rsp)
	jne .Lfailed
	cmpq $20, 16(%rsp)
	jne .Lfailed
	cmpq $30, 24(%rsp)
	jne .Lfailed
	jmp .Lpassed
.Lpromotions:
	cmp $0, %al
	jne .Lfailed
	cmp $255, %esi
	jne .Lfailed
	cmp $65535, %edx
	jne .Lfailed
	cmp $1, %ecx
	jne .Lfailed
	cmp $255, %r8d
	jne .Lfailed
	cmp $-1, %r9d
	jne .Lfailed
.Lpassed:
	mov $1, %eax
	ret
.Lfailed:
	xor %eax, %eax
	ret
	.size variadic_probe, .-variadic_probe

	# Inputs: one named double, named integer, then promoted int and double.
	# Output: one when values and vector register count match.
	.globl variadic_probe_fp
	.type variadic_probe_fp, @function
variadic_probe_fp:
	cmp $2, %al
	jne .Lfp_failed
	cmp $3, %edi
	jne .Lfp_failed
	cmp $-5, %esi
	jne .Lfp_failed
	movq %xmm0, %rdx
	movabs $0x4010000000000000, %rcx
	cmp %rcx, %rdx
	jne .Lfp_failed
	movq %xmm1, %rdx
	movabs $0x3ff8000000000000, %rcx
	cmp %rcx, %rdx
	jne .Lfp_failed
	mov $1, %eax
	ret
.Lfp_failed:
	xor %eax, %eax
	ret
	.size variadic_probe_fp, .-variadic_probe_fp

	# Inputs: mode, promoted long, and double in the System V variadic ABI.
	# Output: pair returned in rax and xmm0, or a zero integer on mismatch.
	.globl variadic_return
	.type variadic_return, @function
variadic_return:
	cmp $1, %al
	jne .Lreturn_failed
	cmp $7, %edi
	jne .Lreturn_failed
	mov %rsi, %rax
	ret
.Lreturn_failed:
	xor %eax, %eax
	ret
	.size variadic_return, .-variadic_return

	# Inputs: hidden result pointer, mode, and promoted integer.
	# Output: memory-class aggregate, returning its address in rax.
	.globl variadic_return_big
	.type variadic_return_big, @function
variadic_return_big:
	cmp $0, %al
	jne .Lreturn_big_failed
	cmp $8, %esi
	jne .Lreturn_big_failed
	movslq %edx, %rdx
	mov %rdx, (%rdi)
	movq $20, 8(%rdi)
	movq $30, 16(%rdi)
	mov %rdi, %rax
	ret
.Lreturn_big_failed:
	movq $0, (%rdi)
	movq $0, 8(%rdi)
	movq $0, 16(%rdi)
	mov %rdi, %rax
	ret
	.size variadic_return_big, .-variadic_return_big
	.section .note.GNU-stack,"",@progbits
