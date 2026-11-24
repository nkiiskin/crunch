	.text
	# Inputs: NUL-terminated string in rdi, optional end pointer in rsi, base in edx.
	# Output: unsigned long in rax; overflow sets ERANGE, invalid base sets EINVAL.
	.globl __crunch_strtoul
	.type __crunch_strtoul, @function
__crunch_strtoul:
	xor %ecx, %ecx
	jmp _crunch_parse_integer
	.size __crunch_strtoul, .-__crunch_strtoul
	.section .note.GNU-stack,"",@progbits
