	.text
	# Inputs: buffer, byte count, position pointer, entry pointer in rdi, rsi, rdx, rcx.
	# Output: one for an entry, zero at end, or -1 with errno set.
	.globl __crunch_dirent64_next
	.type __crunch_dirent64_next, @function
__crunch_dirent64_next:
	test %rdx, %rdx
	jz .Linvalid
	test %rcx, %rcx
	jz .Linvalid
	test %rsi, %rsi
	js .Linvalid
	mov (%rdx), %r8
	test %r8, %r8
	js .Linvalid
	cmp %rsi, %r8
	ja .Linvalid
	je .Lend
	test %rdi, %rdi
	jz .Linvalid
	sub %r8, %rsi
	cmp $20, %rsi
	jb .Linvalid
	add %r8, %rdi
	jc .Linvalid
	movzwl 16(%rdi), %r9d
	cmp $20, %r9
	jb .Linvalid
	cmp %rsi, %r9
	ja .Linvalid
	lea 19(%rdi), %r10
	mov %r10, %rax
	lea (%rdi,%r9), %rsi
	cmp %rdi, %rsi
	jb .Linvalid
.Lscan:
	cmp %rsi, %rax
	jae .Linvalid
	cmpb $0, (%rax)
	je .Lfound
	inc %rax
	jmp .Lscan
.Lfound:
	sub %r10, %rax
	mov %rax, 24(%rcx)
	mov %r10, 16(%rcx)
	mov (%rdi), %rax
	mov %rax, (%rcx)
	mov 8(%rdi), %rax
	mov %rax, 8(%rcx)
	movzbl 18(%rdi), %eax
	mov %eax, 32(%rcx)
	add %r9, %r8
	mov %r8, (%rdx)
	mov $1, %eax
	ret
.Lend:
	xor %eax, %eax
	ret
.Linvalid:
	mov $-22, %rax
	jmp __crunch_syscall_result
	.size __crunch_dirent64_next, .-__crunch_dirent64_next
	.section .note.GNU-stack,"",@progbits
