	.text
	# Inputs: directory, path, owner, group, flags. Output: result or -1 with errno set.
	.globl __crunch_fchownat
	.type __crunch_fchownat, @function
__crunch_fchownat:
	mov %rcx, %r10
	mov $260, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fchownat, .-__crunch_fchownat

	# Inputs: directory, path, times. Output: result or -1 with errno set.
	.globl __crunch_futimesat
	.type __crunch_futimesat, @function
__crunch_futimesat:
	mov $261, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_futimesat, .-__crunch_futimesat

	# Inputs: old_directory, old_path, new_directory, new_path, flags. Output: result or -1 with errno set.
	.globl __crunch_linkat
	.type __crunch_linkat, @function
__crunch_linkat:
	mov %rcx, %r10
	mov $265, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_linkat, .-__crunch_linkat

	# Inputs: directory, path, mode. Output: result or -1 with errno set.
	.globl __crunch_crunch_fchmodat
	.type __crunch_crunch_fchmodat, @function
__crunch_crunch_fchmodat:
	mov $268, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_fchmodat, .-__crunch_crunch_fchmodat

	# Inputs: directory, path, mode. Output: result or -1 with errno set.
	.globl __crunch_crunch_faccessat
	.type __crunch_crunch_faccessat, @function
__crunch_crunch_faccessat:
	mov $269, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_faccessat, .-__crunch_crunch_faccessat

	# Inputs: count, readfds, writefds, exceptfds, timeout, signals. Output: result or -1 with errno set.
	.globl __crunch_crunch_pselect6
	.type __crunch_crunch_pselect6, @function
__crunch_crunch_pselect6:
	mov %rcx, %r10
	mov $270, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_pselect6, .-__crunch_crunch_pselect6

	# Inputs: descriptors, count, timeout, mask. Output: result or -1 with errno set.
	.globl __crunch_crunch_ppoll
	.type __crunch_crunch_ppoll, @function
__crunch_crunch_ppoll:
	mov %rcx, %r10
	mov $8, %r8d
	mov $271, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_ppoll, .-__crunch_crunch_ppoll

	# Inputs: flags. Output: result or -1 with errno set.
	.globl __crunch_unshare
	.type __crunch_unshare, @function
__crunch_unshare:
	mov $272, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_unshare, .-__crunch_unshare

	# Inputs: head, length. Output: result or -1 with errno set.
	.globl __crunch_set_robust_list
	.type __crunch_set_robust_list, @function
__crunch_set_robust_list:
	mov $273, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_set_robust_list, .-__crunch_set_robust_list

	# Inputs: process, head, length. Output: result or -1 with errno set.
	.globl __crunch_get_robust_list
	.type __crunch_get_robust_list, @function
__crunch_get_robust_list:
	mov $274, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_get_robust_list, .-__crunch_get_robust_list

	# Inputs: input, input_offset, output, output_offset, length, flags. Output: result or -1 with errno set.
	.globl __crunch_splice
	.type __crunch_splice, @function
__crunch_splice:
	mov %rcx, %r10
	mov $275, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_splice, .-__crunch_splice

	# Inputs: input, output, length, flags. Output: result or -1 with errno set.
	.globl __crunch_tee
	.type __crunch_tee, @function
__crunch_tee:
	mov %rcx, %r10
	mov $276, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_tee, .-__crunch_tee

	# Inputs: descriptor, offset, length, flags. Output: result or -1 with errno set.
	.globl __crunch_sync_file_range
	.type __crunch_sync_file_range, @function
__crunch_sync_file_range:
	mov %rcx, %r10
	mov $277, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sync_file_range, .-__crunch_sync_file_range

	# Inputs: descriptor, vectors, count, flags. Output: result or -1 with errno set.
	.globl __crunch_vmsplice
	.type __crunch_vmsplice, @function
__crunch_vmsplice:
	mov %rcx, %r10
	mov $278, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_vmsplice, .-__crunch_vmsplice

	# Inputs: process, count, pages, nodes, status, flags. Output: result or -1 with errno set.
	.globl __crunch_move_pages
	.type __crunch_move_pages, @function
__crunch_move_pages:
	mov %rcx, %r10
	mov $279, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_move_pages, .-__crunch_move_pages

	# Inputs: directory, path, times, flags. Output: result or -1 with errno set.
	.globl __crunch_utimensat
	.type __crunch_utimensat, @function
__crunch_utimensat:
	mov %rcx, %r10
	mov $280, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_utimensat, .-__crunch_utimensat

	# Inputs: epoll, events, maximum, timeout, mask. Output: result or -1 with errno set.
	.globl __crunch_epoll_pwait
	.type __crunch_epoll_pwait, @function
__crunch_epoll_pwait:
	mov %rcx, %r10
	mov $8, %r9d
	mov $281, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_epoll_pwait, .-__crunch_epoll_pwait

	# Inputs: descriptor, mask. Output: result or -1 with errno set.
	.globl __crunch_crunch_signalfd
	.type __crunch_crunch_signalfd, @function
