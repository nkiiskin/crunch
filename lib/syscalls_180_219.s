	.text
	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_nfsservctl
	.type __crunch_crunch_nfsservctl, @function
__crunch_crunch_nfsservctl:
	mov $180, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_nfsservctl, .-__crunch_crunch_nfsservctl

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_getpmsg
	.type __crunch_crunch_getpmsg, @function
__crunch_crunch_getpmsg:
	mov $181, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_getpmsg, .-__crunch_crunch_getpmsg

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_putpmsg
	.type __crunch_crunch_putpmsg, @function
__crunch_crunch_putpmsg:
	mov $182, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_putpmsg, .-__crunch_crunch_putpmsg

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_afs_syscall
	.type __crunch_crunch_afs_syscall, @function
__crunch_crunch_afs_syscall:
	mov $183, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_afs_syscall, .-__crunch_crunch_afs_syscall

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_tuxcall
	.type __crunch_crunch_tuxcall, @function
__crunch_crunch_tuxcall:
	mov $184, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_tuxcall, .-__crunch_crunch_tuxcall

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_security
	.type __crunch_crunch_security, @function
__crunch_crunch_security:
	mov $185, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_security, .-__crunch_crunch_security

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_gettid
	.type __crunch_gettid, @function
__crunch_gettid:
	mov $186, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_gettid, .-__crunch_gettid

	# Inputs: descriptor, offset, byte_count. Output: result or -1 with errno set.
	.globl __crunch_readahead
	.type __crunch_readahead, @function
__crunch_readahead:
	mov $187, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_readahead, .-__crunch_readahead

	# Inputs: path, name, value, size, flags. Output: result or -1 with errno set.
	.globl __crunch_setxattr
	.type __crunch_setxattr, @function
__crunch_setxattr:
	mov %rcx, %r10
	mov $188, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setxattr, .-__crunch_setxattr

	# Inputs: path, name, value, size, flags. Output: result or -1 with errno set.
	.globl __crunch_lsetxattr
	.type __crunch_lsetxattr, @function
__crunch_lsetxattr:
	mov %rcx, %r10
	mov $189, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_lsetxattr, .-__crunch_lsetxattr

	# Inputs: descriptor, name, value, size, flags. Output: result or -1 with errno set.
	.globl __crunch_fsetxattr
	.type __crunch_fsetxattr, @function
__crunch_fsetxattr:
	mov %rcx, %r10
	mov $190, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fsetxattr, .-__crunch_fsetxattr

	# Inputs: path, name, value, size. Output: result or -1 with errno set.
	.globl __crunch_getxattr
	.type __crunch_getxattr, @function
__crunch_getxattr:
	mov %rcx, %r10
	mov $191, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_getxattr, .-__crunch_getxattr

	# Inputs: path, name, value, size. Output: result or -1 with errno set.
	.globl __crunch_lgetxattr
	.type __crunch_lgetxattr, @function
__crunch_lgetxattr:
	mov %rcx, %r10
	mov $192, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_lgetxattr, .-__crunch_lgetxattr

	# Inputs: descriptor, name, value, size. Output: result or -1 with errno set.
	.globl __crunch_fgetxattr
	.type __crunch_fgetxattr, @function
__crunch_fgetxattr:
	mov %rcx, %r10
	mov $193, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fgetxattr, .-__crunch_fgetxattr

	# Inputs: path, list, size. Output: result or -1 with errno set.
	.globl __crunch_listxattr
	.type __crunch_listxattr, @function
__crunch_listxattr:
	mov $194, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_listxattr, .-__crunch_listxattr

	# Inputs: path, list, size. Output: result or -1 with errno set.
	.globl __crunch_llistxattr
	.type __crunch_llistxattr, @function
__crunch_llistxattr:
	mov $195, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_llistxattr, .-__crunch_llistxattr

	# Inputs: descriptor, list, size. Output: result or -1 with errno set.
	.globl __crunch_flistxattr
	.type __crunch_flistxattr, @function
__crunch_flistxattr:
	mov $196, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_flistxattr, .-__crunch_flistxattr

	# Inputs: path, name. Output: result or -1 with errno set.
	.globl __crunch_removexattr
	.type __crunch_removexattr, @function
__crunch_removexattr:
	mov $197, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_removexattr, .-__crunch_removexattr

	# Inputs: path, name. Output: result or -1 with errno set.
	.globl __crunch_lremovexattr
	.type __crunch_lremovexattr, @function
__crunch_lremovexattr:
	mov $198, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_lremovexattr, .-__crunch_lremovexattr

	# Inputs: descriptor, name. Output: result or -1 with errno set.
	.globl __crunch_fremovexattr
	.type __crunch_fremovexattr, @function
__crunch_fremovexattr:
	mov $199, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fremovexattr, .-__crunch_fremovexattr

	# Inputs: thread, signal. Output: result or -1 with errno set.
	.globl __crunch_tkill
	.type __crunch_tkill, @function
__crunch_tkill:
	mov $200, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_tkill, .-__crunch_tkill

	# Inputs: result. Output: result or -1 with errno set.
	.globl __crunch_time
	.type __crunch_time, @function
__crunch_time:
	mov $201, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_time, .-__crunch_time

	# Inputs: address, operation, value, timeout, address2, value3. Output: result or -1 with errno set.
	.globl __crunch_futex
	.type __crunch_futex, @function
