	.text
	# Inputs: requested and remaining timespec pointers. Output: zero or -1.
	.globl __crunch_nanosleep
	.type __crunch_nanosleep, @function
__crunch_nanosleep:
	mov $35, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_nanosleep, .-__crunch_nanosleep
	.section .note.GNU-stack,"",@progbits
