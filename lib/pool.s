	.bss
	.align 8
.Lpool_classes:
	.zero 96
.Lrollback_mappings:
	.zero 8

	.text
	# Input: mapped allocation in rdi. Output: retains it for reuse after failed rollback unmap.
	.globl _crunch_cache_rollback_mapping
	.hidden _crunch_cache_rollback_mapping
	.type _crunch_cache_rollback_mapping, @function
_crunch_cache_rollback_mapping:
	sub $32, %rdi
	mov .Lrollback_mappings(%rip), %rax
	mov %rax, 8(%rdi)
	mov %rdi, .Lrollback_mappings(%rip)
	ret
	.size _crunch_cache_rollback_mapping, .-_crunch_cache_rollback_mapping

	# Input: requested size in rdi. Output: retained mapping or null.
	.globl _crunch_take_rollback_mapping
	.hidden _crunch_take_rollback_mapping
	.type _crunch_take_rollback_mapping, @function
_crunch_take_rollback_mapping:
	lea .Lrollback_mappings(%rip), %rdx
.Lrollback_search:
	mov (%rdx), %rax
	test %rax, %rax
	jz .Lrollback_not_found
	cmp %rdi, 24(%rax)
	jae .Lrollback_found
	lea 8(%rax), %rdx
	jmp .Lrollback_search
.Lrollback_found:
	mov 8(%rax), %rcx
	mov %rcx, (%rdx)
	mov %rdi, 8(%rax)
	add $32, %rax
	ret
.Lrollback_not_found:
	ret
	.size _crunch_take_rollback_mapping, .-_crunch_take_rollback_mapping

	# Input: none. Output: zero, or -1 with the first unmap error in errno.
	.globl _crunch_allocator_trim
	.hidden _crunch_allocator_trim
	.type _crunch_allocator_trim, @function
	.globl __crunch_allocator_trim
	.type __crunch_allocator_trim, @function
_crunch_allocator_trim:
__crunch_allocator_trim:
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	sub $16, %rsp
	xor %r14d, %r14d
	lea .Lrollback_mappings(%rip), %rbx
.Ltrim_mapping:
	mov (%rbx), %r12
	test %r12, %r12
	jz .Ltrim_start_arenas
	mov 8(%r12), %r13
	mov %r12, %rdi
	mov 16(%r12), %rsi
	call _crunch_munmap
	test %eax, %eax
	jnz .Ltrim_mapping_failed
	mov %r13, (%rbx)
	jmp .Ltrim_mapping
.Ltrim_mapping_failed:
	test %r14d, %r14d
	jnz .Ltrim_mapping_keep
	call _crunch_errno_location
	mov (%rax), %r14d
.Ltrim_mapping_keep:
	lea 8(%r12), %rbx
	jmp .Ltrim_mapping
.Ltrim_start_arenas:
	lea .Lpool_classes(%rip), %r15
.Ltrim_class:
	mov %r15, %rbx
	xor %r12d, %r12d
.Ltrim_arena:
	mov (%rbx), %r13
	test %r13, %r13
	jz .Ltrim_next_class
	mov 24(%r13), %rax
	cmp 48(%r13), %rax
	jne .Ltrim_arena_keep
	mov 0(%r13), %rax
	mov %rax, (%rsp)
	mov %r13, %rdi
	mov $65536, %esi
	call _crunch_munmap
	test %eax, %eax
	jnz .Ltrim_arena_failed
	mov (%rsp), %rax
	mov %rax, (%rbx)
	test %rax, %rax
	jz .Ltrim_arena
	mov %r12, 8(%rax)
	jmp .Ltrim_arena
.Ltrim_arena_failed:
	test %r14d, %r14d
	jnz .Ltrim_arena_keep
	call _crunch_errno_location
	mov (%rax), %r14d
.Ltrim_arena_keep:
	mov %r13, %r12
	lea 0(%r13), %rbx
	jmp .Ltrim_arena
.Ltrim_next_class:
	add $8, %r15
	lea .Lrollback_mappings(%rip), %rax
	cmp %rax, %r15
	jne .Ltrim_class
	test %r14d, %r14d
	jz .Ltrim_success
	call _crunch_errno_location
	mov %r14d, (%rax)
	mov $-1, %eax
	jmp .Ltrim_done
.Ltrim_success:
	xor %eax, %eax
.Ltrim_done:
	add $16, %rsp
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	ret
	.size _crunch_allocator_trim, .-_crunch_allocator_trim
	.size __crunch_allocator_trim, .-__crunch_allocator_trim

	# Input: requested size in rdi (0..256). Output: allocation or null with errno set.
	.globl _crunch_pool_alloc
	.hidden _crunch_pool_alloc
	.type _crunch_pool_alloc, @function
_crunch_pool_alloc:
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	mov %rdi, %r12
	# Class capacities are 16..128 in steps of 16, then 160..256 in steps of 32.
	cmp $128, %rdi
	ja .Lpool_large_class
	lea 15(%rdi), %rax
	shr $4, %rax
	test %rax, %rax
	jnz .Lpool_small_nonzero
	inc %rax
.Lpool_small_nonzero:
	dec %rax
	jmp .Lpool_class_selected
.Lpool_large_class:
	lea -129(%rdi), %rax
	shr $5, %rax
	add $8, %rax
.Lpool_class_selected:
	lea .Lpool_classes(%rip), %r13
	lea (%r13,%rax,8), %r13
	cmp $8, %rax
	jae .Lpool_large_capacity
	inc %rax
	shl $4, %rax
	jmp .Lpool_capacity_selected
