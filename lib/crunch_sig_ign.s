	.text
	# Input: none. Output: Linux SIG_IGN handler value.
	.globl __crunch_crunch_sig_ign
	.type __crunch_crunch_sig_ign, @function
__crunch_crunch_sig_ign:
	mov $1, %eax
	ret
	.size __crunch_crunch_sig_ign, .-__crunch_crunch_sig_ign
	.section .note.GNU-stack,"",@progbits
