	.text
	# Copies rdx bytes from rsi to rdi; returns the original destination in rax.
	# Inputs: destination, source, byte count. Output: destination pointer.
	.globl __crunch_memcpy
	.type __crunch_memcpy, @function
__crunch_memcpy:
	mov %rdi, %rax
	cmp $8, %rdx
	jb .Lbytes
	mov %rdx, %rcx
	shr $3, %rcx
	rep movsq
	and $7, %rdx
.Lbytes:
	mov %rdx, %rcx
	rep movsb
	ret
	.size __crunch_memcpy, .-__crunch_memcpy
	.section .note.GNU-stack,"",@progbits