.Lpool_large_capacity:
	sub $7, %rax
	shl $5, %rax
	add $128, %rax
.Lpool_capacity_selected:
	mov %rax, %r14
	mov (%r13), %rbx
	test %rbx, %rbx
	jnz .Lpool_pop

	xor %edi, %edi
	mov $65536, %esi
	mov $3, %edx
	mov $34, %ecx
	mov $-1, %r8
	xor %r9d, %r9d
	call _crunch_mmap
	cmp $-1, %rax
	je .Lpool_alloc_failed
	mov %rax, %rbx
	movq $0, 0(%rbx)
	movq $0, 8(%rbx)
	movq $0, 16(%rbx)
	mov %r14, 32(%rbx)
	mov %r13, 40(%rbx)
	lea 32(%r14), %r15
	mov $65472, %eax
	xor %edx, %edx
	div %r15
	mov %rax, 24(%rbx)
	mov %rax, 48(%rbx)
	lea 64(%rbx), %rcx
.Lpool_initialize:
	mov 16(%rbx), %rdx
	mov %rdx, (%rcx)
	mov %rcx, 16(%rbx)
	add %r15, %rcx
	dec %rax
	jnz .Lpool_initialize
	mov %rbx, (%r13)
.Lpool_pop:
	mov 16(%rbx), %rax
	mov (%rax), %rdx
	mov %rdx, 16(%rbx)
	decq 24(%rbx)
	jnz .Lpool_set_header
	mov 0(%rbx), %rdx
	mov %rdx, (%r13)
	test %rdx, %rdx
	jz .Lpool_clear_links
	movq $0, 8(%rdx)
.Lpool_clear_links:
	movq $0, 0(%rbx)
	movq $0, 8(%rbx)
.Lpool_set_header:
	movq $1, 0(%rax)
	mov %r12, 8(%rax)
	mov %rbx, 16(%rax)
	mov %r14, 24(%rax)
	add $32, %rax
	jmp .Lpool_alloc_done
.Lpool_alloc_failed:
	xor %eax, %eax
.Lpool_alloc_done:
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	ret
	.size _crunch_pool_alloc, .-_crunch_pool_alloc

	# Input: live pool allocation in rdi. Output: none; unmaps an empty arena.
	.globl _crunch_pool_free
	.hidden _crunch_pool_free
	.type _crunch_pool_free, @function
_crunch_pool_free:
	xor %esi, %esi
	jmp .Lpool_free_common
	.size _crunch_pool_free, .-_crunch_pool_free

	# Input: live pool allocation in rdi. Output: zero, or -1 with original block intact.
	.globl _crunch_pool_free_transactional
	.hidden _crunch_pool_free_transactional
	.type _crunch_pool_free_transactional, @function
_crunch_pool_free_transactional:
	mov $1, %esi
.Lpool_free_common:
	push %rbx
	push %r12
	push %r13
	push %r14
	sub $8, %rsp
	mov %esi, %r14d
	lea -32(%rdi), %r13
	mov -16(%rdi), %rbx
	mov %r13, %rax
	mov 16(%rbx), %rdx
	mov %rdx, (%rax)
	mov %rax, 16(%rbx)
	mov 24(%rbx), %rax
	incq 24(%rbx)
	test %rax, %rax
	jnz .Lpool_check_empty
	mov 40(%rbx), %r12
	mov (%r12), %rdx
	mov %rdx, 0(%rbx)
	movq $0, 8(%rbx)
	test %rdx, %rdx
	jz .Lpool_insert_head
	mov %rbx, 8(%rdx)
.Lpool_insert_head:
	mov %rbx, (%r12)
.Lpool_check_empty:
	mov 24(%rbx), %rax
	cmp 48(%rbx), %rax
	jne .Lpool_free_done
	mov 40(%rbx), %r12
	mov 0(%rbx), %rdx
	mov 8(%rbx), %rcx
	test %rcx, %rcx
	jz .Lpool_remove_head
	mov %rdx, 0(%rcx)
	jmp .Lpool_removed_previous
.Lpool_remove_head:
	mov %rdx, (%r12)
.Lpool_removed_previous:
	test %rdx, %rdx
	jz .Lpool_unmap
	mov %rcx, 8(%rdx)
.Lpool_unmap:
	mov %rbx, %rdi
	mov $65536, %esi
	call _crunch_munmap
	test %eax, %eax
	jz .Lpool_free_done
	# Keep the arena available if munmap failed.
	mov (%r12), %rdx
	mov %rdx, 0(%rbx)
	movq $0, 8(%rbx)
	test %rdx, %rdx
	jz .Lpool_restore_head
	mov %rbx, 8(%rdx)
.Lpool_restore_head:
	mov %rbx, (%r12)
	test %r14d, %r14d
	jz .Lpool_free_failed
	# Undo the free so a failed realloc retains its original allocation.
	mov (%r13), %rdx
	mov %rdx, 16(%rbx)
	decq 24(%rbx)
	movq $1, (%r13)
	cmpq $0, 24(%rbx)
	jne .Lpool_free_failed
	mov 0(%rbx), %rdx
	mov %rdx, (%r12)
	test %rdx, %rdx
	jz .Lpool_free_failed
	movq $0, 8(%rdx)
.Lpool_free_failed:
	mov $-1, %eax
	jmp .Lpool_free_return
.Lpool_free_done:
	xor %eax, %eax
.Lpool_free_return:
	add $8, %rsp
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	ret
	.size _crunch_pool_free_transactional, .-_crunch_pool_free_transactional
	.section .note.GNU-stack,"",@progbits
