	.text
	# Inputs: ID, command, buffer. Output: zero or -1.
	.globl __crunch_shmctl
	.type __crunch_shmctl, @function
__crunch_shmctl:
	mov $31, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_shmctl, .-__crunch_shmctl
	.section .note.GNU-stack,"",@progbits
