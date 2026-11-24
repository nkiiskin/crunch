	.text
	# Inputs: operation, new set, old set. Output: zero or -1.
	.globl __crunch_sigprocmask
	.type __crunch_sigprocmask, @function
__crunch_sigprocmask:
	mov $8, %r10d
	mov $14, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sigprocmask, .-__crunch_sigprocmask
	.section .note.GNU-stack,"",@progbits
