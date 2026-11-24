	.text
	# Inputs: domain, type, protocol, descriptor array. Output: zero or -1.
	.globl __crunch_socketpair
	.type __crunch_socketpair, @function
__crunch_socketpair:
	mov %rcx, %r10
	mov $53, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_socketpair, .-__crunch_socketpair
	.section .note.GNU-stack,"",@progbits
