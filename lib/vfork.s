	.text
	# Input: none. Output: child PID, zero in child, or -1.
	.globl __crunch_vfork
	.type __crunch_vfork, @function
__crunch_vfork:
	pop %rdi
	mov $58, %eax
	syscall
	push %rdi
	jmp __crunch_syscall_result
	.size __crunch_vfork, .-__crunch_vfork
	.section .note.GNU-stack,"",@progbits
