.section .text
.global _crunch_munmap
.hidden _crunch_munmap
.type _crunch_munmap, @function
#
# Crunch-prototype:
# int _crunch_munmap(void *addr, long length);
#
# C-prototype:
# int _crunch_munmap(void *addr, size_t length);
#
# Inputs (System V AMD64 ABI):
#   %rdi = addr
#   %rsi = length
#
# Return:
#   %eax = 0 on success
#   %eax = -1 on failure, with errno set

_crunch_munmap:
    movl    $11, %eax       # SYS_munmap = 11
    syscall
    jmp     __crunch_syscall_result

.size _crunch_munmap, .-_crunch_munmap

.section .note.GNU-stack,"",@progbits
