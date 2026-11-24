	.text
	# Input: unsigned byte count in rdi. Output: aligned allocation or null with errno set.
	.globl _crunch_malloc
	.hidden _crunch_malloc
	.type _crunch_malloc, @function
	.globl __crunch_malloc
	.type __crunch_malloc, @function
_crunch_malloc:
__crunch_malloc:
	push %r12
	push %r13
	sub $8, %rsp
	mov %rdi, %r12
	cmp $256, %rdi
	ja .Lmalloc_mapping
	call _crunch_pool_alloc
	jmp .Lmalloc_done
.Lmalloc_mapping:
	movabs $0x7fffffffffffffd0, %rax
	cmp %rax, %rdi
	ja .Lmalloc_overflow
	call _crunch_take_rollback_mapping
	test %rax, %rax
	jnz .Lmalloc_done
	mov %r12, %rdi
	lea 47(%rdi), %r13
	and $-16, %r13
	xor %edi, %edi
	mov %r13, %rsi
	mov $3, %edx
	mov $34, %ecx
	mov $-1, %r8
	xor %r9d, %r9d
	call _crunch_mmap
	cmp $-1, %rax
	je .Lmalloc_failed
	movq $2, (%rax)
	mov %r12, 8(%rax)
	mov %r13, 16(%rax)
	mov %r13, %rdx
	sub $32, %rdx
	mov %rdx, 24(%rax)
	add $32, %rax
	jmp .Lmalloc_done
.Lmalloc_overflow:
	call _crunch_errno_location
	movl $12, (%rax)
.Lmalloc_failed:
	xor %eax, %eax
.Lmalloc_done:
	add $8, %rsp
	pop %r13
	pop %r12
	ret
	.size _crunch_malloc, .-_crunch_malloc
	.size __crunch_malloc, .-__crunch_malloc
	.section .note.GNU-stack,"",@progbits
