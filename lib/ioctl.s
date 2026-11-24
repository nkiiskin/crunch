	.text
	# Inputs: descriptor, request, request argument. Output: result or -1.
	.globl __crunch_ioctl
	.type __crunch_ioctl, @function
	.globl __crunch_ioctl_value
	.type __crunch_ioctl_value, @function
__crunch_ioctl:
__crunch_ioctl_value:
	mov $16, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_ioctl, .-__crunch_ioctl
	.size __crunch_ioctl_value, .-__crunch_ioctl_value
	.section .note.GNU-stack,"",@progbits
