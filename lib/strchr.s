	.text
	# Finds the first byte matching esi in the NUL-terminated string at rdi.
	# Inputs: string pointer and byte value. Output: match pointer or NULL in rax.
	.globl __crunch_strchr
	.type __crunch_strchr, @function
__crunch_strchr:
	movzbl %sil, %esi
.Lloop:
	movzbl (%rdi), %eax
	cmp %esi, %eax
	je .Lfound
	test %eax, %eax
	je .Lnone
	inc %rdi
	jmp .Lloop
.Lfound:
	mov %rdi, %rax
	ret
.Lnone:
	xor %eax, %eax
	ret
	.size __crunch_strchr, .-__crunch_strchr
	.section .note.GNU-stack,"",@progbits
