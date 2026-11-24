	.text
	# Inputs: format slot in rdi, value count in rsi; capacity and buffer follow the slot.
	# Output: number of bytes that would have been written, or -1 on error.
	.globl __crunch_snprintf
	.type __crunch_snprintf, @function
__crunch_snprintf:
	xor %r9d, %r9d
	jmp .Lsnprintf_common
	.size __crunch_snprintf, .-__crunch_snprintf
	# Inputs: format slot in rdi, count in rsi; capacity and buffer follow the slot.
	# Output: would-be byte count, or -1 on error; enables unsigned conversions.
	.globl __crunch_snprintf_v6
	.ifndef CRUNCH_SHARED
	.hidden __crunch_snprintf_v6
	.endif
	.type __crunch_snprintf_v6, @function
__crunch_snprintf_v6:
	mov $1, %r9d
	jmp .Lsnprintf_common
	.size __crunch_snprintf_v6, .-__crunch_snprintf_v6
	# Inputs: format slot in rdi, count in rsi; capacity and buffer follow the slot.
	# Output: would-be byte count or -1; accepts representable signed/unsigned counterparts.
	.globl __crunch_snprintf_v26
	.ifndef CRUNCH_SHARED
	.hidden __crunch_snprintf_v26
	.endif
	.type __crunch_snprintf_v26, @function
__crunch_snprintf_v26:
	mov $3, %r9d
	jmp .Lsnprintf_common
	.size __crunch_snprintf_v26, .-__crunch_snprintf_v26
	# Inputs: format slot in rdi, count in rsi; capacity and buffer follow the slot.
	# Output: would-be byte count or -1; also enables %.17g.
	.globl __crunch_snprintf_v28
	.ifndef CRUNCH_SHARED
	.hidden __crunch_snprintf_v28
	.endif
	.type __crunch_snprintf_v28, @function
__crunch_snprintf_v28:
	mov $7, %r9d
	jmp .Lsnprintf_common
	.size __crunch_snprintf_v28, .-__crunch_snprintf_v28
	# Inputs: format slot in rdi, count in rsi; capacity and buffer follow the slot.
	# Output: would-be byte count or -1; also enables binary80 formatting.
	.globl __crunch_snprintf_v34
	.ifndef CRUNCH_SHARED
	.hidden __crunch_snprintf_v34
	.endif
	.type __crunch_snprintf_v34, @function
__crunch_snprintf_v34:
	mov $23, %r9d
	jmp .Lsnprintf_common
	.size __crunch_snprintf_v34, .-__crunch_snprintf_v34
	# Inputs: format slot in rdi, count in rsi; capacity and buffer follow the slot.
	# Output: would-be byte count or -1; also enables dynamic string precision.
	.globl __crunch_snprintf_v36
	.ifndef CRUNCH_SHARED
	.hidden __crunch_snprintf_v36
	.endif
	.type __crunch_snprintf_v36, @function
__crunch_snprintf_v36:
	mov $55, %r9d
	jmp .Lsnprintf_common
	.size __crunch_snprintf_v36, .-__crunch_snprintf_v36
	# Inputs: format slot in rdi, count in rsi; capacity and buffer follow the slot.
	# Output: would-be byte count or -1; also enables bounded binary80 precision.
	.globl __crunch_snprintf_v42
	.ifndef CRUNCH_SHARED
	.hidden __crunch_snprintf_v42
	.endif
	.type __crunch_snprintf_v42, @function
__crunch_snprintf_v42:
	mov $119, %r9d
.Lsnprintf_common:
	sub $40, %rsp
	mov 8(%rdi), %r8
	mov 16(%rdi), %rax
	test %r8, %r8
	jz .Lsnprintf_ready
	test %rax, %rax
	jz .Lsnprintf_invalid
	movb $0, (%rax)
	dec %r8
.Lsnprintf_ready:
	mov %rax, (%rsp)
	mov %r8, 8(%rsp)
	mov %rsp, %rdx
	lea .Lsnprintf_write(%rip), %rcx
	lea .Lsnprintf_pad(%rip), %r8
	call _crunch_format
	add $40, %rsp
	ret
.Lsnprintf_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %rax
	add $40, %rsp
	ret
	.size __crunch_snprintf_v42, .-__crunch_snprintf_v42

	# Inputs: destination in rdi, capacity in rsi, format in rdx, va_list in rcx.
	# Output: number of bytes that would have been written, or -1 on error.
	.globl __crunch_vsnprintf
	.type __crunch_vsnprintf, @function
__crunch_vsnprintf:
	sub $56, %rsp
	mov %rdi, (%rsp)
	mov %rsi, 8(%rsp)
	mov %rdx, 16(%rsp)
	test %rsi, %rsi
	jz .Lvsnprintf_ready
	test %rdi, %rdi
	jz .Lvsnprintf_invalid
	movb $0, (%rdi)
	decq 8(%rsp)
