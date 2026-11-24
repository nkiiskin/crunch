	.text
	# Input: attachment address. Output: zero or -1.
	.globl __crunch_shmdt
	.type __crunch_shmdt, @function
__crunch_shmdt:
	mov $67, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_shmdt, .-__crunch_shmdt
	.section .note.GNU-stack,"",@progbits
