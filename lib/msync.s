	.text
	# Inputs: address, length, flags. Output: zero or -1.
	.globl __crunch_msync
	.type __crunch_msync, @function
__crunch_msync:
	mov $26, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_msync, .-__crunch_msync
	.section .note.GNU-stack,"",@progbits
