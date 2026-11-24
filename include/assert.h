#undef assert
#ifndef CRUNCH_ASSERT_SUPPORT_H
#define CRUNCH_ASSERT_SUPPORT_H

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * @brief Reports a failed assertion and terminates with SIGABRT.
 * @param expression Text of the failed expression.
 * @param file Source file containing the assertion.
 * @param line Source line containing the assertion.
 * @param function Function containing the assertion.
 * @return Does not return.
 */
static inline void noreturn __crunch_assert_fail(const char *expression, const char *file,
                                                 int line, const char *function)
{
    sigset_t mask = 1UL << (SIGABRT - 1);
    struct sigaction action = {0};
    fprintf(stderr, "%s:%d: %s: Assertion '%s' failed.\n", file, line, function, expression);
    sigprocmask(SIG_UNBLOCK, &mask, 0);
    kill(getpid(), SIGABRT);
    sigaction(SIGABRT, &action, 0);
    kill(getpid(), SIGABRT);
    exit(128 + SIGABRT);
}

#endif

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
#define assert(expression) ((expression) ? (void)0 : __crunch_assert_fail(#expression, __FILE__, __LINE__, __func__))
#endif
