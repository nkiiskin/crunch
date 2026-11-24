	.text
	# Inputs: buffer, item size, count, stream in rdi, rsi, rdx, rcx. Output: complete items.
	.globl __crunch_fread
	.type __crunch_fread, @function
__crunch_fread:
	xor %r8d, %r8d
	jmp _crunch_stream_transfer
	.size __crunch_fread, .-__crunch_fread
	.section .note.GNU-stack,"",@progbits
