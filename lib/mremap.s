	.text
	# Inputs: old address, old size, new size, flags, new address.
	# Output: mapped address or -1 with errno set.
	.globl __crunch_mremap
	.type __crunch_mremap, @function
__crunch_mremap:
	jmp _crunch_mremap
	.size __crunch_mremap, .-__crunch_mremap
	.section .note.GNU-stack,"",@progbits
