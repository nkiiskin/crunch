.section .text
.global _crunch_close
.type _crunch_close, @function
#
# Crunch prototype:
# int _crunch_close(int fd);
#
# C-prototype:
# int _crunch_close(int fd);
#
# Input (System V AMD64 ABI):
#   %rdi = fd
#
# Return:
#   %rax = 0 on success, or negative error number

_crunch_close:
    movl    $3, %eax         # SYS_close = 3
    syscall
    ret

.size _crunch_close, .-_crunch_close

.section .note.GNU-stack,"",@progbits
