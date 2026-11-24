	.text
	# Inputs: buffer in rdi, length in rsi, stream in rdx.
	# Output: bytes written or -1 on failure.
	.globl _crunch_format_write
	.hidden _crunch_format_write
	.type _crunch_format_write, @function
_crunch_format_write:
	push %r12
	mov %rsi, %r12
	mov %rdx, %rcx
	mov %rsi, %rdx
	mov $1, %esi
	call __crunch_fwrite
	cmp %r12, %rax
	je .Lformat_write_done
	mov $-1, %rax
.Lformat_write_done:
	pop %r12
	ret
	.size _crunch_format_write, .-_crunch_format_write
	.section .note.GNU-stack,"",@progbits
