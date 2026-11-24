	.text
	# Inputs: descriptor, address, length. Output: zero or -1.
	.globl __crunch_connect
	.type __crunch_connect, @function
__crunch_connect:
	mov $42, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_connect, .-__crunch_connect
	.section .note.GNU-stack,"",@progbits
