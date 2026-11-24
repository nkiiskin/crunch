	.text
	# Input: stream pointer or NULL. Output: zero; streams have no buffered data.
	.globl __crunch_fflush
	.type __crunch_fflush, @function
__crunch_fflush:
	xor %eax, %eax
	ret
	.size __crunch_fflush, .-__crunch_fflush
	.section .note.GNU-stack,"",@progbits
