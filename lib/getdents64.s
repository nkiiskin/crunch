	.text
	# Inputs: descriptor, buffer, byte count in edi, rsi, rdx.
	# Output: byte count, zero at end, or -1 with errno set.
	.globl __crunch_getdents64
	.type __crunch_getdents64, @function
__crunch_getdents64:
	mov $0xffffffff, %eax
	cmp %rax, %rdx
	ja .Linvalid
	push %rbp
	mov %rsp, %rbp
	and $-16, %rsp
	call _crunch_getdents64
	mov %rbp, %rsp
	pop %rbp
	jmp __crunch_syscall_result
.Linvalid:
	mov $-22, %rax
	jmp __crunch_syscall_result
	.size __crunch_getdents64, .-__crunch_getdents64
	.section .note.GNU-stack,"",@progbits
