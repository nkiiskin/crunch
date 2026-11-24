	.text
	# Inputs: path, mode, stream. Output: same stream or NULL with errno set.
	.globl __crunch_freopen
	.type __crunch_freopen, @function
__crunch_freopen:
	push %r12
	push %r13
	push %r14
	push %r15
	push %rbx
	sub $48, %rsp
	mov %rdi, %r12
	mov %rsi, %r13
	mov %rdx, %r14
	test %r14, %r14
	jz .Lfreopen_invalid
	mov (%r14), %r15d
	test %r12, %r12
	jnz .Lfreopen_named
	test %r15d, %r15d
	js .Lfreopen_bad_descriptor
	# Build /proc/self/fd/N while the original descriptor is still open.
	lea 47(%rsp), %rdi
	movb $0, (%rdi)
	mov %r15d, %eax
	mov $10, %ecx
.Lfreopen_digits:
	xor %edx, %edx
	div %ecx
	add $48, %dl
	dec %rdi
	mov %dl, (%rdi)
	test %eax, %eax
	jnz .Lfreopen_digits
	lea -14(%rdi), %r12
	movabs $0x6c65732f636f7270, %rax
	mov %rax, 1(%r12)
	movl $0x64662f66, 9(%r12)
	movb $47, 13(%r12)
	movb $47, (%r12)
	jmp .Lfreopen_open
.Lfreopen_named:
	mov %r15d, %edi
	call __crunch_close
	movl $-1, (%r14)
.Lfreopen_open:
	mov %r12, %rdi
	mov %r13, %rsi
	call __crunch_fopen
	test %rax, %rax
	jz .Lfreopen_failed
	mov %rax, %rbx
	test %r15d, %r15d
	js .Lfreopen_transfer
	mov (%rbx), %edi
	cmp %r15d, %edi
	je .Lfreopen_transfer
	mov %r15d, %esi
	call __crunch_dup2
	cmp $-1, %eax
	je .Lfreopen_dup_failed
	mov (%rbx), %edi
	call __crunch_close
	mov %r15d, (%rbx)
.Lfreopen_transfer:
	mov (%rbx), %eax
	mov %eax, (%r14)
	mov 4(%rbx), %eax
	mov %eax, 4(%r14)
	movl $0, 8(%r14)
	mov %rbx, %rdi
	call __crunch_free
	mov %r14, %rax
	jmp .Lfreopen_done
.Lfreopen_dup_failed:
	call _crunch_errno_location
	mov (%rax), %r12d
	mov %rbx, %rdi
	call __crunch_fclose
	call _crunch_errno_location
	mov %r12d, (%rax)
.Lfreopen_failed:
	call _crunch_errno_location
	mov (%rax), %r12d
	mov (%r14), %edi
	test %edi, %edi
	js .Lfreopen_released
	call __crunch_close
	movl $-1, (%r14)
.Lfreopen_released:
	cmpl $0, 12(%r14)
	je .Lfreopen_restore
	mov %r14, %rdi
	call __crunch_free
.Lfreopen_restore:
	call _crunch_errno_location
	mov %r12d, (%rax)
	xor %eax, %eax
	jmp .Lfreopen_done
.Lfreopen_bad_descriptor:
	call _crunch_errno_location
	movl $9, (%rax)
	jmp .Lfreopen_failed
.Lfreopen_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	xor %eax, %eax
.Lfreopen_done:
	add $48, %rsp
	pop %rbx
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	ret
	.size __crunch_freopen, .-__crunch_freopen
	.section .note.GNU-stack,"",@progbits
