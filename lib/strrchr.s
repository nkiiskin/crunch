	.text
	# Inputs: string pointer and byte value. Output: last match pointer or NULL.
	.globl __crunch_strrchr
	.type __crunch_strrchr, @function
__crunch_strrchr:
	movzbl %sil, %esi
	xor %edx, %edx
.Lloop:
	movzbl (%rdi), %eax
	cmp %esi, %eax
	jne .Lcontinue
	mov %rdi, %rdx
.Lcontinue:
	test %eax, %eax
	je .Ldone
	inc %rdi
	jmp .Lloop
.Ldone:
	mov %rdx, %rax
	ret
	.size __crunch_strrchr, .-__crunch_strrchr
	.section .note.GNU-stack,"",@progbits
