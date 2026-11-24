	.text
	# Input: none. Output: -1 with errno set after a signal.
	.globl __crunch_pause
	.type __crunch_pause, @function
__crunch_pause:
	mov $34, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_pause, .-__crunch_pause
	.section .note.GNU-stack,"",@progbits
