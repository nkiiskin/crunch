	.text
	# Inputs: string, optional end pointer, and base. Output: signed value in rax.
	.globl __crunch_strtoll
	.type __crunch_strtoll, @function
__crunch_strtoll:
	mov $8, %ecx
	jmp _crunch_parse_integer
	.size __crunch_strtoll, .-__crunch_strtoll
	.section .note.GNU-stack,"",@progbits
