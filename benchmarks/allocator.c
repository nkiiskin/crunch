#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern void *_crunch_malloc(size_t size);
extern void _crunch_free(void *pointer);
extern int *__crunch___errno_location(void);

static int read_proc(const char *path, char *buffer, size_t capacity)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        return 0;
    }
    ssize_t length = read(fd, buffer, capacity - 1);
    if (length < 0) {
        perror(path);
        close(fd);
        return 0;
    }
    if (close(fd) != 0) {
        perror("close");
        return 0;
    }
    buffer[length] = '\0';
    return 1;
}

static int memory_usage(unsigned long long *virtual_kib, unsigned long long *rss_kib)
{
    char buffer[8192];
    unsigned long long pages;
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || !read_proc("/proc/self/statm", buffer, sizeof buffer) ||
        sscanf(buffer, "%llu", &pages) != 1) {
        fprintf(stderr, "cannot read process virtual memory size\n");
        return 0;
    }
    *virtual_kib = pages * (unsigned long long)page_size / 1024;
    if (!read_proc("/proc/self/smaps_rollup", buffer, sizeof buffer))
        return 0;
    char *rss = strstr(buffer, "\nRss:");
    if (!rss || sscanf(rss, "\nRss: %llu kB", rss_kib) != 1) {
        fprintf(stderr, "cannot read process resident memory size\n");
        return 0;
    }
    return 1;
}

static int timestamp(struct timespec *time)
{
    if (clock_gettime(CLOCK_MONOTONIC, time) == 0)
        return 1;
    perror("clock_gettime");
    return 0;
}

static double milliseconds(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
}

static int positive_long(const char *text, long *value)
{
    char *end;
    errno = 0;
    long result = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || result <= 0)
        return 0;
    *value = result;
    return 1;
}

int main(int argc, char **argv)
{
    long count, size;
    if (argc != 4 || (strcmp(argv[1], "crunch") && strcmp(argv[1], "system")) ||
        !positive_long(argv[2], &count) || !positive_long(argv[3], &size) ||
        (unsigned long)count > SIZE_MAX / sizeof(void *) ||
        (unsigned long)count > SIZE_MAX / (unsigned long)size) {
        fprintf(stderr, "usage: allocator crunch|system COUNT SIZE\n");
        return 2;
    }

    int crunch = strcmp(argv[1], "crunch") == 0;
    void **blocks = calloc((size_t)count, sizeof *blocks);
    if (!blocks) {
        perror("pointer array");
        return 1;
    }
    /* Commit the bookkeeping array before recording the baseline. */
    memset(blocks, 1, (size_t)count * sizeof *blocks);

    unsigned long long before_vm, before_rss, after_vm, after_rss;
    struct timespec start, allocated, touched, release_start, released;
    if (!memory_usage(&before_vm, &before_rss) || !timestamp(&start)) {
        free(blocks);
        return 1;
    }
    long allocated_count = 0;
    for (; allocated_count < count; ++allocated_count) {
        blocks[allocated_count] = crunch ? _crunch_malloc(size) : malloc((size_t)size);
        if (!blocks[allocated_count]) {
            fprintf(stderr, "%s allocation %ld failed (errno %d)\n", argv[1], allocated_count,
                    crunch ? *__crunch___errno_location() : errno);
            break;
        }
    }
    if (allocated_count != count) {
        for (long i = 0; i < allocated_count; ++i) {
            if (crunch) _crunch_free(blocks[i]); else free(blocks[i]);
        }
        free(blocks);
        return 1;
    }
    if (!timestamp(&allocated)) return 1;
    for (long i = 0; i < count; ++i)
        memset(blocks[i], 0x5a, (size_t)size);
    if (!timestamp(&touched) || !memory_usage(&after_vm, &after_rss) ||
        !timestamp(&release_start)) return 1;
    for (long i = 0; i < count; ++i) {
        if (crunch) _crunch_free(blocks[i]); else free(blocks[i]);
    }
    if (!timestamp(&released)) return 1;
    free(blocks);

    printf("%.3f %.3f %.3f %lld %lld\n",
           milliseconds(start, allocated), milliseconds(allocated, touched),
           milliseconds(release_start, released),
           (long long)after_vm - (long long)before_vm,
           (long long)after_rss - (long long)before_rss);
    return 0;
}
