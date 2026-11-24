	.text
	# Inputs: buffer, item size, count, stream in rdi, rsi, rdx, rcx. Output: complete items.
	.globl __crunch_fwrite
	.type __crunch_fwrite, @function
__crunch_fwrite:
	mov $1, %r8d
	jmp _crunch_stream_transfer
	.size __crunch_fwrite, .-__crunch_fwrite
	.section .note.GNU-stack,"",@progbits
