	.text
	# Inputs: signal, action, previous action. Output: zero or -1.
	.globl __crunch_sigaction
	.type __crunch_sigaction, @function
__crunch_sigaction:
	sub $40, %rsp
	test %rsi, %rsi
	jz .Lsigaction_call
	mov 0(%rsi), %rax
	mov %rax, 0(%rsp)
	mov 8(%rsi), %rax
	or $0x04000000, %rax
	mov %rax, 8(%rsp)
	lea .Lsigreturn(%rip), %rax
	mov %rax, 16(%rsp)
	mov 24(%rsi), %rax
	mov %rax, 24(%rsp)
	mov %rsp, %rsi
.Lsigaction_call:
	mov $8, %r10d
	mov $13, %eax
	syscall
	add $40, %rsp
	jmp __crunch_syscall_result
.Lsigreturn:
	mov $15, %eax
	syscall
	.size __crunch_sigaction, .-__crunch_sigaction
	.section .note.GNU-stack,"",@progbits
