	.text
	# Inputs: descriptor, buffer, length, offset. Output: byte count or -1.
	.globl __crunch_pwrite
	.type __crunch_pwrite, @function
__crunch_pwrite:
	mov %rcx, %r10
	mov $18, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_pwrite, .-__crunch_pwrite
	.section .note.GNU-stack,"",@progbits
