Public Crunch prototypes in `<sys/mman.h>`:

    void* mmap(void* address, size_t byte_count, int protection,
               int flags, int descriptor, long offset);
    int munmap(void* address, size_t byte_count);
    void* mremap(void* address, size_t old_size, size_t new_size,
                 int flags, void* new_address);
    int map_failed(const void* address);

Crunch does not currently support general variadic functions.
Pass zero as the fifth `mremap` argument unless using `MREMAP_FIXED`.
Use `map_failed` on the result of `mmap` or `mremap`; those functions return
the Linux `MAP_FAILED` sentinel on error and set `errno`.

The internal assembly symbols `_crunch_mmap`, `_crunch_munmap`, and
`_crunch_mremap` use C-style results like the public wrappers. On failure
they set Crunch's process-wide `errno` and return -1; successful calls leave
`errno` unchanged. The internal `_crunch_mremap` requires a fifth argument
only when `MREMAP_FIXED` is set, as in C.

The file and process calls `access`, `dup`, `dup2`, `pipe`,
`fork`, and `getpid` are declared in `<unistd.h>`, `poll` in `<poll.h>`,
`select` in `<sys/select.h>`, and `kill` in `<signal.h>`.
Their `_crunch_*` assembly stubs return raw Linux results, including negative
error numbers, without changing `errno`. The `__crunch_*` wrappers implement
the public interface and convert errors to -1 with process-wide `errno` set.
`getpid` cannot fail on Linux and leaves `errno` unchanged.
