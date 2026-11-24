	.text
	# Inputs: address, length, protection. Output: zero or -1.
	.globl __crunch_mprotect
	.type __crunch_mprotect, @function
__crunch_mprotect:
	mov $10, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mprotect, .-__crunch_mprotect
	.section .note.GNU-stack,"",@progbits
