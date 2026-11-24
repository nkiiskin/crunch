	.text
	# Terminates the process with status in edi; does not return.
	# Input: exit status. Output: none.
	.globl __crunch_end
	.type __crunch_end, @function
	.globl __crunch_exit
	.type __crunch_exit, @function
	.weak __crunch_run_finalizers
__crunch_end:
__crunch_exit:
	push %rdi
	.ifdef CRUNCH_SHARED
	mov __crunch_run_finalizers@GOTPCREL(%rip), %rax
	.else
	mov .Lfinalizer_pointer(%rip), %rax
	.endif
	test %rax, %rax
	jz .Lexit
	call *%rax
.Lexit:
	pop %rdi
	mov $60, %eax
	syscall
	ud2
	.size __crunch_end, .-__crunch_end
	.size __crunch_exit, .-__crunch_exit
	.ifndef CRUNCH_SHARED
	.data
	.balign 8
.Lfinalizer_pointer:
	.quad __crunch_run_finalizers
	.endif
	.section .note.GNU-stack,"",@progbits
