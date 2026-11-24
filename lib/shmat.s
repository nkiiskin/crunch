	.text
	# Inputs: ID, address, flags. Output: attachment or -1.
	.globl __crunch_shmat
	.type __crunch_shmat, @function
__crunch_shmat:
	mov $30, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_shmat, .-__crunch_shmat
	.section .note.GNU-stack,"",@progbits
