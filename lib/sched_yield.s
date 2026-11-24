	.text
	# Input: none. Output: zero or -1.
	.globl __crunch_sched_yield
	.type __crunch_sched_yield, @function
__crunch_sched_yield:
	mov $24, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_yield, .-__crunch_sched_yield
	.section .note.GNU-stack,"",@progbits
