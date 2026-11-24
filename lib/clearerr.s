	.text
	# Input: stream in rdi. Output: none; null sets errno to EINVAL.
	.globl __crunch_clearerr
	.type __crunch_clearerr, @function
__crunch_clearerr:
	test %rdi, %rdi
	jz .Lclearerr_invalid
	andl $-4, 8(%rdi)
	ret
.Lclearerr_invalid:
	sub $8, %rsp
	call _crunch_errno_location
	movl $22, (%rax)
	add $8, %rsp
	ret
	.size __crunch_clearerr, .-__crunch_clearerr
	.section .note.GNU-stack,"",@progbits
