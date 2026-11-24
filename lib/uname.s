	.text
	# Input: identification structure. Output: zero or -1.
	.globl __crunch_uname
	.type __crunch_uname, @function
__crunch_uname:
	mov $63, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_uname, .-__crunch_uname
	.section .note.GNU-stack,"",@progbits
