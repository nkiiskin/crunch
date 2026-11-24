	.text
	# Inputs: NUL-terminated text in rdi, optional end-pointer in rsi.
	# Output: nearest binary64 value in xmm0; range errors set errno.
	.globl __crunch_strtod
	.type __crunch_strtod, @function
	.hidden _crunch_real_to_double
__crunch_strtod:
	jmp _crunch_real_to_double
	.size __crunch_strtod, .-__crunch_strtod
	.section .note.GNU-stack,"",@progbits
