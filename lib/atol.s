	.text
	# Parses a decimal integer from the NUL-terminated string at rdi.
	# Input: string pointer. Output: parsed long value in rax.
	.globl __crunch_atol
	.type __crunch_atol, @function
__crunch_atol:
.Lskip:
	movzbl (%rdi), %ecx
	cmp $32, %ecx
	je .Lnext
	lea -9(%rcx), %edx
	cmp $4, %edx
	ja .Lsign
.Lnext:
	inc %rdi
	jmp .Lskip
.Lsign:
	xor %esi, %esi
	cmp $45, %ecx
	jne .Lplus
	mov $1, %esi
	inc %rdi
	jmp .Ldigits
.Lplus:
	cmp $43, %ecx
	jne .Ldigits
	inc %rdi
.Ldigits:
	xor %eax, %eax
.Lloop:
	movzbl (%rdi), %ecx
	sub $48, %ecx
	cmp $9, %ecx
	ja .Ldone
	imul $10, %rax, %rax
	add %rcx, %rax
	inc %rdi
	jmp .Lloop
.Ldone:
	test %esi, %esi
	jz .Lret
	neg %rax
.Lret:
	ret
	.size __crunch_atol, .-__crunch_atol
	.section .note.GNU-stack,"",@progbits
