#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);
int pp_main(int argc, char **argv, char **envp);

static void *live[4096];
static size_t live_count, allocation_count, fail_at;
static int tracking, persistent;

/**
 * @brief Records an allocation owned by the preprocessor.
 * @param ptr Allocated pointer, or NULL.
 * @return No value.
 */
static void record(void *ptr)
{
    if (tracking && ptr) {
        assert(live_count < sizeof(live) / sizeof(live[0]));
        live[live_count++] = ptr;
    }
}

/**
 * @brief Removes a tracked allocation, ignoring allocations made inside libc.
 * @param ptr Pointer being freed or reallocated.
 * @return Nonzero if the pointer was tracked.
 */
static int forget(void *ptr)
{
    if (tracking && ptr) {
        for (size_t i = 0; i < live_count; ++i) {
            if (live[i] == ptr) {
                live[i] = live[--live_count];
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief Determines whether the current allocation should fail.
 * @return Nonzero when failure injection is active at this allocation.
 */
static int should_fail(void)
{
    if (!tracking) return 0;
    ++allocation_count;
    return fail_at && (allocation_count == fail_at ||
                       (persistent && allocation_count > fail_at));
}

/**
 * @brief Injects malloc failures.
 * @param size Requested allocation size.
 * @return Allocated pointer or NULL.
 */
void *__wrap_malloc(size_t size)
{
    if (should_fail()) return NULL;
    void *ptr = __real_malloc(size);
    record(ptr);
    return ptr;
}

/**
 * @brief Injects calloc failures.
 * @param count Number of elements.
 * @param size Element size.
 * @return Allocated pointer or NULL.
 */
void *__wrap_calloc(size_t count, size_t size)
{
    if (should_fail()) return NULL;
    void *ptr = __real_calloc(count, size);
    record(ptr);
    return ptr;
}

/**
 * @brief Injects realloc failures while tracking moved allocations.
 * @param ptr Existing allocation or NULL.
 * @param size Requested allocation size.
 * @return Reallocated pointer or NULL.
 */
void *__wrap_realloc(void *ptr, size_t size)
{
    if (should_fail()) return NULL;
    int was_tracked = forget(ptr);
    void *grown = __real_realloc(ptr, size);
    if (grown) record(grown);
    else if (was_tracked && size) record(ptr);
    return grown;
}

/**
 * @brief Tracks and releases an allocation.
 * @param ptr Pointer to release.
 * @return No value.
 */
void __wrap_free(void *ptr)
{
    forget(ptr);
    __real_free(ptr);
}

/**
 * @brief Runs the preprocessor with each allocation failed in turn.
 * @param argc Number of preprocessor arguments.
 * @param argv Preprocessor arguments, including the test executable name.
 * @return Zero when all cases report errors and release allocations.
 */
int main(int argc, char **argv)
{
    FILE *output = tmpfile(), *errors = tmpfile();
    assert(output && errors);
    int saved_err = dup(STDERR_FILENO);
    assert(saved_err >= 0);
    assert(dup2(fileno(output), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(errors), STDERR_FILENO) >= 0);
    size_t count = 0;
    for (persistent = 0; persistent <= 1; ++persistent) {
        for (size_t failure = 0; failure <= count; ++failure) {
            assert(fflush(stdout) == 0 && fflush(stderr) == 0);
            assert(ftruncate(fileno(output), 0) == 0);
            assert(ftruncate(fileno(errors), 0) == 0);
            rewind(output);
            rewind(errors);
            clearerr(stdout);
            clearerr(stderr);
            allocation_count = live_count = 0;
            fail_at = failure;
            tracking = 1;
            int status = pp_main(argc, argv, NULL);
            tracking = 0;
            if (!failure) count = allocation_count;
            assert(count > 0 && (!failure || allocation_count >= failure));
            assert(fflush(stderr) == 0);
            struct stat diagnostic;
            assert(fstat(fileno(errors), &diagnostic) == 0);
            char text[1024];
            ssize_t n = pread(fileno(errors), text, sizeof(text) - 1, 0);
            assert(n >= 0);
            text[n] = '\0';
            if ((!failure && status != 0) ||
                (failure && (status == 0 || diagnostic.st_size <= 0)) ||
                strstr(text, "unterminated argument list") || live_count != 0) {
                dprintf(saved_err, "preprocessor failure %zu (persistent %d): "
                        "status %d, diagnostics %lld bytes, %zu allocations live\n%s",
                        failure, persistent, status, (long long)diagnostic.st_size,
                        live_count, text);
                return 1;
            }
        }
    }
    close(saved_err);
    fclose(output);
    fclose(errors);
    return 0;
}
