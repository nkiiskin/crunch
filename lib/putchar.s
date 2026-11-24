	.text
	# Input: character in edi. Output: written byte or EOF on error.
	.globl __crunch_putchar
	.type __crunch_putchar, @function
__crunch_putchar:
	mov _crunch_stdout@GOTPCREL(%rip), %rsi
	mov (%rsi), %rsi
	jmp __crunch_fputc
	.size __crunch_putchar, .-__crunch_putchar
	.section .note.GNU-stack,"",@progbits
