#ifndef CRUNCH_SIGNAL_H
#define CRUNCH_SIGNAL_H

#include <sys/types.h>
#include <time.h>

#macro SIGHUP 1
#macro SIGINT 2
#macro SIGQUIT 3
#macro SIGILL 4
#macro SIGTRAP 5
#macro SIGABRT 6
#macro SIGBUS 7
#macro SIGFPE 8
#macro SIGKILL 9
#macro SIGUSR1 10
#macro SIGSEGV 11
#macro SIGUSR2 12
#macro SIGPIPE 13
#macro SIGALRM 14
#macro SIGTERM 15
#macro SIGSTKFLT 16
#macro SIGCHLD 17
#macro SIGCONT 18
#macro SIGSTOP 19
#macro SIGTSTP 20
#macro SIGTTIN 21
#macro SIGTTOU 22
#macro SIGURG 23
#macro SIGXCPU 24
#macro SIGXFSZ 25
#macro SIGVTALRM 26
#macro SIGPROF 27
#macro SIGWINCH 28
#macro SIGIO 29
#macro SIGPWR 30
#macro SIGSYS 31

typedef unsigned long sigset_t;
typedef union sigval {
    int sival_int;
    void* sival_ptr;
} sigval_t;
typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    int __padding;
    union {
        struct {
            pid_t pid;
            uid_t uid;
            sigval_t value;
        } queued;
        unsigned char padding[112];
    } data;
} siginfo_t;
typedef struct {
    void* ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;
#macro SI_QUEUE -1
#macro SS_ONSTACK 1
#macro SS_DISABLE 2
#macro SA_ONSTACK 134217728
typedef void (*sighandler_t)(int);
struct sigaction {
    sighandler_t sa_handler;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    sigset_t sa_mask;
};
#macro SIG_DFL ((sighandler_t)0)
#macro SIG_IGN crunch_sig_ign()
#macro SA_RESTART 268435456
#macro SA_NODEFER 1073741824
#macro SA_RESETHAND 2147483648U
#macro SIG_BLOCK 0
#macro SIG_UNBLOCK 1
#macro SIG_SETMASK 2

/**
 * @brief Produces Linux's ignored-signal disposition.
 * @return The SIG_IGN handler value.
 */
sighandler_t crunch_sig_ign(void);

/**
 * @brief Clears a Linux signal set.
 * @param set Writable signal set.
 * @return Zero, or -1 with errno set for NULL.
 */
int sigemptyset(sigset_t* set);
/**
 * @brief Installs or queries a Linux signal action.
 * @param signal Signal number.
 * @param action New action, or NULL to query only.
 * @param previous Receives the previous action, or NULL.
 * @return Zero, or -1 with errno set.
 * @note Crunch uses the Linux x86-64 kernel layout, not libc's sigaction ABI.
 */
int sigaction(int signal, const struct sigaction* action, struct sigaction* previous);

/**
 * @brief Changes or queries the calling thread's Linux signal mask.
 * @param how SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK.
 * @param set Signal set to apply, or NULL to query only.
 * @param previous Receives the previous mask, or NULL.
 * @return Zero, or -1 with errno set.
 * @note Signal sets use the eight-byte Linux kernel layout, not libc's sigset_t.
 */
int sigprocmask(int how, const sigset_t* set, sigset_t* previous);
/**
 * @brief Reads pending signals using the eight-byte Linux kernel signal-set layout.
 * @param set Linux kernel signal set.
 * @return The result, or -1 with errno set.
 */
int sigpending(sigset_t* set);
/**
 * @brief Waits for a signal in set, optionally with a timeout.
 * @param set Linux kernel signal set.
 * @param information Linux kernel signal information.
 * @param timeout Relative timeout, or NULL.
 * @return The result, or -1 with errno set.
 */
int sigtimedwait(const sigset_t* set, siginfo_t* information, const struct timespec* timeout);
/**
 * @brief Temporarily replaces the signal mask with the eight-byte Linux kernel signal set and waits for a signal.
 * @param mask Temporary signal mask.
 * @return The result, or -1 with errno set.
 */
int sigsuspend(const sigset_t* mask);
/**
 * @brief Sends a signal using the raw Linux rt_sigqueueinfo syscall.
 * @param process Process identifier; zero selects the caller where supported.
 * @param signal Signal number.
 * @param information Linux kernel signal information.
 * @return The result, or -1 with errno set.
 */
int rt_sigqueueinfo(pid_t process, int signal, const siginfo_t* information);
/**
 * @brief Changes or queries the alternate signal stack using the Linux x86-64 kernel stack_t layout.
 * @param stack New alternate stack, or NULL.
 * @param previous Receives the previous setting, or NULL.
 * @return The result, or -1 with errno set.
 */
int sigaltstack(const stack_t* stack, stack_t* previous);

/**
 * @brief Sends a signal, or checks permissions and existence with signal zero.
 * @param process Linux process selector: positive PID, zero, -1, or negative group ID.
 * @param signal Signal number, or zero.
 * @return Zero on success, or -1 with errno set.
 */
int kill(pid_t process, int signal);

#endif /* CRUNCH_SIGNAL_H */
