	.text
	# Inputs: descriptor, mode. Output: zero or -1.
	.globl __crunch_fchmod
	.type __crunch_fchmod, @function
__crunch_fchmod:
	mov $91, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fchmod, .-__crunch_fchmod
	.section .note.GNU-stack,"",@progbits
