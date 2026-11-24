#ifndef CRUNCH_SYS_AIO_H
#define CRUNCH_SYS_AIO_H
#include <stdint.h>
#include <time.h>

typedef unsigned long aio_context_t;

#macro IOCB_CMD_PREAD 0
#macro IOCB_CMD_PWRITE 1
#macro IOCB_CMD_FSYNC 2
#macro IOCB_CMD_FDSYNC 3
#macro IOCB_CMD_POLL 5
#macro IOCB_CMD_NOOP 6
#macro IOCB_CMD_PREADV 7
#macro IOCB_CMD_PWRITEV 8
#macro IOCB_FLAG_RESFD 1
#macro IOCB_FLAG_IOPRIO 2

union aio_buffer {
    void* pointer;
    uint64_t value;
};

struct iocb {
    uint64_t aio_data;
    uint32_t aio_key;
    uint32_t aio_rw_flags;
    uint16_t aio_lio_opcode;
    int16_t aio_reqprio;
    uint32_t aio_fildes;
    union aio_buffer aio_buf;
    uint64_t aio_nbytes;
    int64_t aio_offset;
    uint64_t aio_reserved2;
    uint32_t aio_flags;
    uint32_t aio_resfd;
};

struct io_event {
    uint64_t data;
    struct iocb* obj;
    int64_t res;
    int64_t res2;
};

/**
 * @brief Creates a Linux kernel asynchronous I/O context.
 * @param max_events Maximum number of outstanding events.
 * @param context Kernel AIO context.
 * @return The syscall result, or -1 with errno set.
 */
int io_setup(unsigned int max_events, aio_context_t* context);

/**
 * @brief Destroys a kernel asynchronous I/O context, waiting for outstanding requests.
 * @param context Kernel AIO context.
 * @return The syscall result, or -1 with errno set.
 */
int io_destroy(aio_context_t context);

/**
 * @brief Retrieves kernel AIO completion events, optionally waiting up to timeout.
 * @param context Kernel AIO context.
 * @param minimum Minimum number of completions to wait for.
 * @param maximum Maximum number of events.
 * @param events Writable completion event array.
 * @param timeout Optional timeout.
 * @return The syscall result, or -1 with errno set.
 */
long io_getevents(aio_context_t context, long minimum, long maximum, struct io_event* events, struct timespec* timeout);

/**
 * @brief Submits an array of pointers to Linux kernel AIO control blocks.
 * @param context Kernel AIO context.
 * @param count Number of requests.
 * @param requests Array of pointers to control blocks.
 * @return The syscall result, or -1 with errno set.
 * @note Use aio_buf.pointer for buffers and aio_buf.value for integer arguments.
 */
long io_submit(aio_context_t context, long count, struct iocb** requests);

/**
 * @brief Attempts to cancel an outstanding kernel AIO request.
 * @param context Kernel AIO context.
 * @param request Control block to cancel.
 * @param result Receives the cancellation event on success.
 * @return The syscall result, or -1 with errno set.
 */
int io_cancel(aio_context_t context, struct iocb* request, struct io_event* result);

#endif
