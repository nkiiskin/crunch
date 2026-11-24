	.text
	# Inputs: object count in rdi, object size in rsi. Output: zeroed allocation or null.
	.globl __crunch_calloc
	.type __crunch_calloc, @function
__crunch_calloc:
	push %r12
	mov %rdi, %rax
	mul %rsi
	jo .Lcalloc_overflow
	mov %rax, %r12
	mov %rax, %rdi
	call _crunch_malloc
	test %rax, %rax
	je .Lcalloc_done
	mov %rax, %rdi
	xor %esi, %esi
	mov %r12, %rdx
	call __crunch_memset
	jmp .Lcalloc_done
.Lcalloc_overflow:
	call _crunch_errno_location
	movl $12, (%rax)
	xor %eax, %eax
.Lcalloc_done:
	pop %r12
	ret
	.size __crunch_calloc, .-__crunch_calloc
	.section .note.GNU-stack,"",@progbits
