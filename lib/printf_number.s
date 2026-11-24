	.text
	# Writes a signed or unsigned integer in rdi as decimal digits.
	# Inputs: integer in rdi, stream in rsi, writer in rdx. Output: count or -1.
	.globl __crunch_printf_number
	.hidden __crunch_printf_number
	.type __crunch_printf_number, @function
__crunch_printf_number:
	xor %ecx, %ecx
	mov $10, %r9d
	jmp .Lsetup
	.size __crunch_printf_number, .-__crunch_printf_number

	# Inputs: unsigned integer in rdi, stream in rsi, writer in rdx.
	# Output: character count or -1.
	.globl __crunch_printf_unsigned
	.hidden __crunch_printf_unsigned
	.type __crunch_printf_unsigned, @function
__crunch_printf_unsigned:
	mov $1, %ecx
	mov $10, %r9d
	jmp .Lsetup
	.size __crunch_printf_unsigned, .-__crunch_printf_unsigned

	# Inputs: unsigned integer in rdi, stream in rsi, writer in rdx.
	# Output: lowercase hexadecimal character count or -1.
	.globl __crunch_printf_hex
	.hidden __crunch_printf_hex
	.type __crunch_printf_hex, @function
__crunch_printf_hex:
	mov $1, %ecx
	mov $16, %r9d
.Lsetup:
	sub $56, %rsp
	mov %rsi, (%rsp)
	mov %rdx, 8(%rsp)
	lea 56(%rsp), %rsi
	mov %rdi, %rax
	xor %r8d, %r8d
	test %ecx, %ecx
	jnz .Ldigits
	test %rdi, %rdi
	jns .Ldigits
	mov $1, %r8d
	neg %rax
.Ldigits:
	xor %edx, %edx
	div %r9
	cmp $9, %dl
	jbe .Ldecimal_digit
	add $87, %dl
	jmp .Ldigit_ready
.Ldecimal_digit:
	add $48, %dl
.Ldigit_ready:
	dec %rsi
	mov %dl, (%rsi)
	test %rax, %rax
	jnz .Ldigits
	test %r8d, %r8d
	jz .Lready
	dec %rsi
	movb $45, (%rsi)
.Lready:
	mov %rsi, %rdi
	lea 56(%rsp), %rsi
	sub %rdi, %rsi
	mov (%rsp), %rdx
	call *8(%rsp)
	add $56, %rsp
	ret
	.size __crunch_printf_hex, .-__crunch_printf_hex
	.section .note.GNU-stack,"",@progbits
