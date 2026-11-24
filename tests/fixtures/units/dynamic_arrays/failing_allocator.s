.data
.globl fail_mode
.type fail_mode, @object
.size fail_mode, 4
fail_mode:
    .long 1
.globl alloc_calls
.type alloc_calls, @object
.size alloc_calls, 4
alloc_calls:
    .long 0
.globl free_calls
.type free_calls, @object
.size free_calls, 4
free_calls:
    .long 0

.text
.globl __crunch_alloc_bytes
.type __crunch_alloc_bytes, @function
# Allocates with mmap unless fail_mode is set; counts allocation attempts.
__crunch_alloc_bytes:
    addl $1, alloc_calls(%rip)
    cmpl $0, fail_mode(%rip)
    jne .Lfail
    test %rdi, %rdi
    jz .Lfail
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
.Lfail:
    xor %eax, %eax
.Ldone:
    ret
.size __crunch_alloc_bytes, .-__crunch_alloc_bytes

.globl __crunch_free_bytes
.type __crunch_free_bytes, @function
# Releases a nonempty mapping and counts releases.
__crunch_free_bytes:
    test %rdi, %rdi
    jz .Lfree_done
    test %rsi, %rsi
    jz .Lfree_done
    addl $1, free_calls(%rip)
    mov $11, %eax
    syscall
.Lfree_done:
    ret
.size __crunch_free_bytes, .-__crunch_free_bytes
.section .note.GNU-stack,"",@progbits
