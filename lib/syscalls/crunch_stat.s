.section .text
.global _crunch_stat
.type _crunch_stat, @function

#
# Crunch prototype:
# int _crunch_stat(const byte path[], struct stat *buf);
#
# C-prototype:
# int _crunch_stat(const char *path, struct stat *buf)
#
# Inputs (System V AMD64 ABI):
#   %rdi = path
#   %rsi = buf
#
# Return:
#   %eax = 0 on success, negative error number on failure

_crunch_stat:
    movq    %rsi, %rdx       # buf -> syscall arg 3
    movq    %rdi, %rsi       # path -> syscall arg 2
    movq    $-100, %rdi      # arg 1: AT_FDCWD
    xorl    %r10d, %r10d     # arg 4: flags = 0
    movl    $262, %eax       # SYS_newfstatat
    syscall
    ret

.size _crunch_stat, .-_crunch_stat

.section .note.GNU-stack,"",@progbits
