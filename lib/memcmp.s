	.text
	# Compares rdx bytes at rdi and rsi.
	# Inputs: left pointer, right pointer, byte count. Output: signed comparison in eax.
	.globl __crunch_memcmp
	.type __crunch_memcmp, @function
__crunch_memcmp:
	test %rdx, %rdx
	jle .Lequal
.Lloop:
	movzbl (%rdi), %eax
	movzbl (%rsi), %ecx
	cmp %ecx, %eax
	jne .Ldiff
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
	.size __crunch_memcmp, .-__crunch_memcmp
	.section .note.GNU-stack,"",@progbits
