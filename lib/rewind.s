	.text
	# Input: stream in rdi. Output: none; a failed seek sets errno.
	.globl __crunch_rewind
	.type __crunch_rewind, @function
__crunch_rewind:
	push %r12
	mov %rdi, %r12
	xor %esi, %esi
	xor %edx, %edx
	call __crunch_fseek
	mov %r12, %rdi
	pop %r12
	jmp __crunch_clearerr
	.size __crunch_rewind, .-__crunch_rewind
	.section .note.GNU-stack,"",@progbits
