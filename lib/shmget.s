	.text
	# Inputs: key, size, flags. Output: segment ID or -1.
	.globl __crunch_shmget
	.type __crunch_shmget, @function
__crunch_shmget:
	mov $29, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_shmget, .-__crunch_shmget
	.section .note.GNU-stack,"",@progbits
