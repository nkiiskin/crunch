	.text
	# Inputs: directory, path, flags in edi, rsi, edx.
	# Output: zero or negative Linux errno.
	.globl _crunch_unlinkat
	.type _crunch_unlinkat, @function
_crunch_unlinkat:
	mov $263, %eax
	syscall
	ret
	.size _crunch_unlinkat, .-_crunch_unlinkat
	.section .note.GNU-stack,"",@progbits
