	.text
	# Input: stream in rdi. Output: byte position or -1 with errno set.
	.globl __crunch_ftell
	.type __crunch_ftell, @function
__crunch_ftell:
	push %r12
	test %rdi, %rdi
	jz .Lftell_invalid
	mov %rdi, %r12
	mov (%rdi), %edi
	xor %esi, %esi
	mov $1, %edx
	call __crunch_lseek
	cmp $-1, %rax
	je .Lftell_done
	testl $4, 8(%r12)
	jz .Lftell_done
	dec %rax
.Lftell_done:
	pop %r12
	ret
.Lftell_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %rax
	jmp .Lftell_done
	.size __crunch_ftell, .-__crunch_ftell
	.section .note.GNU-stack,"",@progbits
