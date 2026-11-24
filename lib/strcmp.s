	.text
	# Compares NUL-terminated strings in rdi and rsi.
	# Inputs: left string, right string. Output: signed comparison in eax.
	.globl __crunch_strcmp
	.type __crunch_strcmp, @function
__crunch_strcmp:
.Lloop:
	movzbl (%rdi), %eax
	movzbl (%rsi), %edx
	cmp %edx, %eax
	jne .Ldiff
	test %eax, %eax
	je .Lequal
	inc %rdi
	inc %rsi
	jmp .Lloop
.Ldiff:
	seta %al
	movzbl %al, %eax
	lea -1(%rax,%rax), %eax
	ret
.Lequal:
	xor %eax, %eax
	ret
	.size __crunch_strcmp, .-__crunch_strcmp
	.section .note.GNU-stack,"",@progbits
