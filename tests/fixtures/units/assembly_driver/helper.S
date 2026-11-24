# Ordinary assembly comments can contain unmatched /* or macro-like text.
#include <values.h>
#if !defined(__ASSEMBLER__) || VALUE != 40
invalid_preprocessing
#endif

.macro increment reg:req
    add $INCREMENT, \reg
.endm

.set use_increment, 1
.ifdef use_increment
.ifndef absent_option
.if 0
invalid_assembly
.elseif use_increment
.text
.globl calculate
.type calculate, @function
calculate:
    lea base(%rip), %rdx
    mov base_pointer(%rip), %rcx
    sub $4, %rcx
    cmp %rdx, %rcx
    jne .Lbad_pointer
    mov base(%rip), %eax
    increment %eax # INCREMENT( is not a preprocessor invocation
    call identity
    ret
.Lbad_pointer:
    mov $1, %eax
    ret
.size calculate, .-calculate
.endif
.endif
.endif

.data
.p2align 3
.globl base_pointer
.type base_pointer, @object
base_pointer:
    .quad base+4
.size base_pointer, .-base_pointer
.section .note.GNU-stack,"",@progbits
