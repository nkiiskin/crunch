	.text
	# Inputs: string, optional end pointer, and base. Output: unsigned value in rax.
	.globl __crunch_strtoull
	.type __crunch_strtoull, @function
__crunch_strtoull:
	xor %ecx, %ecx
	jmp _crunch_parse_integer
	.size __crunch_strtoull, .-__crunch_strtoull
	.section .note.GNU-stack,"",@progbits
