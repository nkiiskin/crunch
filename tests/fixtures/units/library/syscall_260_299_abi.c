#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

struct crunch_pselect_sigmask {
    const unsigned long *mask;
    size_t size;
};

int crunch_pselect6(int, void *, void *, void *, struct timespec *,
                    const struct crunch_pselect_sigmask *);
int crunch_ppoll(void *, unsigned long, struct timespec *, const unsigned long *);
int crunch_signalfd(int, const unsigned long *);

int main(void) {
    int fd = open("abi-file", O_CREAT | O_RDWR | O_TRUNC, 0600);
    char source[] = "xyz", destination[3] = {0};
    struct iovec vec = {source, 3}, dest = {destination, 3};
    const off_t offset = 4294967300L;
    if (fd < 0 || pwritev(fd, &vec, 1, offset) != 3 ||
        preadv(fd, &dest, 1, offset) != 3 ||
        destination[0] != 'x' || destination[1] != 'y' || destination[2] != 'z')
        return 1;
    unsigned long mask = 0;
    struct crunch_pselect_sigmask arg = {&mask, 8};
    struct timespec timeout = {0, 0};
    if (crunch_pselect6(0, 0, 0, 0, &timeout, &arg) ||
        crunch_ppoll(0, 0, &timeout, &mask)) return 2;
    int signal_fd = crunch_signalfd(-1, &mask);
    if (signal_fd < 0 || close(signal_fd) || close(fd) || unlink("abi-file"))
        return 3;
    return 0;
}
