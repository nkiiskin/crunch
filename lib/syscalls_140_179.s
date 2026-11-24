	.text
	# Inputs: which, who. Output: result or -1 with errno set.
	.globl __crunch_getpriority
	.type __crunch_getpriority, @function
__crunch_getpriority:
	mov $140, %eax
	syscall
	cmp $-4095, %rax
	jae __crunch_syscall_result
	mov $20, %edx
	sub %eax, %edx
	mov %edx, %eax
	ret
	.size __crunch_getpriority, .-__crunch_getpriority

	# Inputs: which, who, priority. Output: result or -1 with errno set.
	.globl __crunch_setpriority
	.type __crunch_setpriority, @function
__crunch_setpriority:
	mov $141, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setpriority, .-__crunch_setpriority

	# Inputs: process, parameter. Output: result or -1 with errno set.
	.globl __crunch_sched_setparam
	.type __crunch_sched_setparam, @function
__crunch_sched_setparam:
	mov $142, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_setparam, .-__crunch_sched_setparam

	# Inputs: process, parameter. Output: result or -1 with errno set.
	.globl __crunch_sched_getparam
	.type __crunch_sched_getparam, @function
__crunch_sched_getparam:
	mov $143, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_getparam, .-__crunch_sched_getparam

	# Inputs: process, policy, parameter. Output: result or -1 with errno set.
	.globl __crunch_sched_setscheduler
	.type __crunch_sched_setscheduler, @function
__crunch_sched_setscheduler:
	mov $144, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_setscheduler, .-__crunch_sched_setscheduler

	# Inputs: process. Output: result or -1 with errno set.
	.globl __crunch_sched_getscheduler
	.type __crunch_sched_getscheduler, @function
__crunch_sched_getscheduler:
	mov $145, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_getscheduler, .-__crunch_sched_getscheduler

	# Inputs: policy. Output: result or -1 with errno set.
	.globl __crunch_sched_get_priority_max
	.type __crunch_sched_get_priority_max, @function
__crunch_sched_get_priority_max:
	mov $146, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_get_priority_max, .-__crunch_sched_get_priority_max

	# Inputs: policy. Output: result or -1 with errno set.
	.globl __crunch_sched_get_priority_min
	.type __crunch_sched_get_priority_min, @function
__crunch_sched_get_priority_min:
	mov $147, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_get_priority_min, .-__crunch_sched_get_priority_min

	# Inputs: process, interval. Output: result or -1 with errno set.
	.globl __crunch_sched_rr_get_interval
	.type __crunch_sched_rr_get_interval, @function
__crunch_sched_rr_get_interval:
	mov $148, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sched_rr_get_interval, .-__crunch_sched_rr_get_interval

	# Inputs: address, byte_count. Output: result or -1 with errno set.
	.globl __crunch_mlock
	.type __crunch_mlock, @function
__crunch_mlock:
	mov $149, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mlock, .-__crunch_mlock

	# Inputs: address, byte_count. Output: result or -1 with errno set.
	.globl __crunch_munlock
	.type __crunch_munlock, @function
__crunch_munlock:
	mov $150, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_munlock, .-__crunch_munlock

	# Inputs: flags. Output: result or -1 with errno set.
	.globl __crunch_mlockall
	.type __crunch_mlockall, @function
__crunch_mlockall:
	mov $151, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mlockall, .-__crunch_mlockall

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_munlockall
	.type __crunch_munlockall, @function
__crunch_munlockall:
	mov $152, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_munlockall, .-__crunch_munlockall

	# Inputs: none. Output: result or -1 with errno set.
	.globl __crunch_vhangup
	.type __crunch_vhangup, @function
__crunch_vhangup:
	mov $153, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_vhangup, .-__crunch_vhangup

	# Inputs: operation, buffer, byte_count. Output: result or -1 with errno set.
	.globl __crunch_modify_ldt
	.type __crunch_modify_ldt, @function
__crunch_modify_ldt:
	mov $154, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_modify_ldt, .-__crunch_modify_ldt

	# Inputs: new_root, put_old. Output: result or -1 with errno set.
	.globl __crunch_pivot_root
	.type __crunch_pivot_root, @function
__crunch_pivot_root:
	mov $155, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_pivot_root, .-__crunch_pivot_root

	# Inputs: arguments. Output: result or -1 with errno set.
	.globl __crunch_crunch_sysctl
	.type __crunch_crunch_sysctl, @function
__crunch_crunch_sysctl:
	mov $156, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_sysctl, .-__crunch_crunch_sysctl

	# Inputs: option, argument2, argument3, argument4, argument5. Output: result or -1 with errno set.
	.globl __crunch_prctl
	.type __crunch_prctl, @function
__crunch_prctl:
	mov %rcx, %r10
	mov $157, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_prctl, .-__crunch_prctl

	# Inputs: option, pointer, three integer arguments. Output: result or -1 with errno set.
	.globl __crunch_prctl_ptr
	.type __crunch_prctl_ptr, @function
__crunch_prctl_ptr:
	jmp __crunch_prctl
	.size __crunch_prctl_ptr, .-__crunch_prctl_ptr

	# Inputs: operation, argument. Output: result or -1 with errno set.
	.globl __crunch_arch_prctl
	.type __crunch_arch_prctl, @function
__crunch_arch_prctl:
	mov $158, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_arch_prctl, .-__crunch_arch_prctl

	# Inputs: operation and pointer. Output: result or -1 with errno set.
	.globl __crunch_arch_prctl_ptr
	.type __crunch_arch_prctl_ptr, @function