__crunch_futex:
	mov %rcx, %r10
	mov $202, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_futex, .-__crunch_futex

	# Inputs: address, operation, value, value2, address2, value3. Output: result or -1 with errno set.
	.globl __crunch_futex_value
	.type __crunch_futex_value, @function
__crunch_futex_value:
	mov %rcx, %r10
	mov $202, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_futex_value, .-__crunch_futex_value

	# Inputs: process, size, mask. Output: result or -1 with errno set.
	.globl __crunch_sched_setaffinity
	.type __crunch_sched_setaffinity, @function
__crunch_sched_setaffinity:
	mov $203, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_setaffinity, .-__crunch_sched_setaffinity

	# Inputs: process, size, mask. Output: result or -1 with errno set.
	.globl __crunch_sched_getaffinity
	.type __crunch_sched_getaffinity, @function
__crunch_sched_getaffinity:
	mov $204, %eax
	syscall
	cmp $-4095, %rax
	jae __crunch_syscall_result
	lea (%rdx,%rax), %rdi
	mov %rsi, %rcx
	sub %rax, %rcx
	xor %eax, %eax
	rep stosb
	ret
	.size __crunch_sched_getaffinity, .-__crunch_sched_getaffinity

	# Inputs: descriptor. Output: result or -1 with errno set.
	.globl __crunch_crunch_set_thread_area
	.type __crunch_crunch_set_thread_area, @function
__crunch_crunch_set_thread_area:
	mov $205, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_set_thread_area, .-__crunch_crunch_set_thread_area

	# Inputs: max_events, context. Output: result or -1 with errno set.
	.globl __crunch_io_setup
	.type __crunch_io_setup, @function
__crunch_io_setup:
	mov $206, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_io_setup, .-__crunch_io_setup

	# Inputs: context. Output: result or -1 with errno set.
	.globl __crunch_io_destroy
	.type __crunch_io_destroy, @function
__crunch_io_destroy:
	mov $207, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_io_destroy, .-__crunch_io_destroy

	# Inputs: context, minimum, maximum, events, timeout. Output: result or -1 with errno set.
	.globl __crunch_io_getevents
	.type __crunch_io_getevents, @function
__crunch_io_getevents:
	mov %rcx, %r10
	mov $208, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_io_getevents, .-__crunch_io_getevents

	# Inputs: context, count, requests. Output: result or -1 with errno set.
	.globl __crunch_io_submit
	.type __crunch_io_submit, @function
__crunch_io_submit:
	mov $209, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_io_submit, .-__crunch_io_submit

	# Inputs: context, request, result. Output: result or -1 with errno set.
	.globl __crunch_io_cancel
	.type __crunch_io_cancel, @function
__crunch_io_cancel:
	mov $210, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_io_cancel, .-__crunch_io_cancel

	# Inputs: descriptor. Output: result or -1 with errno set.
	.globl __crunch_crunch_get_thread_area
	.type __crunch_crunch_get_thread_area, @function
__crunch_crunch_get_thread_area:
	mov $211, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_get_thread_area, .-__crunch_crunch_get_thread_area

	# Inputs: cookie, buffer, size. Output: result or -1 with errno set.
	.globl __crunch_lookup_dcookie
	.type __crunch_lookup_dcookie, @function
__crunch_lookup_dcookie:
	mov $212, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_lookup_dcookie, .-__crunch_lookup_dcookie

	# Inputs: size. Output: result or -1 with errno set.
	.globl __crunch_epoll_create
	.type __crunch_epoll_create, @function
__crunch_epoll_create:
	mov $213, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_epoll_create, .-__crunch_epoll_create

	# Inputs: epoll, operation, descriptor, event. Output: result or -1 with errno set.
	.globl __crunch_crunch_epoll_ctl_old
	.type __crunch_crunch_epoll_ctl_old, @function
__crunch_crunch_epoll_ctl_old:
	mov %rcx, %r10
	mov $214, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_epoll_ctl_old, .-__crunch_crunch_epoll_ctl_old

	# Inputs: epoll, events, maximum, timeout. Output: result or -1 with errno set.
	.globl __crunch_crunch_epoll_wait_old
	.type __crunch_crunch_epoll_wait_old, @function
__crunch_crunch_epoll_wait_old:
	mov %rcx, %r10
	mov $215, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_epoll_wait_old, .-__crunch_crunch_epoll_wait_old

	# Inputs: address, size, protection, page_offset, flags. Output: result or -1 with errno set.
	.globl __crunch_remap_file_pages
	.type __crunch_remap_file_pages, @function
__crunch_remap_file_pages:
	mov %rcx, %r10
	mov $216, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_remap_file_pages, .-__crunch_remap_file_pages

	# Inputs: address. Output: result or -1 with errno set.
	.globl __crunch_set_tid_address
	.type __crunch_set_tid_address, @function
__crunch_set_tid_address:
	mov $218, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_set_tid_address, .-__crunch_set_tid_address

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_restart_syscall
	.type __crunch_crunch_restart_syscall, @function
__crunch_crunch_restart_syscall:
	mov $219, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_restart_syscall, .-__crunch_crunch_restart_syscall

	.section .note.GNU-stack,"",@progbits
