	.text
	# Inputs: directory, path, flags, mode in edi, rsi, edx, ecx.
	# Output: descriptor or negative Linux errno.
	.globl _crunch_openat
	.type _crunch_openat, @function
_crunch_openat:
	mov %rcx, %r10
	mov $257, %eax
	syscall
	ret
	.size _crunch_openat, .-_crunch_openat
	.section .note.GNU-stack,"",@progbits
