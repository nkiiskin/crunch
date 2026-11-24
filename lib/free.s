	.text
	# Input: allocation pointer in rdi, or null. Output: none.
	.globl _crunch_free
	.hidden _crunch_free
	.type _crunch_free, @function
	.globl __crunch_free
	.type __crunch_free, @function
_crunch_free:
__crunch_free:
	test %rdi, %rdi
	jz .Lfree_done
	cmpq $1, -32(%rdi)
	je _crunch_pool_free
	mov -16(%rdi), %rsi
	sub $32, %rdi
	jmp _crunch_munmap
.Lfree_done:
	ret
	.size _crunch_free, .-_crunch_free
	.size __crunch_free, .-__crunch_free
	.section .note.GNU-stack,"",@progbits
