	.text
	# Parses a decimal integer from the NUL-terminated string at rdi.
	# Input: string pointer. Output: parsed int value in rax.
	.globl __crunch_atoi
	.type __crunch_atoi, @function
__crunch_atoi:
	sub $8, %rsp
	call __crunch_atol
	add $8, %rsp
	movslq %eax, %rax
	ret
	.size __crunch_atoi, .-__crunch_atoi
	.section .note.GNU-stack,"",@progbits
