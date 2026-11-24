.section .rodata
message:
    .ascii "ok\n"

.text
.globl _start
.type _start, @function
_start:
    call calculate
    mov %eax, %r12d
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $3, %edx
    syscall
    mov %r12d, %edi
    mov $60, %eax
    syscall
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
