	.text
	# Input: requested break. Output: zero or -1 with errno set to ENOMEM.
	.globl __crunch_brk
	.type __crunch_brk, @function
__crunch_brk:
	mov $12, %eax
	syscall
	cmp %rdi, %rax
	jb .Lbrk_error
	xor %eax, %eax
	ret
.Lbrk_error:
	mov $-12, %rax
	jmp __crunch_syscall_result
	.size __crunch_brk, .-__crunch_brk
	.section .note.GNU-stack,"",@progbits
