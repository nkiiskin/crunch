	.text
	# Writes up to rdx bytes from the buffer at rsi to descriptor rdi.
	# Inputs: descriptor, buffer, byte count. Output: bytes written or -1 with errno set.
	.globl __crunch_file_write
	.type __crunch_file_write, @function
__crunch_file_write:
	# %rdi=fd, %rsi=buffer, %rdx=count
	mov $1, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_file_write, .-__crunch_file_write
	.section .note.GNU-stack,"",@progbits
