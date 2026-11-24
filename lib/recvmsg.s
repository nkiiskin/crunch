	.text
	# Inputs: descriptor, message header, flags. Output: byte count or -1.
	.globl __crunch_recvmsg
	.type __crunch_recvmsg, @function
__crunch_recvmsg:
	mov $47, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_recvmsg, .-__crunch_recvmsg
	.section .note.GNU-stack,"",@progbits
