.text
# Runs registered Crunch module initializers for a custom process entry point.
# Inputs: none.
# Output: none; each module initializer runs at most once.
.globl __crunch_run_initializers
.type __crunch_run_initializers, @function
.weak __start___crunch_init_array
.weak __stop___crunch_init_array
__crunch_run_initializers:
	push %r12
	push %r13
	sub $8, %rsp
	.ifdef CRUNCH_SHARED
	mov __start___crunch_init_array@GOTPCREL(%rip), %r12
	mov __stop___crunch_init_array@GOTPCREL(%rip), %r13
	test %r12, %r12
	jnz .Lcheck_stop
	test %r13, %r13
	jz .Ldone
	jmp .Lmissing_bounds
.Lcheck_stop:
	test %r13, %r13
	jnz .Lloop
.Lmissing_bounds:
	mov $1, %eax
	mov $2, %edi
	lea .Lmissing_bounds_message(%rip), %rsi
	mov $45, %edx
	syscall
	call _crunch_errno_location
	movl $22, (%rax)
	jmp .Ldone
	.else
	lea __start___crunch_init_array(%rip), %r12
	lea __stop___crunch_init_array(%rip), %r13
	.endif
.Lloop:
	cmp %r13, %r12
	jae .Ldone
	call *(%r12)
	add $8, %r12
	jmp .Lloop
.Ldone:
	add $8, %rsp
	pop %r13
	pop %r12
	ret
.size __crunch_run_initializers, .-__crunch_run_initializers
	.ifdef CRUNCH_SHARED
	.section .rodata,"a",@progbits
.Lmissing_bounds_message:
	.ascii "crunch: incomplete module initializer bounds\n"
	.endif
.section .note.GNU-stack,"",@progbits
