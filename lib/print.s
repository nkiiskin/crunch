.text
# Writes the NUL-terminated string at rdi to standard output.
# Input: string pointer. Output: status in rax (zero on success, negative on error).
.globl __crunch_print
	.ifndef CRUNCH_SHARED
	.hidden __crunch_print
	.endif
	.type __crunch_print, @function
# print(str=%rdi) writes bytes without newline; returns 0/-1 in %rax
__crunch_print:
	mov %rdi, %rsi
	xor %edx, %edx
.Llen:
	cmpb $0, (%rsi,%rdx)
	je .Lwrite
	inc %rdx
	jmp .Llen
.Lwrite:
	mov $1, %eax
	mov $1, %edi
	syscall
	cmp %rdx, %rax
	jne .Lerror
	xor %eax, %eax
	ret
.Lerror:
	mov $-1, %rax
	ret
	.size __crunch_print, .-__crunch_print
	.section .note.GNU-stack,"",@progbits
