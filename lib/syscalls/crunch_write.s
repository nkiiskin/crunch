.section .text
.global _crunch_write
.type _crunch_write, @function
#
# Crunch prototype:
# long _crunch_write(int fd, const byte buf[], long count);
#
# C-prototype:
# ssize_t _crunch_write(int fd, const void *buf, size_t count)
#
# Inputs (System V AMD64 ABI):
#   %rdi = fd
#   %rsi = buf
#   %rdx = count
#
# Return:
#   %rax = number of bytes written, or negative error number

_crunch_write:
    movl    $1, %eax         # SYS_write = 1
    syscall
    ret

.size _crunch_write, .-_crunch_write

.section .note.GNU-stack,"",@progbits
