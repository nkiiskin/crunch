	.text
	# Inputs: descriptor, address, length. Output: zero or -1.
	.globl __crunch_bind
	.type __crunch_bind, @function
__crunch_bind:
	mov $49, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_bind, .-__crunch_bind
	.section .note.GNU-stack,"",@progbits
