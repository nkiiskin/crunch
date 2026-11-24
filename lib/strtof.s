	.text
	# Inputs: NUL-terminated text in rdi, optional end-pointer in rsi.
	# Output: nearest binary32 value in xmm0; range errors set errno.
	.globl __crunch_strtof
	.type __crunch_strtof, @function
	.hidden _crunch_real_to_float
__crunch_strtof:
	jmp _crunch_real_to_float
	.size __crunch_strtof, .-__crunch_strtof
	.section .note.GNU-stack,"",@progbits
