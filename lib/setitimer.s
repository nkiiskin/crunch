	.text
	# Inputs: timer kind, new state, old state. Output: zero or -1.
	.globl __crunch_setitimer
	.type __crunch_setitimer, @function
__crunch_setitimer:
	mov $38, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setitimer, .-__crunch_setitimer
	.section .note.GNU-stack,"",@progbits
