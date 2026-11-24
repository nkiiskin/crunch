	.text
	# Inputs: buffer, length, flags. Output: byte count or -1.
	.globl __crunch_getrandom
	.type __crunch_getrandom, @function
__crunch_getrandom:
	mov $318, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getrandom, .-__crunch_getrandom
	.section .note.GNU-stack,"",@progbits
