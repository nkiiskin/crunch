	.text
	# Input: stream pointer in rdi. Output: zero or -1 with errno set.
	.globl __crunch_fclose
	.type __crunch_fclose, @function
__crunch_fclose:
	push %r12
	push %r13
	push %r14
	test %rdi, %rdi
	jz .Lfclose_invalid
	mov %rdi, %r12
	mov (%rdi), %edi
	call __crunch_close
	mov %eax, %r13d
	cmpl $0, 12(%r12)
	je .Lfclose_result
	call _crunch_errno_location
	mov (%rax), %r14d
	mov %r12, %rdi
	call __crunch_free
	call _crunch_errno_location
	mov %r14d, (%rax)
.Lfclose_result:
	mov %r13d, %eax
	jmp .Lfclose_done
.Lfclose_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %eax
.Lfclose_done:
	pop %r14
	pop %r13
	pop %r12
	ret
	.size __crunch_fclose, .-__crunch_fclose
	.section .note.GNU-stack,"",@progbits
