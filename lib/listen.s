	.text
	# Inputs: descriptor, backlog. Output: zero or -1.
	.globl __crunch_listen
	.type __crunch_listen, @function
__crunch_listen:
	mov $50, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_listen, .-__crunch_listen
	.section .note.GNU-stack,"",@progbits
