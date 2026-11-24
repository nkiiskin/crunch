#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

int __crunch_input(unsigned char *buffer, int max_length);
int *__crunch___errno_location(void);

static int signal_writer;
static volatile sig_atomic_t signal_observed;

/**
 * @brief Unblocks the pending input read by writing its remaining line bytes.
 * @param signal_number Delivered signal number.
 * @return No value; records whether the signal-safe write succeeded.
 */
static void finish_line(int signal_number)
{
    (void)signal_number;
    signal_observed = write(signal_writer, "cd\n", 3) == 3 ? 1 : -1;
}

/**
 * @brief Redirects standard input to a pipe, optionally in nonblocking mode.
 * @param descriptors Output pipe descriptors; the read end is closed after duplication.
 * @param nonblocking Whether reads from standard input should be nonblocking.
 * @return No value; assertions terminate the test if setup fails.
 */
static void pipe_stdin(int descriptors[2], int nonblocking)
{
    assert(pipe(descriptors) == 0);
    if (nonblocking)
        assert(fcntl(descriptors[0], F_SETFL, O_NONBLOCK) == 0);
    assert(dup2(descriptors[0], STDIN_FILENO) == STDIN_FILENO);
    assert(close(descriptors[0]) == 0);
}

/**
 * @brief Tests empty and partial nonblocking reads, including failure while discarding.
 * @note Takes no arguments and returns no value; assertions enforce each contract.
 */
static void test_nonblocking(void)
{
    int descriptors[2];
    unsigned char buffer[8];
    int *error_number = __crunch___errno_location();
    pipe_stdin(descriptors, 1);

    *error_number = 0;
    assert(__crunch_input(buffer, 5) == -1);
    assert(*error_number == EAGAIN && buffer[0] == '\0');

    assert(write(descriptors[1], "ab", 2) == 2);
    assert(__crunch_input(buffer, 5) == -1);
    assert(*error_number == EAGAIN && memcmp(buffer, "ab\0", 3) == 0);
    assert(write(descriptors[1], "cd\n", 3) == 3);
    assert(__crunch_input(buffer, 5) == 2);
    assert(memcmp(buffer, "cd\0", 3) == 0 && *error_number == EAGAIN);

    assert(write(descriptors[1], "abcdef", 6) == 6);
    assert(__crunch_input(buffer, 3) == -1);
    assert(*error_number == EAGAIN && memcmp(buffer, "abc\0", 4) == 0);
    assert(write(descriptors[1], "gh\nnext\n", 8) == 8);
    assert(__crunch_input(buffer, 5) == 2);
    assert(memcmp(buffer, "gh\0", 3) == 0);
    assert(__crunch_input(buffer, 5) == 4);
    assert(memcmp(buffer, "next\0", 5) == 0);
    assert(close(descriptors[1]) == 0);
}

/**
 * @brief Tests retrying an interrupted blocking read after a partial prefix.
 * @note Takes no arguments and returns no value; assertions enforce each contract.
 */
static void test_interrupted(void)
{
    int descriptors[2];
    unsigned char buffer[8];
    int *error_number = __crunch___errno_location();
    struct sigaction action = {0};
    struct itimerval timer = {0};
    pipe_stdin(descriptors, 0);
    assert(write(descriptors[1], "ab", 2) == 2);
    signal_writer = descriptors[1];
    signal_observed = 0;
    action.sa_handler = finish_line;
    assert(sigemptyset(&action.sa_mask) == 0);
    assert(sigaction(SIGALRM, &action, NULL) == 0);
    timer.it_value.tv_usec = 100000;
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    *error_number = ERANGE;
    assert(__crunch_input(buffer, 5) == 4);
    assert(signal_observed == 1);
    assert(memcmp(buffer, "abcd\0", 5) == 0 && *error_number == ERANGE);
    assert(close(descriptors[1]) == 0);
}

/**
 * @brief Runs input error and signal-interruption regressions.
 * @note Takes no arguments; returns zero after all assertions pass.
 */
int main(void)
{
    test_nonblocking();
    test_interrupted();
    return 0;
}
