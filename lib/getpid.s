	.text
	# Input: none. Output: current process ID.
	.globl __crunch_getpid
	.type __crunch_getpid, @function
__crunch_getpid:
	jmp _crunch_getpid
	.size __crunch_getpid, .-__crunch_getpid
	.section .note.GNU-stack,"",@progbits
