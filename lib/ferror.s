	.text
	# Input: stream in rdi. Output: error indicator, or -1 with errno set for null.
	.globl __crunch_ferror
	.type __crunch_ferror, @function
__crunch_ferror:
	test %rdi, %rdi
	jz .Lferror_invalid
	mov 8(%rdi), %eax
	shr $1, %eax
	and $1, %eax
	ret
.Lferror_invalid:
	sub $8, %rsp
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %eax
	add $8, %rsp
	ret
	.size __crunch_ferror, .-__crunch_ferror
	.section .note.GNU-stack,"",@progbits
