	.text
	# Input: Linux error number. Output: pointer to a static error message.
	.globl __crunch_strerror
	.type __crunch_strerror, @function
__crunch_strerror:
	cmp $40, %edi
	ja .Lunknown
	lea .Lmessages(%rip), %rax
	test %edi, %edi
	je .Ldone
.Lskip:
	cmpb $0, (%rax)
	je .Lnext
	inc %rax
	jmp .Lskip
.Lnext:
	inc %rax
	dec %edi
	jne .Lskip
.Ldone:
	ret
.Lunknown:
	lea .Lunknown_message(%rip), %rax
	ret
	.size __crunch_strerror, .-__crunch_strerror
	.section .rodata
.Lmessages:
	.asciz "Success"
	.asciz "Operation not permitted"
	.asciz "No such file or directory"
	.asciz "No such process"
	.asciz "Interrupted system call"
	.asciz "Input/output error"
	.asciz "No such device or address"
	.asciz "Argument list too long"
	.asciz "Exec format error"
	.asciz "Bad file descriptor"
	.asciz "No child processes"
	.asciz "Resource temporarily unavailable"
	.asciz "Cannot allocate memory"
	.asciz "Permission denied"
	.asciz "Bad address"
	.asciz "Block device required"
	.asciz "Device or resource busy"
	.asciz "File exists"
	.asciz "Invalid cross-device link"
	.asciz "No such device"
	.asciz "Not a directory"
	.asciz "Is a directory"
	.asciz "Invalid argument"
	.asciz "Too many open files in system"
	.asciz "Too many open files"
	.asciz "Inappropriate ioctl for device"
	.asciz "Text file busy"
	.asciz "File too large"
	.asciz "No space left on device"
	.asciz "Illegal seek"
	.asciz "Read-only file system"
	.asciz "Too many links"
	.asciz "Broken pipe"
	.asciz "Numerical argument out of domain"
	.asciz "Numerical result out of range"
	.asciz "Resource deadlock avoided"
	.asciz "File name too long"
	.asciz "No locks available"
	.asciz "Function not implemented"
	.asciz "Directory not empty"
	.asciz "Too many levels of symbolic links"
.Lunknown_message:
	.asciz "Unknown error"
	.section .note.GNU-stack,"",@progbits
