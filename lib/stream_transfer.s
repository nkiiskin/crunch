	.text
	# Inputs: buffer, item size, count, stream, write flag in rdi, rsi, rdx, rcx, r8.
	# Output: number of complete items transferred; errors set errno and stream status.
	.globl _crunch_stream_transfer
	.hidden _crunch_stream_transfer
	.type _crunch_stream_transfer, @function
_crunch_stream_transfer:
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	sub $16, %rsp
	mov %r8d, (%rsp)
	mov %rcx, %r12
	mov %rdi, %r13
	mov %rsi, %r14
	xor %ebx, %ebx
	test %rsi, %rsi
	jz .Ltransfer_zero
	test %rdx, %rdx
	jz .Ltransfer_zero
	test %r12, %r12
	jz .Ltransfer_invalid
	test %r13, %r13
	jz .Ltransfer_invalid
	mov %rsi, %rax
	mul %rdx
	jo .Ltransfer_overflow
	test %rax, %rax
	js .Ltransfer_overflow
	mov %rax, %r15
	cmpl $0, (%rsp)
	jne .Ltransfer_loop
	testl $4, 8(%r12)
	jz .Ltransfer_check_eof
	mov 8(%r12), %eax
	shr $8, %eax
	mov %al, (%r13)
	andl $-65285, 8(%r12)
	mov $1, %ebx
	cmp %r15, %rbx
	je .Ltransfer_items
.Ltransfer_check_eof:
	testl $1, 8(%r12)
	jnz .Ltransfer_items
.Ltransfer_loop:
	mov (%r12), %edi
	lea (%r13,%rbx), %rsi
	mov %r15, %rdx
	sub %rbx, %rdx
	cmp $0x7ffff000, %rdx
	jbe .Ltransfer_count_ready
	mov $0x7ffff000, %edx
.Ltransfer_count_ready:
	cmpl $0, (%rsp)
	jne .Ltransfer_write
	call __crunch_read
	jmp .Ltransfer_result
.Ltransfer_write:
	call __crunch_write
.Ltransfer_result:
	cmp $-1, %rax
	je .Ltransfer_error
	test %rax, %rax
	jz .Ltransfer_end
	add %rax, %rbx
	cmp %r15, %rbx
	jb .Ltransfer_loop
	jmp .Ltransfer_items
.Ltransfer_end:
	cmpl $0, (%rsp)
	jne .Ltransfer_no_progress
	orl $1, 8(%r12)
	jmp .Ltransfer_items
.Ltransfer_no_progress:
	call _crunch_errno_location
	movl $5, (%rax)
.Ltransfer_error:
	orl $2, 8(%r12)
.Ltransfer_items:
	mov %rbx, %rax
	xor %edx, %edx
	div %r14
	jmp .Ltransfer_done
.Ltransfer_overflow:
	call _crunch_errno_location
	movl $75, (%rax)
	jmp .Ltransfer_invalid_status
.Ltransfer_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
.Ltransfer_invalid_status:
	test %r12, %r12
	jz .Ltransfer_zero
	orl $2, 8(%r12)
.Ltransfer_zero:
	xor %eax, %eax
.Ltransfer_done:
	add $16, %rsp
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	ret
	.size _crunch_stream_transfer, .-_crunch_stream_transfer
	.section .note.GNU-stack,"",@progbits
