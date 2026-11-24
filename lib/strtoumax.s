	.text
	# Inputs: string, optional end pointer, and base. Output: unsigned value in rax.
	.globl __crunch_strtoumax
	.type __crunch_strtoumax, @function
__crunch_strtoumax:
	xor %ecx, %ecx
	jmp _crunch_parse_integer
	.size __crunch_strtoumax, .-__crunch_strtoumax
	.section .note.GNU-stack,"",@progbits
