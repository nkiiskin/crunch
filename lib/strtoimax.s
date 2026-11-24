	.text
	# Inputs: string, optional end pointer, and base. Output: signed value in rax.
	.globl __crunch_strtoimax
	.type __crunch_strtoimax, @function
__crunch_strtoimax:
	mov $8, %ecx
	jmp _crunch_parse_integer
	.size __crunch_strtoimax, .-__crunch_strtoimax
	.section .note.GNU-stack,"",@progbits
