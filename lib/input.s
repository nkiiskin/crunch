	.text
	# Reads one stdin line, discarding characters beyond the requested maximum.
	# Inputs: writable buffer in rdi (at least max_length + 1 bytes), maximum in esi.
	# Output: stored byte count in eax, or -1 with errno set; valid buffers are NUL-terminated.
	.globl __crunch_input
	.type __crunch_input, @function
__crunch_input:
	test %rdi, %rdi
	jz .Linvalid
	test %esi, %esi
	js .Linvalid
	mov %rdi, %r8
	mov %esi, %r9d
	xor %r10d, %r10d
	movb $0, (%r8)
	sub $8, %rsp
.Lread:
	xor %eax, %eax
	xor %edi, %edi
	mov %rsp, %rsi
	mov $1, %edx
	syscall
	cmp $-4, %rax
	je .Lread
	test %rax, %rax
	js .Lerror
	jz .Ldone
	movzbl (%rsp), %eax
	cmp $10, %al
	je .Ldone
	cmp %r9d, %r10d
	jae .Lread
	mov %al, (%r8,%r10)
	inc %r10d
	jmp .Lread
.Ldone:
	movb $0, (%r8,%r10)
	mov %r10d, %eax
	add $8, %rsp
	ret
.Lerror:
	movb $0, (%r8,%r10)
	add $8, %rsp
	jmp __crunch_syscall_result
.Linvalid:
	mov $-22, %rax
	jmp __crunch_syscall_result
	.size __crunch_input, .-__crunch_input
	.section .note.GNU-stack,"",@progbits
