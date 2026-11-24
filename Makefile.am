# Root Automake entry point; the established build rules live in build/Makefile.
ACLOCAL_AMFLAGS = -I m4

.PHONY: crunch-build crunch-test crunch-native-runtime doc FORCE
FORCE:

all-local: crunch-build
crunch-build:
	$(MAKE) -C $(srcdir)/build CONFIG_MK=$(abs_builddir)/build/config.mk BUILD_DIR=$(abs_builddir)/build/out SEARCH_LIB_DIR='$(prefix)/lib' all

check-local: crunch-test
crunch-test:
	$(MAKE) -C $(srcdir)/build CONFIG_MK=$(abs_builddir)/build/config.mk BUILD_DIR=$(abs_builddir)/build/out SEARCH_LIB_DIR='$(prefix)/lib' test

crunch-native-runtime:
	$(MAKE) -C $(srcdir)/build CONFIG_MK=$(abs_builddir)/build/config.mk BUILD_DIR=$(abs_builddir)/build/out native-runtime

# The man page documents the installed configuration file and header directory.
crunch_library_manpages = input fopen freopen fclose fseek ftell rewind fread fwrite fgetc getchar fputc putc putchar fgets getline fputs puts fprintf sprintf snprintf vsnprintf scanf fscanf perror stdin stdout stderr feof ferror clearerr open openat fstatat readlinkat unlinkat mkdirat renameat symlinkat read write close stat fstat lstat readlink getdents64 dirent64_next unlink rename mkdir rmdir lseek access dup dup2 pipe poll select fork getpid kill malloc calloc free realloc allocator_trim strtod strtof strtold strtok \
  socket bind connect listen accept recvfrom sendmsg recvmsg sigaction sigemptyset nanosleep sleep \
  mmap munmap mremap map_failed execve waitpid uname getrandom chmod fchmod _exit \
  file_open file_read file_write file_close mprotect brk sigprocmask ioctl ioctl_value pread pwrite readv \
  writev sched_yield msync mincore madvise shmget shmat shmctl shmdt pause alarm getitimer setitimer \
  sendfile sendto shutdown getsockname getpeername socketpair setsockopt getsockopt clone vfork \
  wait4 semget semop semctl semctl_value msgget msgsnd msgrcv msgctl fcntl fcntl_ptr flock \
  fsync fdatasync truncate ftruncate getdents getcwd \
  chdir fchdir creat link symlink chown fchown lchown umask gettimeofday getrlimit getrusage sysinfo \
  getpriority setpriority sched_setparam sched_getparam sched_setscheduler sched_getscheduler sched_get_priority_max sched_get_priority_min sched_rr_get_interval mlock munlock mlockall munlockall vhangup modify_ldt pivot_root crunch_sysctl prctl arch_prctl adjtimex setrlimit chroot sync acct settimeofday mount umount2 swapon swapoff crunch_reboot sethostname setdomainname iopl ioperm crunch_create_module init_module delete_module crunch_get_kernel_syms crunch_query_module quotactl \
  crunch_nfsservctl crunch_getpmsg crunch_putpmsg crunch_afs_syscall crunch_tuxcall crunch_security gettid readahead setxattr lsetxattr fsetxattr getxattr lgetxattr fgetxattr listxattr llistxattr flistxattr removexattr lremovexattr fremovexattr tkill time futex futex_value sched_setaffinity sched_getaffinity crunch_set_thread_area io_setup io_destroy io_getevents io_submit io_cancel crunch_get_thread_area lookup_dcookie epoll_create crunch_epoll_ctl_old crunch_epoll_wait_old remap_file_pages set_tid_address crunch_restart_syscall \
  times ptrace ptrace_value getuid klogctl getgid setuid setgid geteuid getegid setpgid getppid getpgrp setsid setreuid setregid getgroups setgroups setresuid getresuid setresgid getresgid getpgid setfsuid setfsgid getsid capget capset sigpending sigtimedwait rt_sigqueueinfo sigsuspend sigaltstack utime mknod uselib personality ustat statfs fstatfs crunch_sysfs crunch_sysfs_index crunch_sysfs_name prctl_ptr arch_prctl_ptr
crunch_library_manpages += semtimedop crunch_fadvise64 crunch_timer_create crunch_timer_settime crunch_timer_gettime crunch_timer_getoverrun crunch_timer_delete clock_settime clock_gettime clock_getres crunch_clock_nanosleep crunch_exit_group epoll_wait epoll_ctl tgkill utimes crunch_vserver mbind set_mempolicy get_mempolicy crunch_mq_open crunch_mq_unlink crunch_mq_timedsend crunch_mq_timedreceive crunch_mq_notify crunch_mq_getsetattr crunch_kexec_load crunch_waitid add_key request_key crunch_keyctl ioprio_set ioprio_get inotify_init inotify_add_watch inotify_rm_watch migrate_pages mknodat
crunch_library_manpages += fchownat futimesat linkat crunch_fchmodat crunch_faccessat crunch_pselect6 crunch_ppoll unshare set_robust_list get_robust_list splice tee sync_file_range vmsplice move_pages utimensat epoll_pwait crunch_signalfd timerfd_create crunch_eventfd fallocate timerfd_settime timerfd_gettime accept4 signalfd eventfd epoll_create1 dup3 pipe2 inotify_init1 preadv pwritev rt_tgsigqueueinfo perf_event_open recvmmsg
crunch_library_manoutputs = $(addprefix build/out/,$(addsuffix .3crunch,$(crunch_library_manpages)))

doc: build/out/crunch.1 $(crunch_library_manoutputs)
build/out/%.3crunch: $(srcdir)/man/%.3crunch
	$(MKDIR_P) build/out
	$(INSTALL_DATA) $< $@
# Always regenerated: make-time overrides such as prefix= change the paths.
build/out/crunch.1: FORCE
	$(MKDIR_P) build/out
	esc() { printf '%s' "$$1" | sed 's/\\/\\\\\\\\/g; s/-/\\\\-/g; s/[&|]/\\&/g'; }; \
	sed -e "s|%VERSION%|$(VERSION)|g" \
	    -e "s|%CONFFILE%|`esc '$(CRUNCH_INSTALL_CONFFILE)'`|g" \
	    -e "s|%INCLUDEDIR%|`esc '$(CRUNCH_INSTALL_INCLUDEDIR)'`|g" \
	    -e "s|%AS%|`esc '$(AS)'`|g" \
	    -e "s|%LD%|`esc '$(LD)'`|g" \
	    $(srcdir)/man/crunch.1.in > $@.tmp && mv $@.tmp $@

crunch_tools = crunch-lexer crunch-parser crunch-ir crunch-opt crunch-asm crunch-as crunch-ld crunch-ar
crunch_headers = glibc/alloca.h assert.h string.h stdio.h stdarg.h __crunch_va_list.h stdlib.h stddef.h stdalign.h stdint.h inttypes.h elf.h math.h float.h ctype.h environment.h errno.h limits.h stdbool.h \
  unistd.h fcntl.h poll.h signal.h dirent.h sys/types.h sys/stat.h sys/select.h sys/mman.h \
  sys/utsname.h sys/wait.h sys/random.h sys/socket.h sys/uio.h sys/un.h sys/ioctl.h time.h netinet/in.h \
  sched.h sys/ipc.h sys/shm.h sys/time.h sys/sendfile.h sys/resource.h sys/sem.h sys/msg.h sys/file.h sys/getdents.h sys/sysinfo.h \
  sys/prctl.h sys/arch_prctl.h sys/timex.h sys/mount.h sys/swap.h sys/reboot.h sys/io.h sys/module.h sys/syscall_extra.h \
  sys/xattr.h sys/futex.h sys/aio.h sys/epoll.h sys/syscall_220_259.h sys/syscall_260_299.h \
  sys/times.h sys/ptrace.h sys/klog.h sys/fsuid.h sys/capability.h utime.h sys/personality.h sys/vfs.h sys/legacy_syscalls.h

