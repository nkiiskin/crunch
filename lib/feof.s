	.text
	# Input: stream in rdi. Output: EOF indicator, or -1 with errno set for null.
	.globl __crunch_feof
	.type __crunch_feof, @function
__crunch_feof:
	test %rdi, %rdi
	jz .Lfeof_invalid
	mov 8(%rdi), %eax
	and $1, %eax
	ret
.Lfeof_invalid:
	sub $8, %rsp
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %eax
	add $8, %rsp
	ret
	.size __crunch_feof, .-__crunch_feof
	.section .note.GNU-stack,"",@progbits
