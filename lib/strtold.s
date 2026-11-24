	.text
	# Inputs: NUL-terminated text in rdi, optional end-pointer in rsi.
	# Output: nearest binary80 value in st(0); range errors set errno.
	.globl __crunch_strtold
	.type __crunch_strtold, @function
	.hidden _crunch_real_to_extended
__crunch_strtold:
	jmp _crunch_real_to_extended
	.size __crunch_strtold, .-__crunch_strtold
	.section .note.GNU-stack,"",@progbits
