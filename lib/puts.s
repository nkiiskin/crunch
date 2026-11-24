	.text
	# Input: NUL-terminated string in rdi. Output: nonnegative or EOF on error.
	.globl __crunch_puts
	.type __crunch_puts, @function
__crunch_puts:
	push %r12
	mov _crunch_stdout@GOTPCREL(%rip), %r12
	mov (%r12), %r12
	mov %r12, %rsi
	call __crunch_fputs
	cmp $-1, %eax
	je .Lputs_done
	mov $10, %edi
	mov %r12, %rsi
	call __crunch_fputc
.Lputs_done:
	pop %r12
	ret
	.size __crunch_puts, .-__crunch_puts
	.section .note.GNU-stack,"",@progbits
