.text
# Allocates and zeroes rdi bytes for runtime-managed dynamic arrays.
# Input: allocation size. Output: pointer in rax, or NULL on allocation failure.
.globl __crunch_alloc_bytes
	.type __crunch_alloc_bytes, @function
# mmap(size in %rdi) -> pointer in %rax, NULL on failure
__crunch_alloc_bytes:
	test %rdi, %rdi
	jz .Lzero
	mov %rdi, %rsi
	xor %edi, %edi
	mov $9, %eax
	mov $3, %edx
	mov $34, %r10d
	mov $-1, %r8
	xor %r9d, %r9d
	syscall
	cmp $-4095, %rax
	jb .Ldone
.Lzero:
	xor %eax, %eax
.Ldone:
	ret
	.size __crunch_alloc_bytes, .-__crunch_alloc_bytes
	.section .note.GNU-stack,"",@progbits
