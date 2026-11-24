	.text
	# Input: stream. Output: underlying descriptor or -1.
	.globl __crunch_fileno
	.type __crunch_fileno, @function
__crunch_fileno:
	test %rdi, %rdi
	jz .Linvalid
	mov (%rdi), %eax
	ret
.Linvalid:
	sub $8, %rsp
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %eax
	add $8, %rsp
	ret
	.size __crunch_fileno, .-__crunch_fileno
	.section .note.GNU-stack,"",@progbits
