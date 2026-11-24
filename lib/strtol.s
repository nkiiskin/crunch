	.text
	# Inputs: NUL-terminated string in rdi, optional end pointer in rsi, base in edx.
	# Output: signed long in rax; overflow sets ERANGE, invalid base sets EINVAL.
	.globl __crunch_strtol
	.type __crunch_strtol, @function
__crunch_strtol:
	mov $8, %ecx
	jmp _crunch_parse_integer
	.size __crunch_strtol, .-__crunch_strtol
	.section .note.GNU-stack,"",@progbits
