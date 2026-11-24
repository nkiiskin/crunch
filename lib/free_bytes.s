.text
# Releases a runtime-managed dynamic-array allocation.
# Inputs: allocation pointer in rdi and mapping size in rsi; zero inputs are ignored.
# Output: none; callers discard the munmap result.
.globl __crunch_free_bytes
	.type __crunch_free_bytes, @function
# munmap(pointer in %rdi, size in %rsi)
__crunch_free_bytes:
	test %rdi, %rdi
	jz .Ldone
	test %rsi, %rsi
	jz .Ldone
	mov $11, %eax
	syscall
.Ldone:
	ret
	.size __crunch_free_bytes, .-__crunch_free_bytes
	.section .note.GNU-stack,"",@progbits
