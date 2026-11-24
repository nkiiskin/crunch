	.text
	# Inputs: address, length, protection, flags, descriptor, offset.
	# Output: mapped address or -1 with errno set.
	.globl __crunch_mmap
	.type __crunch_mmap, @function
__crunch_mmap:
	jmp _crunch_mmap
	.size __crunch_mmap, .-__crunch_mmap
	.section .note.GNU-stack,"",@progbits