build/out/crunch.conf: FORCE
	$(MKDIR_P) build/out
	escaped=`printf '%s' '$(prefix)/lib' | sed 's/[\\&|]/\\&/g'`; \
	sed "s|^# library_path = @PREFIX@/lib$$|library_path = $$escaped|" \
	  '$(srcdir)/conf/crunch.conf' > $@.tmp && mv $@.tmp $@

install-exec-local: crunch-build
	$(MKDIR_P) '$(DESTDIR)$(bindir)'
	for tool in $(crunch_tools); do \
	  if test "$$tool" = crunch-ld; then continue; fi; \
	  $(INSTALL_PROGRAM) build/out/$$tool '$(DESTDIR)$(bindir)'/$$tool || exit 1; \
	done
	install_dir=`mktemp -d "$${TMPDIR:-/tmp}/crunch-install.XXXXXXXX"` || exit 1; \
	trap 'rm -f "$$install_dir/crunch" "$$install_dir/crunch-pp" "$$install_dir/crunch-ld"; rmdir "$$install_dir"' 0; \
	trap 'exit 1' 1 2 15; \
	$(MAKE) -C $(srcdir)/build CONFIG_MK=$(abs_builddir)/build/config.mk BUILD_DIR=$(abs_builddir)/build/out INSTALL_BUILD_DIR="$$install_dir" INSTALL_INCLUDE_DIR='$(CRUNCH_INSTALL_INCLUDEDIR)' INSTALL_CONF_FILE='$(CRUNCH_INSTALL_CONFFILE)' INSTALL_LIB_DIR='$(CRUNCH_INSTALL_LIBDIR)' SEARCH_LIB_DIR='$(prefix)/lib' install-pp && \
	$(INSTALL_PROGRAM) "$$install_dir/crunch-pp" '$(DESTDIR)$(bindir)'/crunch-pp && \
	$(INSTALL_PROGRAM) "$$install_dir/crunch" '$(DESTDIR)$(bindir)'/crunch && \
	$(INSTALL_PROGRAM) "$$install_dir/crunch-ld" '$(DESTDIR)$(bindir)'/crunch-ld

# An existing configuration file is never overwritten.
install-data-local: crunch-build doc build/out/crunch.conf
	$(MKDIR_P) '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'
	$(INSTALL_DATA) build/out/libcrunch.a '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch.a
	$(INSTALL_DATA) build/out/libcrunch-fallback.a '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch-fallback.a
	$(INSTALL_DATA) build/out/libcrunch-support.a '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch-support.a
	$(INSTALL_PROGRAM) build/out/libcrunch.so.1 '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch.so.1
	ln -sfn libcrunch.so.1 '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch.so
	$(MKDIR_P) '$(DESTDIR)$(mandir)/man1'
	$(INSTALL_DATA) build/out/crunch.1 '$(DESTDIR)$(mandir)/man1'/crunch.1
	$(INSTALL_DATA) '$(srcdir)/src/archiver/crunch-ar.1' '$(DESTDIR)$(mandir)/man1'/crunch-ar.1
	$(INSTALL_DATA) '$(srcdir)/src/linker/crunch-ld.1' '$(DESTDIR)$(mandir)/man1'/crunch-ld.1
	$(MKDIR_P) '$(DESTDIR)$(mandir)/man3'
	for page in $(crunch_library_manpages); do \
	  $(INSTALL_DATA) "build/out/$$page.3crunch" '$(DESTDIR)$(mandir)/man3'/$$page.3crunch || exit 1; \
	done
	$(MKDIR_P) '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)'
	for header in $(crunch_headers); do \
	  $(MKDIR_P) '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)'/"`dirname "$$header"`" || exit 1; \
	  $(INSTALL_DATA) '$(srcdir)'/include/$$header '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)'/$$header || exit 1; \
	done
	conf='$(DESTDIR)$(CRUNCH_INSTALL_CONFFILE)'; \
	if test -e "$$conf"; then \
	  echo "keeping existing $$conf"; \
	else \
	  $(MKDIR_P) "`dirname "$$conf"`" && \
	  $(INSTALL_DATA) build/out/crunch.conf "$$conf"; \
	fi

# The configuration file is removed only if it is still the unmodified default.
uninstall-local: build/out/crunch.conf
	conf='$(DESTDIR)$(CRUNCH_INSTALL_CONFFILE)'; \
	if test -f "$$conf"; then \
	  if cmp -s build/out/crunch.conf "$$conf"; then \
	    rm -f "$$conf"; rmdir "`dirname "$$conf"`" 2>/dev/null || true; \
	  else \
	    echo "keeping modified $$conf"; \
	  fi; \
	fi
	for tool in $(crunch_tools) crunch crunch-pp; do rm -f '$(DESTDIR)$(bindir)'/$$tool; done
	rm -f '$(DESTDIR)$(mandir)/man1'/crunch.1
	rm -f '$(DESTDIR)$(mandir)/man1'/crunch-ar.1
	rm -f '$(DESTDIR)$(mandir)/man1'/crunch-ld.1
	for page in $(crunch_library_manpages); do rm -f '$(DESTDIR)$(mandir)/man3'/$$page.3crunch; done
	rm -f '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch.a
	rm -f '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch-fallback.a
	rm -f '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch-support.a
	rm -f '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch.so '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)'/libcrunch.so.1
	-rmdir '$(DESTDIR)$(CRUNCH_INSTALL_LIBDIR)' 2>/dev/null
	for header in $(crunch_headers); do rm -f '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)'/$$header; done
	-rmdir '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)'/glibc 2>/dev/null
	-rmdir '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)'/sys 2>/dev/null
	-rmdir '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)'/netinet 2>/dev/null
	-rmdir '$(DESTDIR)$(CRUNCH_INSTALL_INCLUDEDIR)' 2>/dev/null

# Automake runs distcheck-hook before the actual distcheck work, so do not
# report success here. The nested make check prints the colored test result.


clean-local:
	$(MAKE) -C $(srcdir)/build CONFIG_MK=$(abs_builddir)/build/config.mk BUILD_DIR=$(abs_builddir)/build/out clean

distclean-local:
	rm -f build/config.mk

