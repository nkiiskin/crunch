	.section .rodata
.Lvector:
	.quad 0x4019000000000000
	.quad 0x1122334455667788
	.text
	.global _start
_start:
	mov $1, %rdi
	mov $42, %rsi
	movups .Lvector(%rip), %xmm0
	mov $1, %eax
	call probe_simple
	mov %eax, %edi
	mov $60, %eax
	syscall
	.global inspect_simple
inspect_simple:
	cmpl $8, (%rdi)
	jne .Lfailure
	cmpl $48, 4(%rdi)
	jne .Lfailure
	mov 16(%rdi), %r10
	cmpq $42, 8(%r10)
	jne .Lfailure
	mov 56(%r10), %r11
	movabs $0x1122334455667788, %rax
	cmp %rax, %r11
	jne .Lfailure
	mov 48(%r10), %r10
	movabs $0x4019000000000000, %rax
	cmp %rax, %r10
	jne .Lfailure
	pxor %xmm0, %xmm0
	xor %esi, %esi
	xor %eax, %eax
	ret
.Lfailure:
	mov $1, %eax
	ret
	.global inspect_stack
	.global inspect_mixed
	.global inspect_hidden
	.global inspect_vector
inspect_stack:
inspect_mixed:
inspect_hidden:
inspect_vector:
	xor %eax, %eax
	ret
