.text
.globl alignment_offset
.type alignment_offset, @function
alignment_offset:
    mov %rdi, %rax
    and $15, %rax
    ret
.section .note.GNU-stack,"",@progbits
