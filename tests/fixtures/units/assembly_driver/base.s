.data
.p2align 2
.globl base
.type base, @object
base:
    .long 40
.size base, .-base
.comm scratch,8,8

.text
.globl identity
.type identity, @function
identity:
    ret
.size identity, .-identity
.section .note.GNU-stack,"",@progbits
