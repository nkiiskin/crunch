# Crunch library: strlen(str=%rdi) -> byte count in %rax
	.text
	# Counts bytes before the NUL terminator at rdi.
	# Input: string pointer. Output: length in rax.
	.globl __crunch_strlen
	.type __crunch_strlen, @function
__crunch_strlen:
	mov %rdi, %rax
	# Align word loads so they cannot cross an inaccessible page.
.Lalign:
	test $7, %rax
	jz .Lwords
	cmpb $0, (%rax)
	je .Ldone
	inc %rax
	jmp .Lalign
.Lwords:
	movabsq $0x0101010101010101, %rcx
	movabsq $0x8080808080808080, %r10
.Lloop:
	mov (%rax), %rdx
	mov %rdx, %r8
	sub %rcx, %r8
	not %rdx
	and %rdx, %r8
	test %r10, %r8
	jnz .Ltail
	add $8, %rax
	jmp .Lloop
.Ltail:
	cmpb $0, (%rax)
	je .Ldone
	inc %rax
	jmp .Ltail
.Ldone:
	sub %rdi, %rax
	ret
	.size __crunch_strlen, .-__crunch_strlen
	.section .note.GNU-stack,"",@progbits