__crunch_crunch_signalfd:
	mov $8, %edx
	mov $282, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_signalfd, .-__crunch_crunch_signalfd

	# Inputs: clock, flags. Output: result or -1 with errno set.
	.globl __crunch_timerfd_create
	.type __crunch_timerfd_create, @function
__crunch_timerfd_create:
	mov $283, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_timerfd_create, .-__crunch_timerfd_create

	# Inputs: value. Output: result or -1 with errno set.
	.globl __crunch_crunch_eventfd
	.type __crunch_crunch_eventfd, @function
__crunch_crunch_eventfd:
	mov $284, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_eventfd, .-__crunch_crunch_eventfd

	# Inputs: descriptor, mode, offset, length. Output: result or -1 with errno set.
	.globl __crunch_fallocate
	.type __crunch_fallocate, @function
__crunch_fallocate:
	mov %rcx, %r10
	mov $285, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_fallocate, .-__crunch_fallocate

	# Inputs: descriptor, flags, value, previous. Output: result or -1 with errno set.
	.globl __crunch_timerfd_settime
	.type __crunch_timerfd_settime, @function
__crunch_timerfd_settime:
	mov %rcx, %r10
	mov $286, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_timerfd_settime, .-__crunch_timerfd_settime

	# Inputs: descriptor, value. Output: result or -1 with errno set.
	.globl __crunch_timerfd_gettime
	.type __crunch_timerfd_gettime, @function
__crunch_timerfd_gettime:
	mov $287, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_timerfd_gettime, .-__crunch_timerfd_gettime

	# Inputs: descriptor, address, length, flags. Output: result or -1 with errno set.
	.globl __crunch_accept4
	.type __crunch_accept4, @function
__crunch_accept4:
	mov %rcx, %r10
	mov $288, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_accept4, .-__crunch_accept4

	# Inputs: descriptor, mask, flags. Output: result or -1 with errno set.
	.globl __crunch_signalfd
	.type __crunch_signalfd, @function
__crunch_signalfd:
	mov %edx, %r10d
	mov $8, %edx
	mov $289, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_signalfd, .-__crunch_signalfd

	# Inputs: value, flags. Output: result or -1 with errno set.
	.globl __crunch_eventfd
	.type __crunch_eventfd, @function
__crunch_eventfd:
	mov $290, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_eventfd, .-__crunch_eventfd

	# Inputs: flags. Output: result or -1 with errno set.
	.globl __crunch_epoll_create1
	.type __crunch_epoll_create1, @function
__crunch_epoll_create1:
	mov $291, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_epoll_create1, .-__crunch_epoll_create1

	# Inputs: old_descriptor, new_descriptor, flags. Output: result or -1 with errno set.
	.globl __crunch_dup3
	.type __crunch_dup3, @function
__crunch_dup3:
	mov $292, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_dup3, .-__crunch_dup3

	# Inputs: descriptors, flags. Output: result or -1 with errno set.
	.globl __crunch_pipe2
	.type __crunch_pipe2, @function
__crunch_pipe2:
	mov $293, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_pipe2, .-__crunch_pipe2

	# Inputs: flags. Output: result or -1 with errno set.
	.globl __crunch_inotify_init1
	.type __crunch_inotify_init1, @function
__crunch_inotify_init1:
	mov $294, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_inotify_init1, .-__crunch_inotify_init1

	# Inputs: descriptor, vectors, count, offset. Output: result or -1 with errno set.
	.globl __crunch_preadv
	.type __crunch_preadv, @function
__crunch_preadv:
	mov %ecx, %r10d
	shr $32, %rcx
	mov %ecx, %r8d
	mov $295, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_preadv, .-__crunch_preadv

	# Inputs: descriptor, vectors, count, offset. Output: result or -1 with errno set.
	.globl __crunch_pwritev
	.type __crunch_pwritev, @function
__crunch_pwritev:
	mov %ecx, %r10d
	shr $32, %rcx
	mov %ecx, %r8d
	mov $296, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_pwritev, .-__crunch_pwritev

	# Inputs: process, thread, signal, information. Output: result or -1 with errno set.
	.globl __crunch_rt_tgsigqueueinfo
	.type __crunch_rt_tgsigqueueinfo, @function
__crunch_rt_tgsigqueueinfo:
	mov %rcx, %r10
	mov $297, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_rt_tgsigqueueinfo, .-__crunch_rt_tgsigqueueinfo

	# Inputs: attributes, process, cpu, group, flags. Output: result or -1 with errno set.
	.globl __crunch_perf_event_open
	.type __crunch_perf_event_open, @function
__crunch_perf_event_open:
	mov %rcx, %r10
	mov $298, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_perf_event_open, .-__crunch_perf_event_open

	# Inputs: descriptor, messages, count, flags, timeout. Output: result or -1 with errno set.
	.globl __crunch_recvmmsg
	.type __crunch_recvmmsg, @function
__crunch_recvmmsg:
	mov %rcx, %r10
	mov $299, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_recvmmsg, .-__crunch_recvmmsg

	.section .note.GNU-stack,"",@progbits
