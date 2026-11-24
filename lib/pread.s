	.text
	# Inputs: descriptor, buffer, length, offset. Output: byte count or -1.
	.globl __crunch_pread
	.type __crunch_pread, @function
__crunch_pread:
	mov %rcx, %r10
	mov $17, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_pread, .-__crunch_pread
	.section .note.GNU-stack,"",@progbits
