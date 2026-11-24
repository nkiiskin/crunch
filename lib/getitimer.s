	.text
	# Inputs: timer kind, destination. Output: zero or -1.
	.globl __crunch_getitimer
	.type __crunch_getitimer, @function
__crunch_getitimer:
	mov $36, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getitimer, .-__crunch_getitimer
	.section .note.GNU-stack,"",@progbits
