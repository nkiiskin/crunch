.section .text
.global _crunch_lseek
.type _crunch_lseek, @function
#
# Crunch prototype:
# long _crunch_lseek(int fd, long offset, int whence);
#
# C-prototype:
# off_t _crunch_lseek(int fd, off_t offset, int whence);
#
# Inputs (System V AMD64 ABI):
#   %rdi = fd
#   %rsi = offset
#   %rdx = whence
#
# Return:
#   %rax >= 0 : resulting file offset
#   %rax < 0  : negative error number

_crunch_lseek:
    movl    %edx, %r10d      # Not needed for lseek; whence stays in RDX
    movl    $8, %eax         # SYS_lseek = 8
    syscall
    ret

.size _crunch_lseek, .-_crunch_lseek

.section .note.GNU-stack,"",@progbits
