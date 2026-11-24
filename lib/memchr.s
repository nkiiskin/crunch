	.text
	# Inputs: memory pointer, byte value, and length. Output: match pointer or null.
	.globl __crunch_memchr
	.type __crunch_memchr, @function
__crunch_memchr:
	test %rdx, %rdx
	je .Lmemchr_not_found
.Lmemchr_loop:
	cmp %sil, (%rdi)
	je .Lmemchr_found
	inc %rdi
	dec %rdx
	jne .Lmemchr_loop
.Lmemchr_not_found:
	xor %eax, %eax
	ret
.Lmemchr_found:
	mov %rdi, %rax
	ret
	.size __crunch_memchr, .-__crunch_memchr
	.section .note.GNU-stack,"",@progbits