.Lvsnprintf_ready:
	lea 16(%rsp), %rdi
	mov %rcx, %rsi
	mov %rsp, %rdx
	lea .Lsnprintf_write(%rip), %rcx
	lea .Lsnprintf_pad(%rip), %r8
	mov $95, %r9d
	call _crunch_format
	add $56, %rsp
	ret
.Lvsnprintf_invalid:
	call _crunch_errno_location
	movl $22, (%rax)
	mov $-1, %rax
	add $56, %rsp
	ret
	.size __crunch_vsnprintf, .-__crunch_vsnprintf

	# Inputs: destination, format, va_list. Output: count or -1 on error.
	.globl __crunch_vsprintf
	.type __crunch_vsprintf, @function
__crunch_vsprintf:
	mov %rdx, %rcx
	mov %rsi, %rdx
	mov $-1, %rsi
	jmp __crunch_vsnprintf
	.size __crunch_vsprintf, .-__crunch_vsprintf

	# Inputs: format slot and count; buffer follows the slot. Output: count or -1.
	.globl __crunch_sprintf
	.type __crunch_sprintf, @function
__crunch_sprintf:
	xor %r9d, %r9d
	jmp .Lsprintf_common
	.size __crunch_sprintf, .-__crunch_sprintf
	# Inputs: format slot and count; buffer follows the slot. Output: count or -1.
	.globl __crunch_sprintf_v6
	.type __crunch_sprintf_v6, @function
__crunch_sprintf_v6:
	mov $1, %r9d
	jmp .Lsprintf_common
	.size __crunch_sprintf_v6, .-__crunch_sprintf_v6
	# Inputs: format slot and count; buffer follows the slot. Output: count or -1.
	.globl __crunch_sprintf_v26
	.type __crunch_sprintf_v26, @function
__crunch_sprintf_v26:
	mov $3, %r9d
	jmp .Lsprintf_common
	.size __crunch_sprintf_v26, .-__crunch_sprintf_v26
	# Inputs: format slot and count; buffer follows the slot. Output: count or -1.
	.globl __crunch_sprintf_v28
	.type __crunch_sprintf_v28, @function
__crunch_sprintf_v28:
	mov $7, %r9d
	jmp .Lsprintf_common
	.size __crunch_sprintf_v28, .-__crunch_sprintf_v28
	# Inputs: format slot and count; buffer follows the slot. Output: count or -1.
	.globl __crunch_sprintf_v34
	.type __crunch_sprintf_v34, @function
__crunch_sprintf_v34:
	mov $23, %r9d
	jmp .Lsprintf_common
	.size __crunch_sprintf_v34, .-__crunch_sprintf_v34
	# Inputs: format slot and count; buffer follows the slot. Output: count or -1.
	.globl __crunch_sprintf_v36
	.type __crunch_sprintf_v36, @function
__crunch_sprintf_v36:
	mov $55, %r9d
	jmp .Lsprintf_common
	.size __crunch_sprintf_v36, .-__crunch_sprintf_v36
	# Inputs: format slot and count; buffer follows the slot. Output: count or -1.
	.globl __crunch_sprintf_v42
	.type __crunch_sprintf_v42, @function
__crunch_sprintf_v42:
	mov $119, %r9d
.Lsprintf_common:
	sub $40, %rsp
	mov 8(%rdi), %rax
	test %rax, %rax
	jz .Lsnprintf_invalid
	movb $0, (%rax)
	mov %rax, (%rsp)
	movq $-2, 8(%rsp)
	mov %rsp, %rdx
	lea .Lsnprintf_write(%rip), %rcx
	lea .Lsnprintf_pad(%rip), %r8
	call _crunch_format
	add $40, %rsp
	ret
	.size __crunch_sprintf_v42, .-__crunch_sprintf_v42

	# Inputs: source in rdi, byte count in rsi, buffer state in rdx.
	# Output: requested byte count, including bytes discarded by truncation.
.Lsnprintf_write:
	mov %rsi, %rax
	mov 8(%rdx), %rcx
	cmp %rsi, %rcx
	cmova %rsi, %rcx
	test %rcx, %rcx
	jz .Lsnprintf_write_done
	mov %rcx, %r8
	mov %rdi, %rsi
	mov (%rdx), %rdi
	rep movsb
	mov %rdi, (%rdx)
	sub %r8, 8(%rdx)
	movb $0, (%rdi)
.Lsnprintf_write_done:
	ret

	# Inputs: padding count in rdi, byte in sil, buffer state in rdx.
	# Output: requested padding count, including bytes discarded by truncation.
.Lsnprintf_pad:
	mov %rdi, %r9
	mov 8(%rdx), %rcx
	cmp %rdi, %rcx
	cmova %rdi, %rcx
	test %rcx, %rcx
	jz .Lsnprintf_pad_done
	mov %rcx, %r8
	mov %esi, %eax
	mov (%rdx), %rdi
	rep stosb
	mov %rdi, (%rdx)
	sub %r8, 8(%rdx)
	movb $0, (%rdi)
.Lsnprintf_pad_done:
	mov %r9, %rax
	ret
	.section .note.GNU-stack,"",@progbits
