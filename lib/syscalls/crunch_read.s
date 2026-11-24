.section .text
.global _crunch_read
.type _crunch_read, @function

#
# Crunch prototype:
# long _crunch_read(int fd, byte buf[], long count);
#
# C-prototype:
# ssize_t _crunch_read(int fd, void *buf, size_t count)
#
# Inputs (System V AMD64 ABI):
#   %rdi = fd
#   %rsi = buf
#   %rdx = count
#
# Return:
#   %rax = number of bytes read, or negative error number

_crunch_read:
    xorl    %eax, %eax       # SYS_read = 0
    syscall
    ret

.size _crunch_read, .-_crunch_read

.section .note.GNU-stack,"",@progbits
