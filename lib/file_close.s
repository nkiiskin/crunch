	.text
	# Closes descriptor rdi.
	# Input: descriptor. Output: zero on success or -1 with positive errno set.
	.globl __crunch_file_close
	.type __crunch_file_close, @function
__crunch_file_close:
	mov $3, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_file_close, .-__crunch_file_close
	.section .note.GNU-stack,"",@progbits
