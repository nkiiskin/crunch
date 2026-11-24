	.text
	# Input: character in edi. Output: nonzero if alphanumeric.
	.globl __crunch_isalnum
	.type __crunch_isalnum, @function
__crunch_isalnum:
	mov %edi, %eax
	sub $48, %eax
	cmp $9, %eax
	jbe .Lctype_true
	jmp .Lctype_alpha
	.size __crunch_isalnum, .-__crunch_isalnum

	# Input: character in edi. Output: nonzero if alphabetic.
	.globl __crunch_isalpha
	.type __crunch_isalpha, @function
__crunch_isalpha:
.Lctype_alpha:
	mov %edi, %eax
	or $32, %eax
	sub $97, %eax
	cmp $25, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_isalpha, .-__crunch_isalpha

	# Input: character in edi. Output: nonzero if space or tab.
	.globl __crunch_isblank
	.type __crunch_isblank, @function
__crunch_isblank:
	cmp $32, %edi
	je .Lctype_true
	cmp $9, %edi
	je .Lctype_true
	jmp .Lctype_false
	.size __crunch_isblank, .-__crunch_isblank

	# Input: character in edi. Output: nonzero if a control character.
	.globl __crunch_iscntrl
	.type __crunch_iscntrl, @function
__crunch_iscntrl:
	cmp $31, %edi
	jbe .Lctype_true
	cmp $127, %edi
	je .Lctype_true
	jmp .Lctype_false
	.size __crunch_iscntrl, .-__crunch_iscntrl

	# Input: character in edi. Output: nonzero if a decimal digit.
	.globl __crunch_isdigit
	.type __crunch_isdigit, @function
__crunch_isdigit:
	mov %edi, %eax
	sub $48, %eax
	cmp $9, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_isdigit, .-__crunch_isdigit

	# Input: character in edi. Output: nonzero if graphical.
	.globl __crunch_isgraph
	.type __crunch_isgraph, @function
__crunch_isgraph:
	mov %edi, %eax
	sub $33, %eax
	cmp $93, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_isgraph, .-__crunch_isgraph

	# Input: character in edi. Output: nonzero if lowercase.
	.globl __crunch_islower
	.type __crunch_islower, @function
__crunch_islower:
	mov %edi, %eax
	sub $97, %eax
	cmp $25, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_islower, .-__crunch_islower

	# Input: character in edi. Output: nonzero if printable.
	.globl __crunch_isprint
	.type __crunch_isprint, @function
__crunch_isprint:
	mov %edi, %eax
	sub $32, %eax
	cmp $94, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_isprint, .-__crunch_isprint

	# Input: character in edi. Output: nonzero if punctuation.
	.globl __crunch_ispunct
	.type __crunch_ispunct, @function
__crunch_ispunct:
	cmp $33, %edi
	jb .Lctype_false
	cmp $47, %edi
	jbe .Lctype_true
	cmp $58, %edi
	jb .Lctype_false
	cmp $64, %edi
	jbe .Lctype_true
	cmp $91, %edi
	jb .Lctype_false
	cmp $96, %edi
	jbe .Lctype_true
	cmp $123, %edi
	jb .Lctype_false
	cmp $126, %edi
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_ispunct, .-__crunch_ispunct

	# Input: character in edi. Output: nonzero if whitespace.
	.globl __crunch_isspace
	.type __crunch_isspace, @function
__crunch_isspace:
	cmp $32, %edi
	je .Lctype_true
	mov %edi, %eax
	sub $9, %eax
	cmp $4, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_isspace, .-__crunch_isspace

	# Input: character in edi. Output: nonzero if uppercase.
	.globl __crunch_isupper
	.type __crunch_isupper, @function
__crunch_isupper:
	mov %edi, %eax
	sub $65, %eax
	cmp $25, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_isupper, .-__crunch_isupper

	# Input: character in edi. Output: nonzero if a hexadecimal digit.
	.globl __crunch_isxdigit
	.type __crunch_isxdigit, @function
__crunch_isxdigit:
	mov %edi, %eax
	sub $48, %eax
	cmp $9, %eax
	jbe .Lctype_true
	mov %edi, %eax
	or $32, %eax
	sub $97, %eax
	cmp $5, %eax
	jbe .Lctype_true
	jmp .Lctype_false
	.size __crunch_isxdigit, .-__crunch_isxdigit

	# Input: character in edi. Output: lowercase character or unchanged input.
	.globl __crunch_tolower
	.type __crunch_tolower, @function
__crunch_tolower:
	mov %edi, %eax
	sub $65, %eax
	cmp $25, %eax
	ja .Lctype_unchanged
	add $32, %edi
	jmp .Lctype_unchanged
	.size __crunch_tolower, .-__crunch_tolower

	# Input: character in edi. Output: uppercase character or unchanged input.
	.globl __crunch_toupper
	.type __crunch_toupper, @function
__crunch_toupper:
	mov %edi, %eax
	sub $97, %eax
	cmp $25, %eax
	ja .Lctype_unchanged
	sub $32, %edi
.Lctype_unchanged:
	movslq %edi, %rax
	ret
	.size __crunch_toupper, .-__crunch_toupper

.Lctype_true:
	mov $1, %eax
	ret
.Lctype_false:
	xor %eax, %eax
	ret
	.section .note.GNU-stack,"",@progbits
