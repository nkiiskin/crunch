	.text
	# Opens the path at rdi using mode esi.
	# Inputs: path and mode. Output: descriptor in rax, or -1 with positive errno set.
	.globl __crunch_file_open
	.type __crunch_file_open, @function
__crunch_file_open:
	mov $0x1a4, %edx
	# %rdi=path, %rsi=flags
	mov $2, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_file_open, .-__crunch_file_open
	.section .note.GNU-stack,"",@progbits
