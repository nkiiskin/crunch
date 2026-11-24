#ifndef CRUNCH_COMMON_RUNTIME_ROUTINES_H
#define CRUNCH_COMMON_RUNTIME_ROUTINES_H

#include <string.h>

/* Argument classes describe the public prototypes: p is a pointer, i an integer. */
#define CRUNCH_CTYPE_ROUTINES(X) \
    X("isalnum", 1, 'i', "i") X("isalpha", 1, 'i', "i") X("isblank", 1, 'i', "i") \
    X("iscntrl", 1, 'i', "i") X("isdigit", 1, 'i', "i") X("isgraph", 1, 'i', "i") \
    X("islower", 1, 'i', "i") X("isprint", 1, 'i', "i") X("ispunct", 1, 'i', "i") \
    X("isspace", 1, 'i', "i") X("isupper", 1, 'i', "i") X("isxdigit", 1, 'i', "i") \
    X("tolower", 1, 'i', "i") X("toupper", 1, 'i', "i")

#define CRUNCH_RUNTIME_ROUTINES(X) \
    CRUNCH_CTYPE_ROUTINES(X) \
    X("memcpy", 3, 'p', "ppi") X("memmove", 3, 'p', "ppi") X("memset", 3, 'p', "pii") X("memcmp", 3, 'i', "ppi") X("memchr", 3, 'p', "pii") \
    X("strcmp", 2, 'i', "pp") X("strncmp", 3, 'i', "ppi") X("strlen", 1, 'l', "p") \
    X("strcpy", 2, 'p', "pp") X("strcat", 2, 'p', "pp") X("strchr", 2, 'p', "pi") X("strrchr", 2, 'p', "pi") \
    X("strstr", 2, 'p', "pp") X("strerror", 1, 'p', "i") X("strdup", 1, 'p', "p") X("strtok", 2, 'p', "pp") X("strtok_r", 3, 'p', "ppp") \
    X("env_get", 2, 'p', "pp") X("atol", 1, 'l', "p") X("atoi", 1, 'i', "p") \
    X("strtol", 3, 'l', "ppi") X("strtoul", 3, 'U', "ppi") X("strtoll", 3, 'l', "ppi") \
    X("strtoull", 3, 'U', "ppi") X("strtoimax", 3, 'l', "ppi") X("strtoumax", 3, 'U', "ppi") \
    X("strtod", 2, 'r', "pp") X("strtof", 2, 'f', "pp") X("strtold", 2, 'e', "pp") \
    X("exit", 1, 'n', "i") X("file_open", 2, 'l', "pi") \
    X("file_read", 3, 'l', "ipi") X("file_write", 3, 'l', "ipi") X("file_close", 1, 'l', "i") \
    X("open", 3, 'i', "pii") X("read", 3, 'l', "ipi") X("write", 3, 'l', "ipi") X("close", 1, 'i', "i") \
    X("access", 2, 'i', "pi") X("dup", 1, 'i', "i") X("dup2", 2, 'i', "ii") X("pipe", 1, 'i', "p") \
    X("poll", 3, 'i', "pii") X("select", 5, 'i', "ipppp") \
    X("fork", 0, 'i', "") X("getpid", 0, 'i', "") X("kill", 2, 'i', "ii") \
    X("execve", 3, 'i', "ppp") X("waitpid", 3, 'i', "ipi") X("uname", 1, 'i', "p") \
    X("getrandom", 3, 'l', "pii") X("chmod", 2, 'i', "pi") X("fchmod", 2, 'i', "ii") X("_exit", 1, 'n', "i") \
    X("socket", 3, 'i', "iii") X("bind", 3, 'i', "ipi") X("connect", 3, 'i', "ipi") \
    X("listen", 2, 'i', "ii") X("accept", 3, 'i', "ipp") X("recvfrom", 6, 'l', "ipiipp") \
    X("sendmsg", 3, 'l', "ipi") X("recvmsg", 3, 'l', "ipi") \
    X("sigaction", 3, 'i', "ipp") X("sigemptyset", 1, 'i', "p") X("crunch_sig_ign", 0, 'p', "") \
    X("nanosleep", 2, 'i', "pp") X("sleep", 1, 'u', "i") \
    X("mprotect", 3, 'i', "pii") X("brk", 1, 'i', "p") X("sigprocmask", 3, 'i', "ipp") \
    X("ioctl", 3, 'i', "iip") X("ioctl_value", 3, 'i', "iii") X("pread", 4, 'l', "ipii") X("pwrite", 4, 'l', "ipii") X("readv", 3, 'l', "ipi") \
    X("writev", 3, 'l', "ipi") X("sched_yield", 0, 'i', "") X("msync", 3, 'i', "pii") X("mincore", 3, 'i', "pip") X("madvise", 3, 'i', "pii") \
    X("shmget", 3, 'i', "iii") X("shmat", 3, 'p', "ipi") X("shmctl", 3, 'i', "iip") X("shmdt", 1, 'i', "p") \
    X("pause", 0, 'i', "") X("getitimer", 2, 'i', "ip") X("alarm", 1, 'u', "i") X("setitimer", 3, 'i', "ipp") \
    X("sendfile", 4, 'l', "iipi") X("sendto", 6, 'l', "ipiipi") X("shutdown", 2, 'i', "ii") \
    X("getsockname", 3, 'i', "ipp") X("getpeername", 3, 'i', "ipp") X("socketpair", 4, 'i', "iiip") \
    X("setsockopt", 5, 'i', "iiipi") X("getsockopt", 5, 'i', "iiipp") X("clone", 4, 'i', "ppip") X("vfork", 0, 'i', "") \
    X("wait4", 4, 'i', "ipip") X("semget", 3, 'i', "iii") X("semop", 3, 'i', "ipi") X("semctl", 4, 'i', "iiip") X("semctl_value", 4, 'i', "iiii") \
    X("msgget", 2, 'i', "ii") X("msgsnd", 4, 'i', "ipii") X("msgrcv", 5, 'l', "ipiii") X("msgctl", 3, 'i', "iip") \
    X("fcntl", 3, 'i', "iii") X("fcntl_ptr", 3, 'i', "iip") X("flock", 2, 'i', "ii") X("fsync", 1, 'i', "i") X("fdatasync", 1, 'i', "i") \
    X("truncate", 2, 'i', "pi") X("ftruncate", 2, 'i', "ii") X("getdents", 3, 'l', "ipi") X("getcwd", 2, 'p', "pi") \
    X("chdir", 1, 'i', "p") X("fchdir", 1, 'i', "i") X("creat", 2, 'i', "pi") X("link", 2, 'i', "pp") X("symlink", 2, 'i', "pp") \
    X("chown", 3, 'i', "pii") X("fchown", 3, 'i', "iii") X("lchown", 3, 'i', "pii") X("umask", 1, 'u', "i") \
    X("gettimeofday", 2, 'i', "pp") X("getrlimit", 2, 'i', "ip") X("getrusage", 2, 'i', "ip") X("sysinfo", 1, 'i', "p") \
    X("times", 1, 'l', "p") X("ptrace", 4, 'l', "iipp") X("ptrace_value", 4, 'l', "iipi") X("getuid", 0, 'u', "") \
    X("klogctl", 3, 'i', "ipi") X("getgid", 0, 'u', "") X("setuid", 1, 'i', "i") \
    X("setgid", 1, 'i', "i") X("geteuid", 0, 'u', "") X("getegid", 0, 'u', "") \
    X("setpgid", 2, 'i', "ii") X("getppid", 0, 'i', "") X("getpgrp", 0, 'i', "") \
    X("setsid", 0, 'i', "") X("setreuid", 2, 'i', "ii") X("setregid", 2, 'i', "ii") \
    X("getgroups", 2, 'i', "ip") X("setgroups", 2, 'i', "ip") X("setresuid", 3, 'i', "iii") \
    X("getresuid", 3, 'i', "ppp") X("setresgid", 3, 'i', "iii") X("getresgid", 3, 'i', "ppp") \
    X("getpgid", 1, 'i', "i") X("setfsuid", 1, 'u', "i") X("setfsgid", 1, 'u', "i") \
    X("getsid", 1, 'i', "i") X("capget", 2, 'i', "pp") X("capset", 2, 'i', "pp") \
    X("sigpending", 1, 'i', "p") X("sigtimedwait", 3, 'i', "ppp") X("rt_sigqueueinfo", 3, 'i', "iip") \
    X("sigsuspend", 1, 'i', "p") X("sigaltstack", 2, 'i', "pp") X("utime", 2, 'i', "pp") \
    X("mknod", 3, 'i', "pii") X("uselib", 1, 'i', "p") X("personality", 1, 'i', "i") \
    X("ustat", 2, 'i', "ip") X("statfs", 2, 'i', "pp") X("fstatfs", 2, 'i', "ip") \
    X("crunch_sysfs", 3, 'l', "iii") X("crunch_sysfs_index", 1, 'i', "p") X("crunch_sysfs_name", 2, 'i', "ip") \
    X("getpriority", 2, 'i', "ii") X("setpriority", 3, 'i', "iii") X("sched_setparam", 2, 'i', "ip") \
    X("sched_getparam", 2, 'i', "ip") X("sched_setscheduler", 3, 'i', "iip") X("sched_getscheduler", 1, 'i', "i") \
    X("sched_get_priority_max", 1, 'i', "i") X("sched_get_priority_min", 1, 'i', "i") X("sched_rr_get_interval", 2, 'i', "ip") \
    X("mlock", 2, 'i', "pi") X("munlock", 2, 'i', "pi") X("mlockall", 1, 'i', "i") \
    X("munlockall", 0, 'i', "") X("vhangup", 0, 'i', "") X("modify_ldt", 3, 'i', "ipi") \
    X("pivot_root", 2, 'i', "pp") X("crunch_sysctl", 1, 'i', "p") X("prctl", 5, 'i', "iiiii") X("prctl_ptr", 5, 'i', "ipiii") \
    X("arch_prctl", 2, 'i', "ii") X("arch_prctl_ptr", 2, 'i', "ip") X("adjtimex", 1, 'i', "p") X("setrlimit", 2, 'i', "ip") \
    X("chroot", 1, 'i', "p") X("sync", 0, 'c', "") X("acct", 1, 'i', "p") \
    X("settimeofday", 2, 'i', "pp") X("mount", 5, 'i', "pppip") X("umount2", 2, 'i', "pi") \
    X("swapon", 2, 'i', "pi") X("swapoff", 1, 'i', "p") X("crunch_reboot", 2, 'i', "ip") \
    X("sethostname", 2, 'i', "pi") X("setdomainname", 2, 'i', "pi") X("iopl", 1, 'i', "i") \
    X("ioperm", 3, 'i', "iii") X("crunch_create_module", 2, 'l', "pi") X("init_module", 3, 'i', "pip") \
    X("delete_module", 2, 'i', "pi") X("crunch_get_kernel_syms", 1, 'i', "p") X("crunch_query_module", 5, 'i', "pipip") \
    X("quotactl", 4, 'i', "ipip") \
    X("crunch_nfsservctl", 0, 'l', "") X("crunch_getpmsg", 0, 'l', "") X("crunch_putpmsg", 0, 'l', "") \
    X("crunch_afs_syscall", 0, 'l', "") X("crunch_tuxcall", 0, 'l', "") X("crunch_security", 0, 'l', "") \
    X("gettid", 0, 'i', "") X("readahead", 3, 'l', "iii") X("setxattr", 5, 'i', "pppii") \
    X("lsetxattr", 5, 'i', "pppii") X("fsetxattr", 5, 'i', "ippii") X("getxattr", 4, 'l', "pppi") \
    X("lgetxattr", 4, 'l', "pppi") X("fgetxattr", 4, 'l', "ippi") X("listxattr", 3, 'l', "ppi") \
    X("llistxattr", 3, 'l', "ppi") X("flistxattr", 3, 'l', "ipi") X("removexattr", 2, 'i', "pp") \
    X("lremovexattr", 2, 'i', "pp") X("fremovexattr", 2, 'i', "ip") X("tkill", 2, 'i', "ii") \
    X("time", 1, 'l', "p") X("futex", 6, 'i', "piippi") X("futex_value", 6, 'i', "piiipi") \
    X("sched_setaffinity", 3, 'i', "iip") X("sched_getaffinity", 3, 'i', "iip") X("crunch_set_thread_area", 1, 'i', "p") \
    X("io_setup", 2, 'i', "ip") X("io_destroy", 1, 'i', "i") X("io_getevents", 5, 'l', "iiipp") \
    X("io_submit", 3, 'l', "iip") X("io_cancel", 3, 'i', "ipp") X("crunch_get_thread_area", 1, 'i', "p") \
    X("lookup_dcookie", 3, 'l', "ipi") X("epoll_create", 1, 'i', "i") X("crunch_epoll_ctl_old", 4, 'i', "iiip") \
    X("crunch_epoll_wait_old", 4, 'i', "ipii") X("remap_file_pages", 5, 'i', "piiii") X("set_tid_address", 1, 'i', "p") \
    X("crunch_restart_syscall", 0, 'l', "") \
    X("semtimedop", 4, 'i', "ipip") \
    X("crunch_fadvise64", 4, 'i', "iiii") \
    X("crunch_timer_create", 3, 'i', "ipp") \
    X("crunch_timer_settime", 4, 'i', "iipp") \
    X("crunch_timer_gettime", 2, 'i', "ip") \
    X("crunch_timer_getoverrun", 1, 'i', "i") \
    X("crunch_timer_delete", 1, 'i', "i") \
    X("clock_settime", 2, 'i', "ip") \
    X("clock_gettime", 2, 'i', "ip") \
    X("clock_getres", 2, 'i', "ip") \
    X("crunch_clock_nanosleep", 4, 'i', "iipp") \
    X("crunch_exit_group", 1, 'n', "i") \
    X("epoll_wait", 4, 'i', "ipii") \
    X("epoll_ctl", 4, 'i', "iiip") \
    X("tgkill", 3, 'i', "iii") \
    X("utimes", 2, 'i', "pp") \
    X("crunch_vserver", 0, 'l', "") \
    X("mbind", 6, 'l', "piipii") \
    X("set_mempolicy", 3, 'l', "ipi") \
    X("get_mempolicy", 5, 'l', "ppipi") \
    X("crunch_mq_open", 4, 'i', "piip") \
    X("crunch_mq_unlink", 1, 'i', "p") \
    X("crunch_mq_timedsend", 5, 'i', "ipiip") \
    X("crunch_mq_timedreceive", 5, 'l', "ipipp") \
    X("crunch_mq_notify", 2, 'i', "ip") \
    X("crunch_mq_getsetattr", 3, 'i', "ipp") \
    X("crunch_kexec_load", 4, 'l', "iipi") \
    X("crunch_waitid", 5, 'i', "iipip") \
    X("add_key", 5, 'l', "pppii") \
    X("request_key", 4, 'l', "pppi") \
    X("crunch_keyctl", 5, 'l', "iiiii") \
    X("ioprio_set", 3, 'i', "iii") \
    X("ioprio_get", 2, 'i', "ii") \
    X("inotify_init", 0, 'i', "") \
    X("inotify_add_watch", 3, 'i', "ipi") \
    X("inotify_rm_watch", 2, 'i', "ii") \
    X("migrate_pages", 4, 'l', "iipp") \
    X("mknodat", 4, 'i', "ipii") \
    X("fchownat", 5, 'i', "ipiii") \
    X("futimesat", 3, 'i', "ipp") \
    X("linkat", 5, 'i', "ipipi") \
    X("crunch_fchmodat", 3, 'i', "ipi") \
    X("crunch_faccessat", 3, 'i', "ipi") \
    X("crunch_pselect6", 6, 'i', "ippppp") \
    X("crunch_ppoll", 4, 'i', "pipp") \
    X("unshare", 1, 'i', "i") \
    X("set_robust_list", 2, 'i', "pi") \
    X("get_robust_list", 3, 'i', "ipp") \
    X("splice", 6, 'l', "ipipii") \
    X("tee", 4, 'l', "iiii") \
    X("sync_file_range", 4, 'i', "iiii") \
    X("vmsplice", 4, 'l', "ipii") \
    X("move_pages", 6, 'l', "iipppi") \
    X("utimensat", 4, 'i', "ippi") \
    X("epoll_pwait", 5, 'i', "ipiip") \
    X("crunch_signalfd", 2, 'i', "ip") \
    X("timerfd_create", 2, 'i', "ii") \
    X("crunch_eventfd", 1, 'i', "i") \
    X("fallocate", 4, 'i', "iiii") \
    X("timerfd_settime", 4, 'i', "iipp") \
    X("timerfd_gettime", 2, 'i', "ip") \
    X("accept4", 4, 'i', "ippi") \
    X("signalfd", 3, 'i', "ipi") \
    X("eventfd", 2, 'i', "ii") \
    X("epoll_create1", 1, 'i', "i") \
    X("dup3", 3, 'i', "iii") \
    X("pipe2", 2, 'i', "pi") \
    X("inotify_init1", 1, 'i', "i") \
    X("preadv", 4, 'l', "ipii") \
    X("pwritev", 4, 'l', "ipii") \
    X("rt_tgsigqueueinfo", 4, 'i', "iiip") \
    X("perf_event_open", 5, 'i', "piiii") \
    X("recvmmsg", 5, 'i', "ipiip") \
    X("fopen", 2, 'p', "pp") X("freopen", 3, 'p', "ppp") X("tmpfile", 0, 'p', "") X("fclose", 1, 'i', "p") X("fileno", 1, 'i', "p") X("fflush", 1, 'i', "p") X("fseek", 3, 'i', "pii") \
    X("ftell", 1, 'l', "p") X("rewind", 1, 'c', "p") \
    X("fread", 4, 'l', "piip") X("fwrite", 4, 'l', "piip") X("fgetc", 1, 'i', "p") X("fputc", 2, 'i', "ip") X("putc", 2, 'i', "ip") X("perror", 1, 'c', "p") \
    X("fgets", 3, 'p', "pip") X("getline", 3, 'l', "ppp") X("fputs", 2, 'i', "pp") X("putchar", 1, 'i', "i") X("puts", 1, 'i', "p") \
    X("feof", 1, 'i', "p") X("ferror", 1, 'i', "p") \
    X("clearerr", 1, 'c', "p") \
    X("stat", 2, 'i', "pp") X("fstat", 2, 'i', "ip") X("unlink", 1, 'i', "p") \
    X("openat", 4, 'i', "ipii") X("lstat", 2, 'i', "pp") X("readlink", 3, 'l', "ppi") \
    X("fstatat", 4, 'i', "ippi") X("readlinkat", 4, 'l', "ippi") X("unlinkat", 3, 'i', "ipi") \
    X("mkdirat", 3, 'i', "ipi") X("renameat", 4, 'i', "ipip") X("symlinkat", 3, 'i', "pip") \
    X("getdents64", 3, 'l', "ipi") X("dirent64_next", 4, 'i', "pipp") \
    X("rename", 2, 'i', "pp") X("mkdir", 2, 'i', "pi") X("rmdir", 1, 'i', "p") X("lseek", 3, 'l', "iii") \
    X("mmap", 6, 'p', "piiiii") X("munmap", 2, 'i', "pi") X("mremap", 5, 'p', "piiip") \
    X("map_failed", 1, 'i', "p") \
    X("malloc", 1, 'p', "i") X("calloc", 2, 'p', "ii") X("free", 1, 'c', "p") X("realloc", 2, 'p', "pi") \
    X("allocator_trim", 0, 'i', "") \
    X("printf", 0, 'i', "p") X("fprintf", 0, 'i', "pp") X("scanf", 0, 'i', "p") X("fscanf", 0, 'i', "pp") X("vfprintf", 3, 'i', "ppp") X("sprintf", 0, 'i', "pp") X("snprintf", 0, 'i', "pip") X("vsnprintf", 4, 'i', "pipp") \
    X("input", 2, 'i', "pi") X("__errno_location", 0, 'p', "")

/**
 * @brief Identifies a character classification or conversion routine.
 * @param name Function name to inspect.
 * @return Nonzero if the name identifies a ctype routine.
 */
static inline int crunch_is_ctype_name(const char *name) {
#define CTYPE_NAME(text, argc, result, arguments) text,
    static const char *const names[] = { CRUNCH_CTYPE_ROUTINES(CTYPE_NAME) };
#undef CTYPE_NAME
    if (!name)
        return 0;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!strcmp(name, names[i]))
            return 1;
    return 0;
}

/**
 * @brief Identifies a source name handled by the Crunch runtime.
 * @param name Function name to inspect.
 * @return Nonzero if the name identifies a runtime routine.
 */
static inline int crunch_is_runtime_name(const char *name) {
#define RUNTIME_NAME(text, argc, result, arguments) text,
    static const char *const names[] = { CRUNCH_RUNTIME_ROUTINES(RUNTIME_NAME) };
#undef RUNTIME_NAME
    if (!name)
        return 0;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!strcmp(name, names[i]))
            return 1;
    return 0;
}

#endif
