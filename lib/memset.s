	.text
	# Fills rdx bytes at rdi with the low byte of esi.
	# Inputs: destination, fill value, byte count. Output: destination pointer.
	.globl __crunch_memset
	.type __crunch_memset, @function
__crunch_memset:
	mov %rdi, %r8
	cmp $8, %rdx
	jb .Lbytes
	movzbl %sil, %eax
	movabsq $0x0101010101010101, %rcx
	imulq %rcx, %rax
	mov %rdx, %rcx
	shr $3, %rcx
	rep stosq
	and $7, %rdx
.Lbytes:
	mov %sil, %al
	mov %rdx, %rcx
	rep stosb
	mov %r8, %rax
	ret
	.size __crunch_memset, .-__crunch_memset
	.section .note.GNU-stack,"",@progbits
