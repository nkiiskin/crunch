	.text
	# Input: none. Output: current process ID.
	.globl _crunch_getpid
	.type _crunch_getpid, @function
_crunch_getpid:
	mov $39, %eax
	syscall
	ret
	.size _crunch_getpid, .-_crunch_getpid
	.section .note.GNU-stack,"",@progbits
