	.text
	# Inputs: output, input, offset pointer, count. Output: count or -1.
	.globl __crunch_sendfile
	.type __crunch_sendfile, @function
__crunch_sendfile:
	mov %rcx, %r10
	mov $40, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sendfile, .-__crunch_sendfile
	.section .note.GNU-stack,"",@progbits
