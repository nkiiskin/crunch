	.text
	# Inputs: path, mode. Output: zero or -1.
	.globl __crunch_chmod
	.type __crunch_chmod, @function
__crunch_chmod:
	mov $90, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_chmod, .-__crunch_chmod
	.section .note.GNU-stack,"",@progbits
