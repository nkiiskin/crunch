	.text
	# Inputs: destination, source, byte count. Output: destination pointer.
	.globl __crunch_memmove
	.type __crunch_memmove, @function
__crunch_memmove:
	mov %rdi, %rax
	test %rdx, %rdx
	je .Ldone
	mov %rdx, %rcx
	cmp %rsi, %rdi
	jbe .Lforward
	lea -1(%rdi,%rdx), %rdi
	lea -1(%rsi,%rdx), %rsi
	std
	rep movsb
	cld
	ret
.Lforward:
	cmp $8, %rdx
	jb .Lforward_bytes
	shr $3, %rcx
	rep movsq
	and $7, %rdx
	mov %rdx, %rcx
.Lforward_bytes:
	rep movsb
.Ldone:
	ret
	.size __crunch_memmove, .-__crunch_memmove
	.section .note.GNU-stack,"",@progbits
