	.text
	# Inputs: format slot in rdi and tagged pointer count in rsi.
	# Output: assignment count or EOF.
	.globl __crunch_scanf
	.type __crunch_scanf, @function
__crunch_scanf:
	mov %rdi, %rax
	mov %rsi, %rcx
	mov (%rax), %rsi
	mov %rax, %rdx
	mov _crunch_stdin@GOTPCREL(%rip), %rdi
	mov (%rdi), %rdi
	xor %r8d, %r8d
	jmp _crunch_scan_engine
	.size __crunch_scanf, .-__crunch_scanf

	# Inputs: format slot, tagged pointer count, stream in rdi, rsi, rdx.
	# Output: assignment count or EOF.
	.globl __crunch_fscanf
	.type __crunch_fscanf, @function
__crunch_fscanf:
	mov %rdi, %rax
	mov %rdx, %rdi
	mov %rsi, %rcx
	mov (%rax), %rsi
	mov %rax, %rdx
	xor %r8d, %r8d
	jmp _crunch_scan_engine
	.size __crunch_fscanf, .-__crunch_fscanf

	# Inputs: format in rdi and System V va_list in rsi.
	# Output: assignment count or EOF.
	.globl __crunch_vscanf
	.type __crunch_vscanf, @function
__crunch_vscanf:
	mov %rsi, %r8
	mov %rdi, %rsi
	mov _crunch_stdin@GOTPCREL(%rip), %rdi
	mov (%rdi), %rdi
	xor %edx, %edx
	xor %ecx, %ecx
	jmp _crunch_scan_engine
	.size __crunch_vscanf, .-__crunch_vscanf

	# Inputs: stream, format and System V va_list in rdi, rsi, rdx.
	# Output: assignment count or EOF.
	.globl __crunch_vfscanf
	.type __crunch_vfscanf, @function
__crunch_vfscanf:
	mov %rdx, %r8
	xor %edx, %edx
	xor %ecx, %ecx
	jmp _crunch_scan_engine
	.size __crunch_vfscanf, .-__crunch_vfscanf

	# Input: stream in rdi. Output: next byte or EOF.
	.globl _crunch_scan_fgetc
	.hidden _crunch_scan_fgetc
_crunch_scan_fgetc:
	jmp __crunch_fgetc
	# Input: character in edi. Output: nonzero for whitespace.
	.globl _crunch_scan_isspace
	.hidden _crunch_scan_isspace
_crunch_scan_isspace:
	jmp __crunch_isspace
	# Input: character in edi. Output: lowercase equivalent.
	.globl _crunch_scan_tolower
	.hidden _crunch_scan_tolower
_crunch_scan_tolower:
	jmp __crunch_tolower
	# Inputs: allocation and size in rdi and rsi. Output: resized pointer.
	.globl _crunch_scan_realloc
	.hidden _crunch_scan_realloc
_crunch_scan_realloc:
	jmp __crunch_realloc
	# Input: allocation in rdi. Output: none.
	.globl _crunch_scan_free
	.hidden _crunch_scan_free
_crunch_scan_free:
	jmp __crunch_free
	# Inputs: text and end pointer in rdi and rsi. Output: float in xmm0.
	.globl _crunch_scan_strtof
	.hidden _crunch_scan_strtof
_crunch_scan_strtof:
	jmp __crunch_strtof
	# Inputs: text and end pointer in rdi and rsi. Output: double in xmm0.
	.globl _crunch_scan_strtod
	.hidden _crunch_scan_strtod
_crunch_scan_strtod:
	jmp __crunch_strtod
	# Inputs: text and end pointer in rdi and rsi. Output: long double in st(0).
	.globl _crunch_scan_strtold
	.hidden _crunch_scan_strtold
_crunch_scan_strtold:
	jmp __crunch_strtold
	.section .note.GNU-stack,"",@progbits
