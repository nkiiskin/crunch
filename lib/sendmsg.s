	.text
	# Inputs: descriptor, message header, flags. Output: byte count or -1.
	.globl __crunch_sendmsg
	.type __crunch_sendmsg, @function
__crunch_sendmsg:
	mov $46, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sendmsg, .-__crunch_sendmsg
	.section .note.GNU-stack,"",@progbits
