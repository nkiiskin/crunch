.section .text
.global _crunch_open
.type _crunch_open, @function

# Crunch prototype:
# int _crunch_open(const byte path[], int flags, int mode);
#
# C-prototype:
# int _crunch_open(const char *path, int flags, int mode);
#
# Inputs (System V AMD64 ABI):
#   %rdi = path
#   %rsi = flags
#   %rdx = mode
#
# Return:
#   %eax = file descriptor or negative error number

_crunch_open:
    movl    %esi, %r8d       # Save flags
    movl    %edx, %r10d      # mode -> syscall arg 4
    movq    %rdi, %rsi       # pathname -> syscall arg 2
    movq    $-100, %rdi      # AT_FDCWD
    movl    %r8d, %edx       # flags -> syscall arg 3
    movl    $257, %eax       # SYS_openat
    syscall
    ret

.size _crunch_open, .-_crunch_open

.section .note.GNU-stack,"",@progbits
