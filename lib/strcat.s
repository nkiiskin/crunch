	.text
	# Appends the NUL-terminated string at rsi to the string at rdi.
	# Inputs: destination and source strings. Output: destination pointer in rax.
	.globl __crunch_strcat
	.type __crunch_strcat, @function
__crunch_strcat:
	mov %rdi, %rax
	# Aligned word reads cannot cross an inaccessible page.
.Lalign_dest:
	test $7, %rdi
	jz .Lfind_words
	cmpb $0, (%rdi)
	je .Lcopy
	inc %rdi
	jmp .Lalign_dest
.Lfind_words:
	movabsq $0x0101010101010101, %r8
	movabsq $0x8080808080808080, %r9
.Lfind:
	mov (%rdi), %rdx
	mov %rdx, %rcx
	sub %r8, %rcx
	not %rdx
	and %rdx, %rcx
	test %r9, %rcx
	jnz .Lfind_tail
	add $8, %rdi
	jmp .Lfind
.Lfind_tail:
	cmpb $0, (%rdi)
	je .Lcopy
	inc %rdi
	jmp .Lfind_tail
.Lcopy:
	test $7, %rsi
	jz .Lcopy_words
	movb (%rsi), %cl
	movb %cl, (%rdi)
	test %cl, %cl
	jz .Ldone
	inc %rsi
	inc %rdi
	jmp .Lcopy
.Lcopy_words:
	movabsq $0x0101010101010101, %r8
	movabsq $0x8080808080808080, %r9
.Lcopy_loop:
	mov (%rsi), %rdx
	mov %rdx, %rcx
	sub %r8, %rcx
	mov %rdx, %r10
	not %r10
	and %r10, %rcx
	test %r9, %rcx
	jnz .Lcopy_tail
	mov %rdx, (%rdi)
	add $8, %rsi
	add $8, %rdi
	jmp .Lcopy_loop
.Lcopy_tail:
	movb (%rsi), %cl
	movb %cl, (%rdi)
	inc %rsi
	inc %rdi
	test %cl, %cl
	jnz .Lcopy_tail
.Ldone:
	ret
	.size __crunch_strcat, .-__crunch_strcat
	.section .note.GNU-stack,"",@progbits
