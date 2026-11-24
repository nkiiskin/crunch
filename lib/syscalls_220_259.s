	.text
	# Inputs: id, operations, count, timeout. Output: result or -1 with errno set.
	.globl __crunch_semtimedop
	.type __crunch_semtimedop, @function
__crunch_semtimedop:
	mov %rcx, %r10
	mov $220, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_semtimedop, .-__crunch_semtimedop

	# Inputs: descriptor, offset, length, advice. Output: result or -1 with errno set.
	.globl __crunch_crunch_fadvise64
	.type __crunch_crunch_fadvise64, @function
__crunch_crunch_fadvise64:
	mov %rcx, %r10
	mov $221, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_fadvise64, .-__crunch_crunch_fadvise64

	# Inputs: clock, event, timer. Output: result or -1 with errno set.
	.globl __crunch_crunch_timer_create
	.type __crunch_crunch_timer_create, @function
__crunch_crunch_timer_create:
	mov $222, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_timer_create, .-__crunch_crunch_timer_create

	# Inputs: timer, flags, value, previous. Output: result or -1 with errno set.
	.globl __crunch_crunch_timer_settime
	.type __crunch_crunch_timer_settime, @function
__crunch_crunch_timer_settime:
	mov %rcx, %r10
	mov $223, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_timer_settime, .-__crunch_crunch_timer_settime

	# Inputs: timer, value. Output: result or -1 with errno set.
	.globl __crunch_crunch_timer_gettime
	.type __crunch_crunch_timer_gettime, @function
__crunch_crunch_timer_gettime:
	mov $224, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_timer_gettime, .-__crunch_crunch_timer_gettime

	# Inputs: timer. Output: result or -1 with errno set.
	.globl __crunch_crunch_timer_getoverrun
	.type __crunch_crunch_timer_getoverrun, @function
__crunch_crunch_timer_getoverrun:
	mov $225, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_timer_getoverrun, .-__crunch_crunch_timer_getoverrun

	# Inputs: timer. Output: result or -1 with errno set.
	.globl __crunch_crunch_timer_delete
	.type __crunch_crunch_timer_delete, @function
__crunch_crunch_timer_delete:
	mov $226, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_timer_delete, .-__crunch_crunch_timer_delete

	# Inputs: clock, value. Output: result or -1 with errno set.
	.globl __crunch_clock_settime
	.type __crunch_clock_settime, @function
__crunch_clock_settime:
	mov $227, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_clock_settime, .-__crunch_clock_settime

	# Inputs: clock, value. Output: result or -1 with errno set.
	.globl __crunch_clock_gettime
	.type __crunch_clock_gettime, @function
__crunch_clock_gettime:
	mov $228, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_clock_gettime, .-__crunch_clock_gettime

	# Inputs: clock, value. Output: result or -1 with errno set.
	.globl __crunch_clock_getres
	.type __crunch_clock_getres, @function
__crunch_clock_getres:
	mov $229, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_clock_getres, .-__crunch_clock_getres

	# Inputs: clock, flags, requested, remaining. Output: result or -1 with errno set.
	.globl __crunch_crunch_clock_nanosleep
	.type __crunch_crunch_clock_nanosleep, @function
__crunch_crunch_clock_nanosleep:
	mov %rcx, %r10
	mov $230, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_clock_nanosleep, .-__crunch_crunch_clock_nanosleep

	# Inputs: status. Output: none; terminates process.
	.globl __crunch_crunch_exit_group
	.type __crunch_crunch_exit_group, @function
__crunch_crunch_exit_group:
	mov $231, %eax
	syscall
	ud2
	.size __crunch_crunch_exit_group, .-__crunch_crunch_exit_group

	# Inputs: epoll, events, maximum, timeout. Output: result or -1 with errno set.
	.globl __crunch_epoll_wait
	.type __crunch_epoll_wait, @function
__crunch_epoll_wait:
	mov %rcx, %r10
	mov $232, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_epoll_wait, .-__crunch_epoll_wait

	# Inputs: epoll, operation, descriptor, event. Output: result or -1 with errno set.
	.globl __crunch_epoll_ctl
	.type __crunch_epoll_ctl, @function
__crunch_epoll_ctl:
	mov %rcx, %r10
	mov $233, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_epoll_ctl, .-__crunch_epoll_ctl

	# Inputs: process, thread, signal. Output: result or -1 with errno set.
	.globl __crunch_tgkill
	.type __crunch_tgkill, @function
__crunch_tgkill:
	mov $234, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_tgkill, .-__crunch_tgkill

	# Inputs: path, times. Output: result or -1 with errno set.
	.globl __crunch_utimes
	.type __crunch_utimes, @function
__crunch_utimes:
	mov $235, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_utimes, .-__crunch_utimes

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_crunch_vserver
	.type __crunch_crunch_vserver, @function
__crunch_crunch_vserver:
	mov $236, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_vserver, .-__crunch_crunch_vserver

	# Inputs: address, length, mode, nodes, maxnode, flags. Output: result or -1 with errno set.
	.globl __crunch_mbind
	.type __crunch_mbind, @function
__crunch_mbind:
	mov %rcx, %r10
	mov $237, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mbind, .-__crunch_mbind

	# Inputs: mode, nodes, maxnode. Output: result or -1 with errno set.
	.globl __crunch_set_mempolicy
	.type __crunch_set_mempolicy, @function
__crunch_set_mempolicy:
	mov $238, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_set_mempolicy, .-__crunch_set_mempolicy

	# Inputs: mode, nodes, maxnode, address, flags. Output: result or -1 with errno set.
	.globl __crunch_get_mempolicy
	.type __crunch_get_mempolicy, @function
