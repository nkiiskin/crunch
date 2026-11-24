	.text
	# Inputs: allocation pointer in rdi and unsigned byte count in rsi.
	# Output: resized allocation or null, preserving the original on failure.
	.globl _crunch_realloc
	.hidden _crunch_realloc
	.type _crunch_realloc, @function
	.globl __crunch_realloc
	.type __crunch_realloc, @function
_crunch_realloc:
__crunch_realloc:
	push %r12
	push %r13
	push %r14
	push %r15
	sub $8, %rsp
	mov %rdi, %r12
	mov %rsi, %r13
	test %r12, %r12
	jz .Lrealloc_malloc
	test %r13, %r13
	jz .Lrealloc_free
	movabs $0x7fffffffffffffd0, %rax
	cmp %rax, %r13
	ja .Lrealloc_overflow
	cmp -8(%r12), %r13
	jbe .Lrealloc_in_place
	cmpq $1, -32(%r12)
	je .Lrealloc_pool_grow
	mov -16(%r12), %r15
	lea 47(%r13), %r14
	and $-16, %r14
	lea -32(%r12), %rdi
	mov %r15, %rsi
	mov %r14, %rdx
	mov $1, %ecx
	call _crunch_mremap
	cmp $-1, %rax
	je .Lrealloc_failed
	mov %r13, 8(%rax)
	mov %r14, 16(%rax)
	lea -32(%r14), %rdx
	mov %rdx, 24(%rax)
	add $32, %rax
	jmp .Lrealloc_done
.Lrealloc_pool_grow:
	mov %r13, %rdi
	call _crunch_malloc
	test %rax, %rax
	jz .Lrealloc_failed
	mov %rax, %r14
	mov %rax, %rdi
	mov %r12, %rsi
	mov -24(%r12), %rdx
	call __crunch_memcpy
	mov %r12, %rdi
	call _crunch_pool_free_transactional
	test %eax, %eax
	jnz .Lrealloc_rollback
	mov %r14, %rax
	jmp .Lrealloc_done
.Lrealloc_rollback:
	call _crunch_errno_location
	mov (%rax), %r15d
	cmpq $1, -32(%r14)
	je .Lrealloc_rollback_pool
	lea -32(%r14), %rdi
	mov -16(%r14), %rsi
	call _crunch_munmap
	test %eax, %eax
	jz .Lrealloc_restore_error
	# Keep a replacement mapping reachable if rollback cannot unmap it.
	mov %r14, %rdi
	call _crunch_cache_rollback_mapping
	jmp .Lrealloc_restore_error
.Lrealloc_rollback_pool:
	mov %r14, %rdi
	call _crunch_free
.Lrealloc_restore_error:
	call _crunch_errno_location
	mov %r15d, (%rax)
	jmp .Lrealloc_failed
.Lrealloc_in_place:
	mov %r13, -24(%r12)
	mov %r12, %rax
	jmp .Lrealloc_done
.Lrealloc_malloc:
	mov %r13, %rdi
	call _crunch_malloc
	jmp .Lrealloc_done
.Lrealloc_free:
	mov %r12, %rdi
	call _crunch_free
	xor %eax, %eax
	jmp .Lrealloc_done
.Lrealloc_overflow:
	call _crunch_errno_location
	movl $12, (%rax)
.Lrealloc_failed:
	xor %eax, %eax
.Lrealloc_done:
	add $8, %rsp
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	ret
	.size _crunch_realloc, .-_crunch_realloc
	.size __crunch_realloc, .-__crunch_realloc
	.section .note.GNU-stack,"",@progbits
