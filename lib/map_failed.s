	.text
	# Input: mapping result in rdi. Output: one in eax if it is MAP_FAILED.
	.globl __crunch_map_failed
	.type __crunch_map_failed, @function
__crunch_map_failed:
	cmp $-1, %rdi
	sete %al
	movzbl %al, %eax
	ret
	.size __crunch_map_failed, .-__crunch_map_failed
	.section .note.GNU-stack,"",@progbits
