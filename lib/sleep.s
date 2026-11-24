	.text
	# Input: seconds. Output: remaining whole seconds, rounded up.
	.globl __crunch_sleep
	.type __crunch_sleep, @function
__crunch_sleep:
	sub $40, %rsp
	mov %edi, %eax
	mov %rax, (%rsp)
	movq $0, 8(%rsp)
	movq $0, 16(%rsp)
	movq $0, 24(%rsp)
	mov %rsp, %rdi
	lea 16(%rsp), %rsi
	mov $35, %eax
	syscall
	test %rax, %rax
	jz .Lsleep_done
	cmp $-4, %rax
	jne .Lsleep_error
	mov 16(%rsp), %rax
	cmpq $0, 24(%rsp)
	je .Lsleep_done
	inc %rax
	jmp .Lsleep_done
.Lsleep_error:
	call __crunch_syscall_result
	mov (%rsp), %rax
.Lsleep_done:
	add $40, %rsp
	ret
	.size __crunch_sleep, .-__crunch_sleep
	.section .note.GNU-stack,"",@progbits
