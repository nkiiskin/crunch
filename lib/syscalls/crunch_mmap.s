.section .text
.global _crunch_mmap
.hidden _crunch_mmap
.type _crunch_mmap, @function
#
# Crunch-prototype:
# void *_crunch_mmap(void *addr, long length,
#                 int prot, int flags, int fd, long offset);
#
# C-prototype:
# void *_crunch_mmap(void *addr, size_t length,
#                 int prot, int flags, int fd, off_t offset);
#
# Inputs (System V AMD64 ABI):
#   %rdi = addr
#   %rsi = length
#   %rdx = prot
#   %rcx = flags
#   %r8  = fd
#   %r9  = offset
#
# Linux syscall ABI:
#   %rdi = addr
#   %rsi = length
#   %rdx = prot
#   %r10 = flags
#   %r8  = fd
#   %r9  = offset
#
# Return:
#   %rax = mapped address on success
#   %rax = -1 on failure, with errno set

_crunch_mmap:
    movq    %rcx, %r10      # flags -> syscall arg 4
    movl    $9, %eax        # SYS_mmap = 9
    syscall
    jmp     __crunch_syscall_result

.size _crunch_mmap, .-_crunch_mmap

.section .note.GNU-stack,"",@progbits
