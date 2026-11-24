	.text
	# Inputs: path, argument vector, environment vector. Output: -1 on error.
	.globl __crunch_execve
	.type __crunch_execve, @function
__crunch_execve:
	mov $59, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_execve, .-__crunch_execve
	.section .note.GNU-stack,"",@progbits
