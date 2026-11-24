	.text
	# Inputs: function, stack top, flags, argument. Output: child ID or -1.
	.globl __crunch_clone
	.type __crunch_clone, @function
__crunch_clone:
	test %rdi, %rdi
	jz .Lclone_invalid
	test %rsi, %rsi
	jz .Lclone_invalid
	test $0x01381000, %edx
	jnz .Lclone_invalid
	and $-16, %rsi
	sub $16, %rsi
	mov %rdi, 0(%rsi)
	mov %rcx, 8(%rsi)
	mov %edx, %edi
	xor %edx, %edx
	xor %r10d, %r10d
	xor %r8d, %r8d
	mov $56, %eax
	syscall
	test %rax, %rax
	jz .Lclone_child
	jmp __crunch_syscall_result
.Lclone_child:
	pop %rax
	pop %rdi
	call *%rax
	mov %eax, %edi
	mov $60, %eax
	syscall
	ud2
.Lclone_invalid:
	mov $-22, %rax
	jmp __crunch_syscall_result
	.size __crunch_clone, .-__crunch_clone
	.section .note.GNU-stack,"",@progbits
