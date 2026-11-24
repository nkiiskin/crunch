	.text
	# Inputs: address, length, advice. Output: zero or -1.
	.globl __crunch_madvise
	.type __crunch_madvise, @function
__crunch_madvise:
	mov $28, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_madvise, .-__crunch_madvise
	.section .note.GNU-stack,"",@progbits
