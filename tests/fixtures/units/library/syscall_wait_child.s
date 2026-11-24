	.text
	.globl wait_child
	.type wait_child, @function
wait_child:
	sub $8, %rsp
	mov %rsp, %rsi
	xor %edx, %edx
	xor %r10d, %r10d
	mov $61, %eax
	syscall
	test %rax, %rax
	js .Lwait_error
	mov (%rsp), %eax
	add $8, %rsp
	ret
.Lwait_error:
	mov $-1, %eax
	add $8, %rsp
	ret
	.size wait_child, .-wait_child

	# Input: none. Output: zero if raw syscall results are correct.
	.globl raw_syscalls
	.type raw_syscalls, @function
raw_syscalls:
	sub $8, %rsp
	lea .Lmissing_path(%rip), %rdi
	xor %esi, %esi
	call _crunch_access
	cmp $-2, %rax
	jne .Lraw_error
	mov $-1, %edi
	call _crunch_dup
	cmp $-9, %rax
	jne .Lraw_error
	mov $-1, %edi
	mov $-1, %esi
	call _crunch_dup2
	cmp $-9, %rax
	jne .Lraw_error
	xor %edi, %edi
	call _crunch_pipe
	cmp $-14, %rax
	jne .Lraw_error
	xor %edi, %edi
	mov $-1, %rsi
	xor %edx, %edx
	call _crunch_poll
	cmp $-22, %rax
	jne .Lraw_error
	mov $-1, %edi
	xor %esi, %esi
	xor %edx, %edx
	xor %ecx, %ecx
	xor %r8d, %r8d
	call _crunch_select
	cmp $-22, %rax
	jne .Lraw_error
	call _crunch_getpid
	test %eax, %eax
	jle .Lraw_error
	mov %eax, %edi
	mov $999, %esi
	call _crunch_kill
	cmp $-22, %rax
	jne .Lraw_error
	xor %eax, %eax
	add $8, %rsp
	ret
.Lraw_error:
	mov $1, %eax
	add $8, %rsp
	ret
	.size raw_syscalls, .-raw_syscalls
	.section .rodata
.Lmissing_path:
	.asciz "missing-crunch-file"
	.section .note.GNU-stack,"",@progbits
