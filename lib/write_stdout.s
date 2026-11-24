	.text
	# Writes rsi bytes from the buffer at rdi to standard output.
	# Inputs: buffer and byte count. Output: zero on success or a negative error in rax.
	.globl __crunch_write_stdout
	.hidden __crunch_write_stdout
	.type __crunch_write_stdout, @function
__crunch_write_stdout:
	mov %rdi, %r8
	mov %rsi, %r9
.Lwrite_loop:
	test %r9, %r9
	jz .Lok
	mov $1, %eax
	mov $1, %edi
	mov %r8, %rsi
	mov %r9, %rdx
	syscall
	test %rax, %rax
	jle .Lerror
	add %rax, %r8
	sub %rax, %r9
	jmp .Lwrite_loop
.Lok:
	xor %eax, %eax
	ret
.Lerror:
	mov $-1, %rax
	ret
	.size __crunch_write_stdout, .-__crunch_write_stdout
	# Writes rsi bytes from rdi to standard output.
	# Inputs: buffer and byte count. Output: bytes written or -1 on failure in rax.
	.globl __crunch_write_stdout_count
	.hidden __crunch_write_stdout_count
	.type __crunch_write_stdout_count, @function
__crunch_write_stdout_count:
	push %rsi
	call __crunch_write_stdout
	pop %rcx
	test %rax, %rax
	js .Lcount_done
	mov %rcx, %rax
.Lcount_done:
	ret
	.size __crunch_write_stdout_count, .-__crunch_write_stdout_count
	.section .note.GNU-stack,"",@progbits