__crunch_get_mempolicy:
	mov %rcx, %r10
	mov $239, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_get_mempolicy, .-__crunch_get_mempolicy

	# Inputs: name, flags, mode, attributes. Output: result or -1 with errno set.
	.globl __crunch_crunch_mq_open
	.type __crunch_crunch_mq_open, @function
__crunch_crunch_mq_open:
	mov %rcx, %r10
	mov $240, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_mq_open, .-__crunch_crunch_mq_open

	# Inputs: name. Output: result or -1 with errno set.
	.globl __crunch_crunch_mq_unlink
	.type __crunch_crunch_mq_unlink, @function
__crunch_crunch_mq_unlink:
	mov $241, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_mq_unlink, .-__crunch_crunch_mq_unlink

	# Inputs: queue, message, length, priority, timeout. Output: result or -1 with errno set.
	.globl __crunch_crunch_mq_timedsend
	.type __crunch_crunch_mq_timedsend, @function
__crunch_crunch_mq_timedsend:
	mov %rcx, %r10
	mov $242, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_mq_timedsend, .-__crunch_crunch_mq_timedsend

	# Inputs: queue, message, length, priority, timeout. Output: result or -1 with errno set.
	.globl __crunch_crunch_mq_timedreceive
	.type __crunch_crunch_mq_timedreceive, @function
__crunch_crunch_mq_timedreceive:
	mov %rcx, %r10
	mov $243, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_mq_timedreceive, .-__crunch_crunch_mq_timedreceive

	# Inputs: queue, event. Output: result or -1 with errno set.
	.globl __crunch_crunch_mq_notify
	.type __crunch_crunch_mq_notify, @function
__crunch_crunch_mq_notify:
	mov $244, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_mq_notify, .-__crunch_crunch_mq_notify

	# Inputs: queue, attributes, previous. Output: result or -1 with errno set.
	.globl __crunch_crunch_mq_getsetattr
	.type __crunch_crunch_mq_getsetattr, @function
__crunch_crunch_mq_getsetattr:
	mov $245, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_mq_getsetattr, .-__crunch_crunch_mq_getsetattr

	# Inputs: entry, count, segments, flags. Output: result or -1 with errno set.
	.globl __crunch_crunch_kexec_load
	.type __crunch_crunch_kexec_load, @function
__crunch_crunch_kexec_load:
	mov %rcx, %r10
	mov $246, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_kexec_load, .-__crunch_crunch_kexec_load

	# Inputs: type, id, information, options, usage. Output: result or -1 with errno set.
	.globl __crunch_crunch_waitid
	.type __crunch_crunch_waitid, @function
__crunch_crunch_waitid:
	mov %rcx, %r10
	mov $247, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_waitid, .-__crunch_crunch_waitid

	# Inputs: type, description, payload, length, keyring. Output: result or -1 with errno set.
	.globl __crunch_add_key
	.type __crunch_add_key, @function
__crunch_add_key:
	mov %rcx, %r10
	mov $248, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_add_key, .-__crunch_add_key

	# Inputs: type, description, callout, keyring. Output: result or -1 with errno set.
	.globl __crunch_request_key
	.type __crunch_request_key, @function
__crunch_request_key:
	mov %rcx, %r10
	mov $249, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_request_key, .-__crunch_request_key

	# Inputs: operation, arg2, arg3, arg4, arg5. Output: result or -1 with errno set.
	.globl __crunch_crunch_keyctl
	.type __crunch_crunch_keyctl, @function
__crunch_crunch_keyctl:
	mov %rcx, %r10
	mov $250, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_keyctl, .-__crunch_crunch_keyctl

	# Inputs: which, who, priority. Output: result or -1 with errno set.
	.globl __crunch_ioprio_set
	.type __crunch_ioprio_set, @function
__crunch_ioprio_set:
	mov $251, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_ioprio_set, .-__crunch_ioprio_set

	# Inputs: which, who. Output: result or -1 with errno set.
	.globl __crunch_ioprio_get
	.type __crunch_ioprio_get, @function
__crunch_ioprio_get:
	mov $252, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_ioprio_get, .-__crunch_ioprio_get

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_inotify_init
	.type __crunch_inotify_init, @function
__crunch_inotify_init:
	mov $253, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_inotify_init, .-__crunch_inotify_init

	# Inputs: descriptor, path, mask. Output: result or -1 with errno set.
	.globl __crunch_inotify_add_watch
	.type __crunch_inotify_add_watch, @function
__crunch_inotify_add_watch:
	mov $254, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_inotify_add_watch, .-__crunch_inotify_add_watch

	# Inputs: descriptor, watch. Output: result or -1 with errno set.
	.globl __crunch_inotify_rm_watch
	.type __crunch_inotify_rm_watch, @function
__crunch_inotify_rm_watch:
	mov $255, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_inotify_rm_watch, .-__crunch_inotify_rm_watch

	# Inputs: process, maxnode, from, to. Output: result or -1 with errno set.
	.globl __crunch_migrate_pages
	.type __crunch_migrate_pages, @function
__crunch_migrate_pages:
	mov %rcx, %r10
	mov $256, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_migrate_pages, .-__crunch_migrate_pages

	# Inputs: directory, path, mode, device. Output: result or -1 with errno set.
	.globl __crunch_mknodat
	.type __crunch_mknodat, @function
__crunch_mknodat:
	mov %rcx, %r10
	mov $259, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mknodat, .-__crunch_mknodat

	.section .note.GNU-stack,"",@progbits
