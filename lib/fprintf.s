	.text
	# Inputs: format slot in rdi, argument count in rsi, stream in rdx.
	# Output: byte count or -1 on invalid format or write failure.
	.globl __crunch_fprintf
	.type __crunch_fprintf, @function
__crunch_fprintf:
	xor %r9d, %r9d
	jmp .Lfprintf_common
	.size __crunch_fprintf, .-__crunch_fprintf
	# Inputs: format slot in rdi, argument count in rsi, stream in rdx.
	# Output: character count, or -1 on error; enables unsigned conversions.
	.globl __crunch_fprintf_v6
	.ifndef CRUNCH_SHARED
	.hidden __crunch_fprintf_v6
	.endif
	.type __crunch_fprintf_v6, @function
__crunch_fprintf_v6:
	mov $1, %r9d
	jmp .Lfprintf_common
	.size __crunch_fprintf_v6, .-__crunch_fprintf_v6
	# Inputs: format slot in rdi, argument count in rsi, stream in rdx.
	# Output: byte count or -1; accepts representable signed/unsigned counterparts.
	.globl __crunch_fprintf_v26
	.ifndef CRUNCH_SHARED
	.hidden __crunch_fprintf_v26
	.endif
	.type __crunch_fprintf_v26, @function
__crunch_fprintf_v26:
	mov $3, %r9d
	jmp .Lfprintf_common
	.size __crunch_fprintf_v26, .-__crunch_fprintf_v26
	# Inputs: format slot in rdi, argument count in rsi, stream in rdx.
	# Output: byte count or -1; also enables %.17g.
	.globl __crunch_fprintf_v28
	.ifndef CRUNCH_SHARED
	.hidden __crunch_fprintf_v28
	.endif
	.type __crunch_fprintf_v28, @function
__crunch_fprintf_v28:
	mov $7, %r9d
	jmp .Lfprintf_common
	.size __crunch_fprintf_v28, .-__crunch_fprintf_v28
	# Inputs: format slot in rdi, argument count in rsi, stream in rdx.
	# Output: byte count or -1; also enables binary80 formatting.
	.globl __crunch_fprintf_v34
	.ifndef CRUNCH_SHARED
	.hidden __crunch_fprintf_v34
	.endif
	.type __crunch_fprintf_v34, @function
__crunch_fprintf_v34:
	mov $23, %r9d
	jmp .Lfprintf_common
	.size __crunch_fprintf_v34, .-__crunch_fprintf_v34
	# Inputs: format slot in rdi, argument count in rsi, stream in rdx.
	# Output: byte count or -1; also enables dynamic string precision.
	.globl __crunch_fprintf_v36
	.ifndef CRUNCH_SHARED
	.hidden __crunch_fprintf_v36
	.endif
	.type __crunch_fprintf_v36, @function
__crunch_fprintf_v36:
	mov $55, %r9d
	jmp .Lfprintf_common
	.size __crunch_fprintf_v36, .-__crunch_fprintf_v36
	# Inputs: format slot in rdi, argument count in rsi, stream in rdx.
	# Output: byte count or -1; also enables bounded binary80 precision.
	.globl __crunch_fprintf_v42
	.ifndef CRUNCH_SHARED
	.hidden __crunch_fprintf_v42
	.endif
	.type __crunch_fprintf_v42, @function
__crunch_fprintf_v42:
	mov $119, %r9d
.Lfprintf_common:
	test %rdx, %rdx
	jz .Lfprintf_invalid
	lea _crunch_format_write(%rip), %rcx
	xor %r8d, %r8d
	jmp _crunch_format
.Lfprintf_invalid:
	sub $8, %rsp
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %rax
	add $8, %rsp
	ret
	.size __crunch_fprintf_v42, .-__crunch_fprintf_v42

	# Inputs: stream in rdi, format in rsi, va_list in rdx.
	# Output: count or -1.
	.globl __crunch_vfprintf
	.type __crunch_vfprintf, @function
__crunch_vfprintf:
	test %rdi, %rdi
	jz .Lvfprintf_invalid
	sub $24, %rsp
	mov %rsi, (%rsp)
	mov %rdi, %rax
	mov %rsp, %rdi
	mov %rdx, %rsi
	mov %rax, %rdx
	lea _crunch_format_write(%rip), %rcx
	xor %r8d, %r8d
	mov $127, %r9d
	call _crunch_format
	add $24, %rsp
	ret
.Lvfprintf_invalid:
	sub $8, %rsp
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %rax
	add $8, %rsp
	ret
	.size __crunch_vfprintf, .-__crunch_vfprintf
	.section .note.GNU-stack,"",@progbits