EXTRA_DIST = \
  include/sys/syscall_260_299.h lib/syscalls_260_299.s tests/fixtures/units/library/syscall_260_299.bc tests/fixtures/units/library/syscall_260_299_abi.c \
  man/fchownat.3crunch man/futimesat.3crunch man/linkat.3crunch man/crunch_fchmodat.3crunch man/crunch_faccessat.3crunch \
  man/crunch_pselect6.3crunch man/crunch_ppoll.3crunch man/unshare.3crunch man/set_robust_list.3crunch man/get_robust_list.3crunch \
  man/splice.3crunch man/tee.3crunch man/sync_file_range.3crunch man/vmsplice.3crunch man/move_pages.3crunch \
  man/utimensat.3crunch man/epoll_pwait.3crunch man/crunch_signalfd.3crunch man/timerfd_create.3crunch man/crunch_eventfd.3crunch \
  man/fallocate.3crunch man/timerfd_settime.3crunch man/timerfd_gettime.3crunch man/accept4.3crunch man/signalfd.3crunch \
  man/eventfd.3crunch man/epoll_create1.3crunch man/dup3.3crunch man/pipe2.3crunch man/inotify_init1.3crunch \
  man/preadv.3crunch man/pwritev.3crunch man/rt_tgsigqueueinfo.3crunch man/perf_event_open.3crunch man/recvmmsg.3crunch \
  lib/syscalls_220_259.s tests/fixtures/units/library/syscall_220_259.bc \
  man/semtimedop.3crunch man/crunch_fadvise64.3crunch man/crunch_timer_create.3crunch man/crunch_timer_settime.3crunch man/crunch_timer_gettime.3crunch \
  man/crunch_timer_getoverrun.3crunch man/crunch_timer_delete.3crunch man/clock_settime.3crunch man/clock_gettime.3crunch man/clock_getres.3crunch \
  man/crunch_clock_nanosleep.3crunch man/crunch_exit_group.3crunch man/epoll_wait.3crunch man/epoll_ctl.3crunch man/tgkill.3crunch \
  man/utimes.3crunch man/crunch_vserver.3crunch man/mbind.3crunch man/set_mempolicy.3crunch man/get_mempolicy.3crunch \
  man/crunch_mq_open.3crunch man/crunch_mq_unlink.3crunch man/crunch_mq_timedsend.3crunch man/crunch_mq_timedreceive.3crunch man/crunch_mq_notify.3crunch \
  man/crunch_mq_getsetattr.3crunch man/crunch_kexec_load.3crunch man/crunch_waitid.3crunch man/add_key.3crunch man/request_key.3crunch \
  man/crunch_keyctl.3crunch man/ioprio_set.3crunch man/ioprio_get.3crunch man/inotify_init.3crunch man/inotify_add_watch.3crunch \
  man/inotify_rm_watch.3crunch man/migrate_pages.3crunch man/mknodat.3crunch \
  lib/scanf.s lib/scan.bc tests/fixtures/units/library/stdio_scanf.bc tests/fixtures/units/library/scanf_abi.c \
  man/scanf.3crunch man/fscanf.3crunch \
  lib/freopen.s lib/putc.s lib/perror.s tests/fixtures/units/library/stdio_extra.bc tests/fixtures/units/library/sprintf_abi.c \
  man/freopen.3crunch man/putc.3crunch man/perror.3crunch man/sprintf.3crunch \
  lib/syscalls_180_219.s tests/fixtures/units/library/syscall_180_219.bc \
  include/sys/xattr.h include/sys/futex.h include/sys/aio.h include/sys/epoll.h \
  man/crunch_nfsservctl.3crunch man/crunch_getpmsg.3crunch man/crunch_putpmsg.3crunch man/crunch_afs_syscall.3crunch man/crunch_tuxcall.3crunch \
  man/crunch_security.3crunch man/gettid.3crunch man/readahead.3crunch man/setxattr.3crunch man/lsetxattr.3crunch \
  man/fsetxattr.3crunch man/getxattr.3crunch man/lgetxattr.3crunch man/fgetxattr.3crunch man/listxattr.3crunch \
  man/llistxattr.3crunch man/flistxattr.3crunch man/removexattr.3crunch man/lremovexattr.3crunch man/fremovexattr.3crunch \
  man/tkill.3crunch man/time.3crunch man/futex.3crunch man/futex_value.3crunch man/sched_setaffinity.3crunch \
  man/sched_getaffinity.3crunch man/crunch_set_thread_area.3crunch man/io_setup.3crunch man/io_destroy.3crunch man/io_getevents.3crunch \
  man/io_submit.3crunch man/io_cancel.3crunch man/crunch_get_thread_area.3crunch man/lookup_dcookie.3crunch man/epoll_create.3crunch \
  man/crunch_epoll_ctl_old.3crunch man/crunch_epoll_wait_old.3crunch man/remap_file_pages.3crunch man/set_tid_address.3crunch man/crunch_restart_syscall.3crunch \
  man/crunch_sysfs_index.3crunch man/crunch_sysfs_name.3crunch man/prctl_ptr.3crunch man/arch_prctl_ptr.3crunch man/ptrace_value.3crunch \
  lib/syscalls_140_179.s tests/fixtures/units/library/syscall_140_179.bc \
  include/sys/prctl.h include/sys/arch_prctl.h include/sys/timex.h include/sys/mount.h \
  include/sys/swap.h include/sys/reboot.h include/sys/io.h include/sys/module.h include/sys/syscall_extra.h \
  man/getpriority.3crunch man/setpriority.3crunch man/sched_setparam.3crunch man/sched_getparam.3crunch man/sched_setscheduler.3crunch \
  man/sched_getscheduler.3crunch man/sched_get_priority_max.3crunch man/sched_get_priority_min.3crunch man/sched_rr_get_interval.3crunch man/mlock.3crunch \
  man/munlock.3crunch man/mlockall.3crunch man/munlockall.3crunch man/vhangup.3crunch man/modify_ldt.3crunch \
  man/pivot_root.3crunch man/crunch_sysctl.3crunch man/prctl.3crunch man/arch_prctl.3crunch man/adjtimex.3crunch \
  man/setrlimit.3crunch man/chroot.3crunch man/sync.3crunch man/acct.3crunch man/settimeofday.3crunch \
  man/mount.3crunch man/umount2.3crunch man/swapon.3crunch man/swapoff.3crunch man/crunch_reboot.3crunch \
  man/sethostname.3crunch man/setdomainname.3crunch man/iopl.3crunch man/ioperm.3crunch man/crunch_create_module.3crunch \
  man/init_module.3crunch man/delete_module.3crunch man/crunch_get_kernel_syms.3crunch man/crunch_query_module.3crunch man/quotactl.3crunch \
  lib/syscalls_100_139.s tests/fixtures/units/library/syscall_100_139.bc \
  include/sys/times.h include/sys/ptrace.h include/sys/klog.h include/sys/fsuid.h \
  include/sys/capability.h include/utime.h include/sys/personality.h include/sys/vfs.h include/sys/legacy_syscalls.h \
  man/times.3crunch man/ptrace.3crunch man/getuid.3crunch man/klogctl.3crunch man/getgid.3crunch \
  man/setuid.3crunch man/setgid.3crunch man/geteuid.3crunch man/getegid.3crunch man/setpgid.3crunch \
  man/getppid.3crunch man/getpgrp.3crunch man/setsid.3crunch man/setreuid.3crunch man/setregid.3crunch \
  man/getgroups.3crunch man/setgroups.3crunch man/setresuid.3crunch man/getresuid.3crunch man/setresgid.3crunch \
  man/getresgid.3crunch man/getpgid.3crunch man/setfsuid.3crunch man/setfsgid.3crunch man/getsid.3crunch \
  man/capget.3crunch man/capset.3crunch man/sigpending.3crunch man/sigtimedwait.3crunch man/rt_sigqueueinfo.3crunch \
  man/sigsuspend.3crunch man/sigaltstack.3crunch man/utime.3crunch man/mknod.3crunch man/uselib.3crunch \
  man/personality.3crunch man/ustat.3crunch man/statfs.3crunch man/fstatfs.3crunch man/crunch_sysfs.3crunch \
  tests/fixtures/units/library/assert.bc \
  tests/fixtures/units/library/source_location.bc \
  include/elf.h \
  tests/fixtures/units/library/elf_header.bc \
  tests/fixtures/units/installed_toolchain/control.bc \
  tests/fixtures/units/installed_toolchain/records.bc \
  tests/fixtures/units/installed_toolchain/memory.bc \
  tests/fixtures/units/installed_toolchain/arguments.bc \
  tests/fixtures/units/installed_toolchain/stream.bc \
  tests/fixtures/units/installed_toolchain/numeric.bc \
  tests/fixtures/units/installed_toolchain/globals.bc \
  tests/fixtures/units/installed_toolchain/sorting.bc \
  tests/fixtures/units/installed_toolchain/file_io.bc \
  tests/fixtures/units/installed_toolchain/modules.bc \
  tests/fixtures/units/installed_toolchain/module_helper.bc \
  build/runtime-fallbacks.py \
  build_2gen_crunch.sh \
  include/sys/sysinfo.h lib/syscalls_80_99.s tests/fixtures/units/library/syscall_80_99.bc \
  man/chdir.3crunch man/fchdir.3crunch man/creat.3crunch man/link.3crunch man/symlink.3crunch \
  man/chown.3crunch man/fchown.3crunch man/lchown.3crunch man/umask.3crunch \
  man/gettimeofday.3crunch man/getrlimit.3crunch man/getrusage.3crunch man/sysinfo.3crunch \
  include/sys/resource.h include/sys/sem.h include/sys/msg.h include/sys/file.h include/sys/getdents.h \
  lib/syscalls_60_79.s lib/getcwd.s tests/fixtures/units/library/syscall_60_79.bc \
  man/wait4.3crunch man/semget.3crunch man/semop.3crunch man/semctl.3crunch man/semctl_value.3crunch \
  man/msgget.3crunch man/msgsnd.3crunch man/msgrcv.3crunch man/msgctl.3crunch man/fcntl.3crunch man/fcntl_ptr.3crunch \
  man/flock.3crunch man/fsync.3crunch man/fdatasync.3crunch man/truncate.3crunch man/ftruncate.3crunch \
  man/getdents.3crunch man/getcwd.3crunch \
  include/sys/sendfile.h tests/fixtures/units/library/syscall_40_59.bc \
  lib/sendfile.s lib/sendto.s lib/shutdown.s lib/getsockname.s lib/getpeername.s lib/socketpair.s \
  lib/setsockopt.s lib/getsockopt.s lib/clone.s lib/vfork.s \
  man/sendfile.3crunch man/sendto.3crunch man/shutdown.3crunch man/getsockname.3crunch \
  man/getpeername.3crunch man/socketpair.3crunch man/setsockopt.3crunch man/getsockopt.3crunch \
  man/clone.3crunch man/vfork.3crunch \
  include/sched.h include/sys/ipc.h include/sys/shm.h include/sys/time.h \
  lib/writev.s lib/sched_yield.s lib/msync.s lib/mincore.s lib/madvise.s \
  lib/shmget.s lib/shmat.s lib/shmctl.s lib/shmdt.s lib/pause.s lib/alarm.s lib/getitimer.s lib/setitimer.s \
  man/writev.3crunch man/sched_yield.3crunch man/msync.3crunch man/mincore.3crunch man/madvise.3crunch \
  man/shmget.3crunch man/shmat.3crunch man/shmctl.3crunch man/shmdt.3crunch \
  man/pause.3crunch man/alarm.3crunch man/getitimer.3crunch man/setitimer.3crunch \
  tests/fixtures/units/library/syscall_20_39.bc \
  man/mprotect.3crunch man/brk.3crunch man/sigprocmask.3crunch man/ioctl.3crunch man/ioctl_value.3crunch \
  man/pread.3crunch man/pwrite.3crunch man/readv.3crunch \
  lib/mprotect.s lib/brk.s lib/sigprocmask.s lib/ioctl.s lib/pread.s lib/pwrite.s lib/readv.s \
  include/sys/ioctl.h tests/fixtures/units/library/syscall_first20.bc \
  man/socket.3crunch man/bind.3crunch man/connect.3crunch man/listen.3crunch \
  man/accept.3crunch man/recvfrom.3crunch man/sendmsg.3crunch man/recvmsg.3crunch \
  man/sigaction.3crunch man/sigemptyset.3crunch man/nanosleep.3crunch man/sleep.3crunch \
  man/mmap.3crunch man/munmap.3crunch man/mremap.3crunch man/map_failed.3crunch \
  man/execve.3crunch man/waitpid.3crunch man/uname.3crunch man/getrandom.3crunch \
  man/chmod.3crunch man/fchmod.3crunch man/_exit.3crunch \
  man/file_open.3crunch man/file_read.3crunch man/file_write.3crunch man/file_close.3crunch \
  tests/fixtures/units/library/socket_signal_sleep.bc \
  lib/socket.s lib/connect.s lib/accept.s lib/bind.s lib/listen.s \
  lib/recvfrom.s lib/sendmsg.s lib/recvmsg.s lib/nanosleep.s lib/sleep.s \
  lib/sigaction.s lib/sigemptyset.s lib/crunch_sig_ign.s \
  include/sys/socket.h include/sys/uio.h include/sys/un.h include/time.h include/netinet/in.h \
  src/archiver/crunch-ar.c \
  src/archiver/crunch-ar.1 \
  src/archiver/Makefile \
  src/linker/crunch-ld.c \
  src/linker/relocatable.inc \
  src/linker/README.md \
  src/linker/Makefile \
  src/linker/crunch-ld.1 \
  src/linker/crt0.s \
  docs/assembler.md \
  docs/assembler-todo.md \
  include/assembler/assembler.h \
  src/assembler/assembler.c \
  src/driver/crunch_as_main.c \
  tests/fixtures/assembler/tests.py \
  tests/fixtures/units/assembly_driver/start.s \
  tests/fixtures/units/assembly_driver/helper.S \
  tests/fixtures/units/assembly_driver/base.s \
  tests/fixtures/units/assembly_driver/values.h \
  tests/unit/assembler/test_assembler.c \
  conf/crunch.conf \
  benchmarks/allocator.c \
  benchmarks/allocator.py \
  lib/syscalls/crunch_access.s \
  lib/syscalls/crunch_dup.s \
  lib/syscalls/crunch_dup2.s \
  lib/syscalls/crunch_pipe.s \
  lib/syscalls/crunch_poll.s \
  lib/syscalls/crunch_select.s \
  lib/syscalls/crunch_fork.s \
  lib/syscalls/crunch_getpid.s \
  lib/syscalls/crunch_kill.s \
  lib/syscalls/crunch_open.s \
  lib/syscalls/crunch_openat.s \
  lib/syscalls/crunch_fstatat.s \
  lib/syscalls/crunch_readlinkat.s \
  lib/syscalls/crunch_unlinkat.s \
  lib/syscalls/crunch_mkdirat.s \
  lib/syscalls/crunch_renameat.s \
  lib/syscalls/crunch_symlinkat.s \
  lib/syscalls/crunch_read.s \
  lib/syscalls/crunch_write.s \
  lib/syscalls/crunch_close.s \
  lib/syscalls/crunch_stat.s \
  lib/syscalls/crunch_fstat.s \
  lib/syscalls/crunch_lstat.s \
  lib/syscalls/crunch_readlink.s \
  lib/syscalls/crunch_getdents64.s \
  lib/syscalls/crunch_unlink.s \
  lib/syscalls/crunch_rename.s \
  lib/syscalls/crunch_mkdir.s \
  lib/syscalls/crunch_rmdir.s \
  lib/syscalls/crunch_lseek.s \
  lib/syscalls/crunch_mmap.s \
  lib/syscalls/crunch_munmap.s \
  lib/syscalls/crunch_mremap.s \
  lib/syscalls/PROTOTYPES.md \
  lib/run_initializers.s \
  lib/mmap.s \
  lib/munmap.s \
  lib/mremap.s \
  lib/map_failed.s \
  lib/malloc.s \
  lib/free.s \
  lib/realloc.s \
  lib/pool.s \
  tests/fixtures/units/library/allocator.bc \
  tests/unit/codegen/test_scan.c \
  tests/bootstrap-compiler.sh \
  tests/fixtures/units/library/strtol.bc \
  tests/fixtures/units/library/strtoul.bc \
  tests/fixtures/units/library/strtold.bc \
  tests/fixtures/units/library/binary80_format.bc \
  tests/fixtures/units/library/ctype.bc \
  tests/fixtures/units/library/memory_void_pointers.bc \
  tests/fixtures/units/control_flow/goto_inline.bc \
  tests/fixtures/units/library/strtoul_differential.c \
  tests/fixtures/units/library/stdio_open.bc \
  tests/fixtures/units/library/stdio_open_internal.c \
  tests/fixtures/units/library/stdio_io.bc \
  tests/fixtures/units/library/stdio_io_internal.c \
  tests/fixtures/units/library/stdio_chars.bc \
  tests/fixtures/units/expressions/character_escapes.bc \
  tests/fixtures/units/expressions/braced_string_and_pointer_cast.bc \
  tests/fixtures/units/expressions/wide_character.bc \
  tests/fixtures/units/library/stdio_lines.bc \
  tests/fixtures/units/library/stdio_lines_internal.c \
  tests/fixtures/units/library/stdio_fprintf.bc \
  tests/fixtures/units/library/stdio_snprintf.bc \
  tests/fixtures/units/library/allocator_blocks.bc \
  tests/fixtures/units/library/allocator_sizes.c \
  tests/fixtures/units/library/allocator_internal.c \
  tests/fixtures/units/library/allocator_pool.c \
  tests/fixtures/units/library/allocator_stress.c \
  tests/fixtures/units/library/allocator_stress.bc \
  tests/fixtures/units/library/allocator_trim.bc \
  tests/fixtures/units/library/allocator_failure.c \
  tests/fixtures/units/library/allocator_mmap_failure.s \
  tests/fixtures/units/library/allocator_remap_failure.s \
  tests/fixtures/units/library/syscall_memory.bc \
  tests/fixtures/units/library/syscall_process.bc \
  tests/fixtures/units/library/syscall_prototypes.bc \
  tests/fixtures/units/types/c_keyword_names.bc \
  tests/fixtures/units/library/syscall_wait_child.s \
  tests/fixtures/units/library/syscall_memory_file.bc \
  tests/fixtures/units/library/syscall_memory_errors.bc \
  tests/fixtures/units/library/syscall_memory_internal.c \
  tests/fixtures/units/types/void_pointer.bc \
  tests/fixtures/units/types/unsigned_runtime.bc \
  tests/fixtures/units/types/c11_numeric.bc \
  tests/fixtures/units/types/c11_numeric_boundaries.bc \
  tests/fixtures/units/types/limits.bc \
  tests/fixtures/units/types/float_header.bc \
  tests/fixtures/units/types/long_double_pointer.bc \
  tests/fixtures/units/types/binary80_indirect.bc \
  tests/fixtures/units/types/binary80_aggregate.bc \
  tests/fixtures/units/types/binary80_math.bc \
  tests/fixtures/units/types/binary80_scalar.bc \
  tests/fixtures/units/types/binary80_variadic.bc \
  tests/fixtures/units/types/array_typedef.bc \
  tests/fixtures/units/types/flexible_array.bc \
  tests/fixtures/units/types/bit_fields.bc \
  tests/fixtures/units/types/pointer_to_array.bc \
  tests/fixtures/units/types/static_assert.bc \
  tests/fixtures/units/types/volatile_access.bc \
  tests/fixtures/units/types/stdint_header.bc \
  tests/fixtures/units/function_calls/vsnprintf_source.bc \
  tests/fixtures/units/function_calls/old_style_definition.bc \
  tests/fixtures/units/function_calls/variadic_frontend.bc \
  tests/fixtures/units/function_calls/variadic_call.bc \
  tests/fixtures/units/function_calls/variadic_call.c \
  tests/fixtures/units/function_calls/variadic_frame.c \
  tests/fixtures/units/function_calls/variadic_frame.s \
  tests/fixtures/units/function_calls/variadic_start.bc \
  tests/fixtures/units/function_calls/variadic_args.bc \
  tests/fixtures/units/function_calls/variadic_aggregate.bc \
  tests/fixtures/units/function_calls/floating_struct_abi.h \
  tests/fixtures/units/function_calls/floating_struct_provider.bc \
  tests/fixtures/units/function_calls/floating_struct_consumer.bc \
  tests/fixtures/units/function_calls/bit_field_provider.bc \
  tests/fixtures/units/function_calls/bit_field_consumer.bc \
  tests/fixtures/ir/variadic_frame.bir \
  tests/fixtures/ir/binary80_abi.bir \
  tests/fixtures/units/function_calls/variadic_call.s \
  tests/fixtures/units/function_calls/invalid_variadic_signature.bc \
  tests/fixtures/units/function_calls/invalid_variadic_count.bc \
  tests/fixtures/units/parser/unsigned_frontend.bc \
  lib/end.s \
  lib/ctype.s \
  lib/errno.s \
  lib/pow.s \
  lib/input.s \
  lib/env_get.s \
  lib/memcmp.s \
  lib/memcpy.s \
  lib/memmove.s \
  lib/fflush.s \
  lib/memchr.s \
  lib/calloc.s \
  lib/getline.s \
  lib/parse_real.bc \
  lib/format_extended.bc \
  lib/printf_g17.s \
  lib/strtod.s \
  lib/strtof.s \
  lib/strtold.s \
  lib/alloc_bytes.s \
  lib/file_close.s \
  lib/file_open.s \
  lib/file_read.s \
  lib/file_write.s \
  lib/fopen.s \
  lib/fclose.s \
  lib/fseek.s \
  lib/ftell.s \
  lib/rewind.s \
  lib/fread.s \
  lib/fwrite.s \
  lib/fgetc.s \
  lib/fputc.s \
  lib/putchar.s \
  lib/fgets.s \
  lib/fputs.s \
  lib/puts.s \
  lib/standard_streams.s \
  lib/feof.s \
  lib/ferror.s \
  lib/clearerr.s \
  lib/stream_transfer.s \
  lib/open.s \
  lib/access.s \
  lib/dup.s \
  lib/dup2.s \
  lib/pipe.s \
  lib/poll.s \
  lib/select.s \
  lib/fork.s \
  lib/getpid.s \
  lib/kill.s \
  lib/openat.s \
  lib/fstatat.s \
  lib/readlinkat.s \
  lib/unlinkat.s \
  lib/mkdirat.s \
  lib/renameat.s \
  lib/symlinkat.s \
  lib/read.s \
  lib/write.s \
  lib/close.s \
  lib/stat.s \
  lib/fstat.s \
  lib/lstat.s \
  lib/readlink.s \
  lib/getdents64.s \
  lib/dirent64_next.s \
  lib/unlink.s \
  lib/rename.s \
  lib/mkdir.s \
  lib/rmdir.s \
  lib/lseek.s \
  lib/atoi.s \
  lib/atol.s \
  lib/parse_integer.s \
  lib/strtol.s \
  lib/strtoul.s \
  lib/strtoll.s \
  lib/strtoull.s \
  lib/strtoimax.s \
  lib/strtoumax.s \
  lib/free_bytes.s \
  lib/strcat.s \
  lib/strchr.s \
  lib/strrchr.s \
  lib/strerror.s \
  lib/strdup.s \
  lib/strtok.s \
  lib/strtok_r.s \
  lib/execve.s \
  lib/waitpid.s \
  lib/uname.s \
  lib/getrandom.s \
  lib/chmod.s \
  lib/fchmod.s \
  lib/fileno.s \
  lib/_exit.s \
  lib/strstr.s \
  lib/strcpy.s \
  lib/strncmp.s \
  lib/print.s \
  lib/memset.s \
  lib/printf_number.s \
  lib/printf_real.s \
  lib/printf.s \
  lib/fprintf.s \
  lib/format_write.s \
  lib/snprintf.s \
  lib/strcmp.s \
  lib/strlen.s \
  lib/tmpfile.s \
  lib/write_stdout.s \
  man/crunch.1.in \
  man/strtok.3crunch \
  man/input.3crunch \
  man/fopen.3crunch \
  man/fclose.3crunch \
  man/fseek.3crunch \
  man/ftell.3crunch \
  man/rewind.3crunch \
  man/fread.3crunch \
  man/fwrite.3crunch \
  man/fgetc.3crunch \
  man/getchar.3crunch \
  man/calloc.3crunch \
  man/getline.3crunch \
  man/strtod.3crunch \
  man/strtof.3crunch \
  man/strtold.3crunch \
  man/fputc.3crunch \
  man/putchar.3crunch \
  man/fgets.3crunch \
  man/fputs.3crunch \
  man/puts.3crunch \
  man/fprintf.3crunch \
  man/snprintf.3crunch \
  man/vsnprintf.3crunch \
  man/stdin.3crunch \
  man/stdout.3crunch \
  man/stderr.3crunch \
  man/feof.3crunch \
  man/ferror.3crunch \
  man/clearerr.3crunch \
  man/open.3crunch \
  man/access.3crunch \
  man/dup.3crunch \
  man/dup2.3crunch \
  man/pipe.3crunch \
  man/poll.3crunch \
  man/select.3crunch \
  man/fork.3crunch \
  man/getpid.3crunch \
  man/kill.3crunch \
  man/openat.3crunch \
  man/fstatat.3crunch \
  man/readlinkat.3crunch \
  man/unlinkat.3crunch \
  man/mkdirat.3crunch \
  man/renameat.3crunch \
  man/symlinkat.3crunch \
  man/read.3crunch \
  man/write.3crunch \
  man/close.3crunch \
  man/stat.3crunch \
  man/fstat.3crunch \
  man/lstat.3crunch \
  man/readlink.3crunch \
  man/getdents64.3crunch \
  man/dirent64_next.3crunch \
  man/unlink.3crunch \
  man/rename.3crunch \
  man/mkdir.3crunch \
  man/rmdir.3crunch \
  man/lseek.3crunch \
  man/malloc.3crunch \
  man/free.3crunch \
  man/realloc.3crunch \
  man/allocator_trim.3crunch \
  build/Makefile \
  build/README.md \
  build/config.mk.in \
  build/crunch-linker-config.h.in \
  docs/architecture.md \
  docs/ast-format.md \
  docs/configure.md \
  docs/errors.md \
  docs/ir.md \
  docs/language-spec.md \
  docs/lexer-format.md \
  docs/sema-symbols.md \
  docs/sema-types.md \
  docs/testing.md \
  include/FILES.md \
  src/FILES.md \
  tests/integration/test_ast_json.py \
  tests/integration/test_configure.sh \
  tests/integration/test_lexer_filter.sh \
  tests/integration/test_parser_fixtures.sh \
  tests/integration/test_sema.sh \
  tests/integration/test_crunch_ir.sh \
  tests/run-tests.sh \
  tests/run-tests.py \
  tests/gnu_header_inventory.py \
  tests/gnu_header_compare.py \
  tests/check_function_docs.py \
  tests/unit/input/test_input_errors.c \
  tests/unit/sema/test_vsnprintf_abi.c \
  include/codegen/codegen.h \
  include/codegen/x86_64.h \
  include/string.h \
  include/assert.h \
  include/stdio.h \
  include/stdarg.h \
  include/glibc/alloca.h \
  include/__crunch_va_list.h \
  include/stdlib.h \
  include/stddef.h \
  include/stdalign.h \
  include/stdint.h \
  include/inttypes.h \
  include/math.h \
  include/float.h \
  include/ctype.h \
  include/errno.h \
  include/limits.h \
  include/stdbool.h \
  include/unistd.h \
  include/fcntl.h \
  include/poll.h \
  include/signal.h \
  include/dirent.h \
  include/sys/types.h \
  include/sys/utsname.h \
  include/sys/wait.h \
  include/sys/random.h \
  include/sys/stat.h \
  include/sys/select.h \
  include/sys/mman.h \
  include/environment.h \
  include/test/guarded.h \
  include/test/outer.h \
  include/test/guarded_cycle_a.h \
  include/test/guarded_cycle_b.h \
  include/test/cycle_a.h \
  include/test/cycle_b.h \
  include/test/location_error.h \
  include/test/branch.h \
  include/common/arena.h \
  include/common/compiler.h \
  include/common/constant_expr.h \
  include/common/diagnostic.h \
  include/common/identifier.h \
  include/common/runtime_declaration.h \
  include/common/runtime_routines.h \
  include/common/source.h \
  include/config.h \
  include/version.h \
  include/driver/driver.h \
  include/driver/options.h \
  include/ir/ir.h \
  include/optimizer/optimizer.h \
  include/lexer/lexer.h \
  include/lexer/token.h \
  include/parser/ast.h \
  include/parser/ast_json.h \
  include/parser/parser.h \
  include/sema/sema.h \
  include/sema/symbol.h \
  src/FILES.md \
  src/codegen/x86_64.c \
  src/common/arena.c \
  src/common/constant_expr.c \
  src/common/diagnostic.c \
  src/common/source.c \
  src/driver/driver.c \
  src/driver/lexer_main.c \
  src/driver/pp_main.c \
  src/driver/crunch_main.c \
  src/driver/ir_main.c \
  src/driver/opt_main.c \
  src/driver/asm_main.c \
  src/driver/options.c \
  src/driver/parser_main.c \
  src/ir/ir.c \
  src/optimizer/optimizer.c \
  src/lexer/lexer.c \
  src/lexer/token.c \
  src/main.c \
  src/parser/ast.c \
  src/parser/ast_json.c \
  src/parser/parser.c \
  src/sema/sema.c \
  src/sema/symbol.c \
  tests/fixtures/units/ir/stack_underflow.bir \
  tests/fixtures/units/ir/stack_merge_mismatch.bir \
  tests/fixtures/units/ir/reachable_fallthrough.bir \
  tests/fixtures/units/ir/conditional_branch_underflow.bir \
  tests/fixtures/units/ir/stack_type_mismatch.bir \
  tests/fixtures/units/ir/stack_type_merge_mismatch.bir \
  tests/fixtures/units/structures/local_global_members.bc \
  tests/fixtures/units/structures/local_contiguous_layout.bc \
  tests/fixtures/units/structures/nested_members_and_addresses.bc \
  tests/fixtures/units/structures/whole_structure_assignment.bc \
  tests/fixtures/units/structures/whole_structure_array_descriptor_copy.bc \
  tests/fixtures/units/structures/nested_dynamic_array_field.bc \
  tests/fixtures/units/structures/dynamic_array_fields.bc \
  tests/fixtures/units/structures/array_field_initializers.bc \
  tests/fixtures/units/structures/global_layout.bc \
  tests/fixtures/units/structures/def_named_struct.bc \
  tests/fixtures/units/structures/def_anonymous_struct.bc \
  tests/fixtures/units/structures/invalid_unnamed_struct_assignment.bc \
  tests/fixtures/units/structures/invalid_anonymous_member_collision.bc \
  tests/fixtures/units/globals/static_groups.bc \
  tests/fixtures/units/dynamic_arrays/failing_allocator.s \
  tests/fixtures/ir/legacy_array_literal_init.bir \
  tests/fixtures/ir/legacy_array_resize.bir \
  tests/fixtures/ir/legacy_array_resize_edges.bir \
  tests/fixtures/ir/legacy_byte_pointer_initializer.bir \
  tests/fixtures/ir/legacy_byte_pointer_initializer_failure.bir \
  tests/fixtures/ir/legacy_byte_pointer_initializer_failure_module.bir \
  tests/fixtures/ir/legacy_byte_pointer_initializer_module.bir \
  tests/fixtures/ir/legacy_const_array_release.bir \
  tests/fixtures/ir/legacy_cross_object_static_0.bir \
  tests/fixtures/ir/legacy_cross_object_static_1.bir \
  tests/fixtures/ir/legacy_descriptor_protection.bir \
  tests/fixtures/ir/legacy_descriptor_protection_module.bir \
  tests/fixtures/ir/legacy_dim_expression_0.bir \
  tests/fixtures/ir/legacy_dim_expression_1.bir \
  tests/fixtures/ir/legacy_dim_expression_2.bir \
  tests/fixtures/ir/legacy_dim_expression_3.bir \
  tests/fixtures/ir/legacy_dim_expression_4.bir \
  tests/fixtures/ir/legacy_global_array.bir \
  tests/fixtures/ir/legacy_global_array_init.bir \
  tests/fixtures/ir/legacy_global_byte_pointer_initializer.bir \
  tests/fixtures/ir/legacy_global_byte_pointer_initializer_module.bir \
  tests/fixtures/ir/legacy_global_decl_array_init.bir \
  tests/fixtures/ir/legacy_global_initializer_allocation_failure.bir \
  tests/fixtures/ir/legacy_global_initializer_allocation_failure_module.bir \
  tests/fixtures/ir/legacy_global_static_no_resize.bir \
  tests/fixtures/ir/legacy_global_static_string.bir \
  tests/fixtures/ir/legacy_global_string_array_copy.bir \
  tests/fixtures/ir/legacy_indirect_array_field_failure.bir \
  tests/fixtures/ir/legacy_indirect_array_field_failure_module.bir \
  tests/fixtures/ir/legacy_indirect_array_fields.bir \
  tests/fixtures/ir/legacy_indirect_array_fields_module.bir \
  tests/fixtures/ir/legacy_init_first.bir \
  tests/fixtures/ir/legacy_init_main.bir \
  tests/fixtures/ir/legacy_init_second.bir \
  tests/fixtures/ir/legacy_initializer_allocation_failure.bir \
  tests/fixtures/ir/legacy_initializer_allocation_failure_module.bir \
  tests/fixtures/ir/legacy_initializer_executable.bir \
  tests/fixtures/ir/legacy_initializer_executable_module.bir \
  tests/fixtures/ir/legacy_initializer_failure.bir \
  tests/fixtures/ir/legacy_initializer_failure_module.bir \
  tests/fixtures/ir/legacy_initializer_first.bir \
  tests/fixtures/ir/legacy_initializer_library.bir \
  tests/fixtures/ir/legacy_initializer_main.bir \
  tests/fixtures/ir/legacy_initializer_mixed.bir \
  tests/fixtures/ir/legacy_initializer_other.bir \
  tests/fixtures/ir/legacy_initializer_second.bir \
  tests/fixtures/ir/legacy_local_array_resize.bir \
  tests/fixtures/ir/legacy_local_array_shrink.bir \
  tests/fixtures/ir/legacy_local_int_array.bir \
  tests/fixtures/ir/legacy_pointer_array_failure.bir \
  tests/fixtures/ir/legacy_pointer_array_failure_module.bir \
  tests/fixtures/ir/legacy_pointer_arrays.bir \
  tests/fixtures/ir/legacy_pointer_arrays_module.bir \
  tests/fixtures/ir/legacy_real_array_resize.bir \
  tests/fixtures/ir/legacy_resize_preserves_string.bir \
  tests/fixtures/ir/legacy_sized_declaration.bir \
  tests/fixtures/ir/legacy_string_array_copy.bir \
  tests/fixtures/ir/legacy_string_init.bir \
  tests/fixtures/ir/legacy_typed_array_copy.bir \
  tests/fixtures/ir/legacy_unsigned_array_boundaries.bir \
  tests/fixtures/ir/legacy_unsigned_arrays.bir \
  tests/fixtures/ir/legacy_unsigned_arrays_module.bir \
  tests/fixtures/units/parser/mixed_declarations.bc \
  tests/fixtures/units/types/fixed_array_fields.bc \
  tests/fixtures/units/types/fixed_arrays.bc \
  tests/fixtures/units/types/fixed_unsigned_arrays.bc \
  tests/fixtures/units/types/global_fixed_arrays.bc \
  tests/fixtures/units/types/invalid_fixed_arrays.bc \
  tests/fixtures/units/types/local_fixed_arrays.bc \
  tests/fixtures/units/types/constant_conversion_warnings.bc \
  tests/fixtures/units/types/static_local_arrays.bc \
  tests/fixtures/units/types/static_local_scalars.bc \
  tests/fixtures/units/types/short_types.bc \
  tests/fixtures/ir/main42.bc \
  tests/fixtures/ir/control_flow.bc \
  tests/fixtures/ir/comparisons.bc \
  tests/fixtures/ir/loop_condition.bc \
  tests/fixtures/ir/function_calls.bc \
  tests/fixtures/ir/numeric_types.bc \
  tests/fixtures/ir/pointers.bc \
  tests/fixtures/parser/argv.bc \
  tests/fixtures/parser/arithmetic.bc \
  tests/fixtures/parser/arrays.bc \
  tests/fixtures/parser/hello.bc \
  tests/fixtures/parser/invalid_missing_semicolon.bc \
  tests/fixtures/parser/loops.bc \
  tests/fixtures/parser/structures.bc \
  tests/fixtures/sema/casts_valid.bc \
  tests/fixtures/sema/conflicting_function.bc \
  tests/fixtures/sema/duplicate_definition.bc \
  tests/fixtures/sema/duplicate_global.bc \
  tests/fixtures/sema/duplicate_local.bc \
  tests/fixtures/sema/duplicate_parameter.bc \
  tests/fixtures/sema/duplicate_struct_field.bc \
  tests/fixtures/sema/duplicate_structure.bc \
  tests/fixtures/sema/numeric_bad_assignment.bc \
  tests/fixtures/sema/numeric_bad_byte_cast_constant.bc \
  tests/fixtures/sema/numeric_bad_byte_initializer.bc \
  tests/fixtures/sema/numeric_bad_int_cast_constant.bc \
  tests/fixtures/sema/numeric_bad_int_initializer.bc \
  tests/fixtures/sema/numeric_bad_long_literal.bc \
  tests/fixtures/sema/numeric_bounds_valid.bc \
  tests/fixtures/sema/scopes_valid.bc \
  tests/fixtures/sema/type_bad_argument.bc \
  tests/fixtures/sema/type_bad_array_init.bc \
  tests/fixtures/sema/type_bad_assignment.bc \
  tests/fixtures/sema/type_bad_byte_real_cast.bc \
  tests/fixtures/sema/type_bad_call.bc \
  tests/fixtures/sema/type_bad_member.bc \
  tests/fixtures/sema/type_bad_operator.bc \
  tests/fixtures/sema/type_bad_real_byte_cast.bc \
  tests/fixtures/sema/type_bad_return.bc \
  tests/fixtures/sema/type_bad_void_return.bc \
  tests/fixtures/sema/types_valid.bc \
  tests/fixtures/sema/undefined_function.bc \
  tests/fixtures/sema/undefined_name.bc \
  tests/unit/lexer/test_lexer.c \
  tests/unit/codegen/test_allocation.c \
  tests/unit/preprocessor/test_allocation.c \
  tests/unit/parser/test_allocation.c \
  tests/fixtures/units/control_flow/loop_and_break.bc \
  tests/fixtures/units/control_flow/short_circuit.bc \
  tests/fixtures/units/expressions/invalid_byte_real_cast.bc \
  tests/fixtures/units/expressions/invalid_power_argument.bc \
  tests/fixtures/units/expressions/invalid_power_narrowing.bc \
  tests/fixtures/units/expressions/invalid_power_pointer.bc \
  tests/fixtures/units/expressions/invalid_operator_types.bc \
  tests/fixtures/units/expressions/power_helper.c \
  tests/fixtures/units/expressions/power_boundaries.bc \
  tests/fixtures/units/expressions/power_runtime.bc \
  tests/fixtures/units/expressions/precedence_runtime.bc \
  tests/fixtures/units/types/sizeof.bc \
  tests/fixtures/units/types/alignof.bc \
  tests/fixtures/units/types/alignas.bc \
  tests/fixtures/units/types/local_vla.bc \
  tests/fixtures/units/types/alignas_helper.s \
  tests/fixtures/units/types/alignas_abi.bc \
  tests/fixtures/units/types/alignas_abi_helper.c \
  tests/fixtures/units/types/array_compound_literal.bc \
  tests/fixtures/units/types/scalar_compound_literal.bc \
  tests/fixtures/units/types/file_compound_literal.bc \
  tests/fixtures/units/expressions/valid_numeric_expressions.bc \
  tests/fixtures/units/function_calls/argument_order.bc \
  tests/fixtures/units/function_calls/arguments_and_return.bc \
  tests/fixtures/units/function_calls/end_header_literal.bc \
  tests/fixtures/units/function_calls/end_header_variable.bc \
  tests/fixtures/units/function_calls/invalid_argument_count.bc \
  tests/fixtures/units/function_calls/invalid_return_type.bc \
  tests/fixtures/units/function_calls/nested_calls_stack.bc \
  tests/fixtures/units/globals/global_and_local_names.bc \
  tests/fixtures/units/globals/global_runtime.bc \
  tests/fixtures/units/globals/invalid_duplicate_global.bc \
  tests/fixtures/units/globals/invalid_external_alias_conflict.bc \
  tests/fixtures/units/ir/invalid_header.bir \
  tests/fixtures/units/ir/unknown_function_call.bir \
  tests/fixtures/units/ir/undefined_branch_target.bir \
  tests/fixtures/units/ir/duplicate_label.bir \
  tests/fixtures/units/ir/duplicate_function.bir \
  tests/fixtures/units/ir/branch_into_other_function.bir \
  tests/fixtures/units/ir/invalid_label_spelling.bir \
  tests/fixtures/units/ir/unknown_instruction.bir \
  tests/fixtures/units/lexer/invalid_character.bc \
  tests/fixtures/units/lexer/leading_dot_real.bc \
  tests/fixtures/units/lexer/literals_and_operators.bc \
  tests/fixtures/units/parser/expression_precedence.bc \
  tests/fixtures/units/parser/function_prototype.bc \
  tests/fixtures/units/parser/nested_blocks.bc \
  tests/fixtures/units/pointers/address_and_indirect_store.bc \
  tests/fixtures/units/pointers/address_index.bc \
  tests/fixtures/units/pointers/invalid_pointer_type_mismatch.bc \
  tests/fixtures/units/pointers/pointer_aliasing.bc \
  tests/fixtures/units/pointers/struct_return.bc \
  tests/fixtures/units/pointers/struct_arithmetic.bc \
  tests/fixtures/units/pointers/pointer_field_arithmetic.bc \
  tests/fixtures/units/pointers/struct_arrow.bc \
  tests/fixtures/units/pointers/struct_pointer_fields.bc \
  tests/fixtures/units/pointers/pointer_to_pointer.bc \
  tests/fixtures/units/scopes/invalid_same_scope_duplicate.bc \
  tests/fixtures/units/scopes/nested_shadowing.bc \
  tests/fixtures/units/types/invalid_implicit_narrowing.bc \
  tests/fixtures/units/types/invalid_alias_const_pointer.bc \
  tests/fixtures/units/library/syscall_file.bc \
  tests/fixtures/units/library/syscall_directory.bc \
  tests/fixtures/units/library/syscall_at.bc \
  tests/fixtures/units/library/syscall_directory_read.bc \
  tests/fixtures/units/library/dirent64_decode.c \
  tests/fixtures/units/library/syscall_raw.c \
  tests/fixtures/units/types/valid_numeric_widening.bc \
  tests/fixtures/projects/headers/Makefile \
  tests/fixtures/projects/alias_power_globals/Makefile \
  tests/fixtures/projects/alias_power_globals/include/shared.h \
  tests/fixtures/projects/alias_power_globals/main.c \
  tests/fixtures/projects/alias_power_globals/values.c \
  tests/fixtures/projects/alias_power_globals/update.c \
  tests/fixtures/projects/headers/include/calculate.h \
  tests/fixtures/projects/headers/include/adjust.h \
  tests/fixtures/projects/headers/main.c \
  tests/fixtures/projects/headers/calculate.c \
  tests/fixtures/projects/headers/adjust.c \
  tests/fixtures/projects/global_data/Makefile \
  tests/fixtures/projects/global_data/include/project.h \
  tests/fixtures/projects/global_data/main.c \
  tests/fixtures/projects/global_data/message.c \
  tests/fixtures/projects/global_data/state.c \
  tests/fixtures/projects/abi/Makefile \
  tests/fixtures/projects/abi/include/calculate.h \
  tests/fixtures/projects/abi/include/custom.h \
  tests/fixtures/projects/abi/main.c \
  tests/fixtures/projects/abi/calculate.c \
  tests/fixtures/projects/abi/custom.c \
  tests/fixtures/projects/arrays/Makefile \
  tests/fixtures/projects/arrays/include/data.h \
  tests/fixtures/projects/arrays/main.c \
  tests/fixtures/projects/arrays/data.c \
  tests/fixtures/projects/arrays/resize.c \
  tests/fixtures/projects/external_arrays/Makefile \
  tests/fixtures/projects/external_arrays/include/data.h \
  tests/fixtures/projects/external_arrays/main.c \
  tests/fixtures/projects/external_arrays/data.c \
  tests/fixtures/projects/external_arrays/resize.c \
  tests/fixtures/projects/input/Makefile \
  tests/fixtures/projects/input/include/io.h \
  tests/fixtures/projects/input/main.c \
  tests/fixtures/projects/input/interact.c \
  tests/fixtures/projects/input/greeting.c \
  tests/fixtures/projects/allocated_reader/Makefile \
  tests/fixtures/projects/allocated_reader/include/reader.h \
  tests/fixtures/projects/allocated_reader/main.c \
  tests/fixtures/projects/allocated_reader/reader.c \
  tests/fixtures/projects/allocated_reader/failure.c \
  tests/fixtures/projects/linked_list/Makefile \
  tests/fixtures/projects/linked_list/include/list.h \
  tests/fixtures/projects/linked_list/main.c \
  tests/fixtures/projects/linked_list/list.c \
  tests/fixtures/projects/linked_list/failure.c \
  tests/fixtures/projects/invalid/include/api.h \
  tests/fixtures/projects/invalid/include/conflicting.h \
  tests/fixtures/projects/invalid/main.c \
  tests/fixtures/projects/invalid/first.c \
  tests/fixtures/projects/invalid/second.c \
  tests/fixtures/projects/invalid/conflict.c \
  tests/unit/sema/test_sema.c \
  m4/README.md
