	.text
	# Compares at most rdx bytes of NUL-terminated strings in rdi and rsi.
	# Inputs: left string, right string, maximum byte count. Output: signed comparison in eax.
	.globl __crunch_strncmp
	.type __crunch_strncmp, @function
__crunch_strncmp:
	test %rdx, %rdx
	je .Lequal
.Lloop:
	movzbl (%rdi), %eax
	movzbl (%rsi), %ecx
	cmp %ecx, %eax
	jne .Ldiff
	test %eax, %eax
	je .Lequal
	inc %rdi
	inc %rsi
	dec %rdx
	jnz .Lloop
.Lequal:
	xor %eax, %eax
	ret
.Ldiff:
	seta %al
	movzbl %al, %eax
	lea -1(%rax,%rax), %eax
	ret
	.size __crunch_strncmp, .-__crunch_strncmp
	.section .note.GNU-stack,"",@progbits
