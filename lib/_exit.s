	.text
	# Input: status. Output: none; terminates without running finalizers.
	.globl __crunch__exit
	.type __crunch__exit, @function
__crunch__exit:
	mov $60, %eax
	syscall
	ud2
	.size __crunch__exit, .-__crunch__exit
	.section .note.GNU-stack,"",@progbits
