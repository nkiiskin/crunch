	.text
	# Input: signal set pointer. Output: zero or -1.
	.globl __crunch_sigemptyset
	.type __crunch_sigemptyset, @function
__crunch_sigemptyset:
	test %rdi, %rdi
	jz .Lsigempty_invalid
	movq $0, (%rdi)
	xor %eax, %eax
	ret
.Lsigempty_invalid:
	mov $-22, %rax
	jmp __crunch_syscall_result
	.size __crunch_sigemptyset, .-__crunch_sigemptyset
	.section .note.GNU-stack,"",@progbits