__crunch_arch_prctl_ptr:
	jmp __crunch_arch_prctl
	.size __crunch_arch_prctl_ptr, .-__crunch_arch_prctl_ptr

	# Inputs: state. Output: result or -1 with errno set.
	.globl __crunch_adjtimex
	.type __crunch_adjtimex, @function
__crunch_adjtimex:
	mov $159, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_adjtimex, .-__crunch_adjtimex

	# Inputs: resource, limits. Output: result or -1 with errno set.
	.globl __crunch_setrlimit
	.type __crunch_setrlimit, @function
__crunch_setrlimit:
	mov $160, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setrlimit, .-__crunch_setrlimit

	# Inputs: path. Output: result or -1 with errno set.
	.globl __crunch_chroot
	.type __crunch_chroot, @function
__crunch_chroot:
	mov $161, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_chroot, .-__crunch_chroot

	# Inputs: none. Output: no result.
	.globl __crunch_sync
	.type __crunch_sync, @function
__crunch_sync:
	mov $162, %eax
	syscall
	ret
	.size __crunch_sync, .-__crunch_sync

	# Inputs: path. Output: result or -1 with errno set.
	.globl __crunch_acct
	.type __crunch_acct, @function
__crunch_acct:
	mov $163, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_acct, .-__crunch_acct

	# Inputs: value, zone. Output: result or -1 with errno set.
	.globl __crunch_settimeofday
	.type __crunch_settimeofday, @function
__crunch_settimeofday:
	mov $164, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_settimeofday, .-__crunch_settimeofday

	# Inputs: source, target, filesystem, flags, data. Output: result or -1 with errno set.
	.globl __crunch_mount
	.type __crunch_mount, @function
__crunch_mount:
	mov %rcx, %r10
	mov $165, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_mount, .-__crunch_mount

	# Inputs: target, flags. Output: result or -1 with errno set.
	.globl __crunch_umount2
	.type __crunch_umount2, @function
__crunch_umount2:
	mov $166, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_umount2, .-__crunch_umount2

	# Inputs: path, flags. Output: result or -1 with errno set.
	.globl __crunch_swapon
	.type __crunch_swapon, @function
__crunch_swapon:
	mov $167, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_swapon, .-__crunch_swapon

	# Inputs: path. Output: result or -1 with errno set.
	.globl __crunch_swapoff
	.type __crunch_swapoff, @function
__crunch_swapoff:
	mov $168, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_swapoff, .-__crunch_swapoff

	# Inputs: command, argument. Output: result or -1 with errno set.
	.globl __crunch_crunch_reboot
	.type __crunch_crunch_reboot, @function
__crunch_crunch_reboot:
	mov %rsi, %r10
	mov %edi, %edx
	mov $0xfee1dead, %edi
	mov $672274793, %esi
	mov $169, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_reboot, .-__crunch_crunch_reboot

	# Inputs: name, length. Output: result or -1 with errno set.
	.globl __crunch_sethostname
	.type __crunch_sethostname, @function
__crunch_sethostname:
	mov $170, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_sethostname, .-__crunch_sethostname

	# Inputs: name, length. Output: result or -1 with errno set.
	.globl __crunch_setdomainname
	.type __crunch_setdomainname, @function
__crunch_setdomainname:
	mov $171, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_setdomainname, .-__crunch_setdomainname

	# Inputs: level. Output: result or -1 with errno set.
	.globl __crunch_iopl
	.type __crunch_iopl, @function
__crunch_iopl:
	mov $172, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_iopl, .-__crunch_iopl

	# Inputs: first, count, enable. Output: result or -1 with errno set.
	.globl __crunch_ioperm
	.type __crunch_ioperm, @function
__crunch_ioperm:
	mov $173, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_ioperm, .-__crunch_ioperm

	# Inputs: name, size. Output: result or -1 with errno set.
	.globl __crunch_crunch_create_module
	.type __crunch_crunch_create_module, @function
__crunch_crunch_create_module:
	mov $174, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_create_module, .-__crunch_crunch_create_module

	# Inputs: image, length, parameters. Output: result or -1 with errno set.
	.globl __crunch_init_module
	.type __crunch_init_module, @function
__crunch_init_module:
	mov $175, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_init_module, .-__crunch_init_module

	# Inputs: name, flags. Output: result or -1 with errno set.
	.globl __crunch_delete_module
	.type __crunch_delete_module, @function
__crunch_delete_module:
	mov $176, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_delete_module, .-__crunch_delete_module

	# Inputs: symbols. Output: result or -1 with errno set.
	.globl __crunch_crunch_get_kernel_syms
	.type __crunch_crunch_get_kernel_syms, @function
__crunch_crunch_get_kernel_syms:
	mov $177, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_get_kernel_syms, .-__crunch_crunch_get_kernel_syms

	# Inputs: name, which, buffer, size, result. Output: result or -1 with errno set.
	.globl __crunch_crunch_query_module
	.type __crunch_crunch_query_module, @function
__crunch_crunch_query_module:
	mov %rcx, %r10
	mov $178, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_crunch_query_module, .-__crunch_crunch_query_module

	# Inputs: command, special, id, address. Output: result or -1 with errno set.
	.globl __crunch_quotactl
	.type __crunch_quotactl, @function
__crunch_quotactl:
	mov %rcx, %r10
	mov $179, %eax
	syscall
	jmp __crunch_syscall_result
	.size __crunch_quotactl, .-__crunch_quotactl

	.section .note.GNU-stack,"",@progbits
