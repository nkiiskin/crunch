	.text
	# Inputs: character and stream. Output: written byte or EOF on error.
	.globl __crunch_putc
	.type __crunch_putc, @function
__crunch_putc:
	jmp __crunch_fputc
	.size __crunch_putc, .-__crunch_putc
	.section .note.GNU-stack,"",@progbits
