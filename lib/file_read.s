	.text
	# Reads up to rdx bytes from descriptor rdi into the buffer at rsi.
	# Inputs: descriptor, buffer, byte count. Output: bytes read or -1 with errno set.
	.globl __crunch_file_read
	.type __crunch_file_read, @function
__crunch_file_read:
	# %rdi=fd, %rsi=buffer, %rdx=count
	mov $0, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_file_read, .-__crunch_file_read
	.section .note.GNU-stack,"",@progbits
