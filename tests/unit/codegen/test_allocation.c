#define _POSIX_C_SOURCE 200809L
#include "codegen/x86_64.h"
#include "common/source.h"
#include "ir/ir.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);
char *__real_strdup(const char *text);
ssize_t __real_getline(char **line, size_t *capacity, FILE *input);

static void *live[65536];
static size_t live_count, allocation_count, fail_at;
static int tracking, persistent, getline_error = ENOMEM;

/**
 * @brief Records an allocation made by a tested stage.
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
 * @brief Removes a tracked allocation, ignoring libc-owned allocations.
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
 * @brief Checks whether the current allocation should fail.
 * @return Nonzero when failure injection triggers.
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
 * @param size Size of each element.
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
 * @brief Injects strdup failures.
 * @param text NUL-terminated source text.
 * @return Allocated copy or NULL.
 */
char *__wrap_strdup(const char *text)
{
    if (should_fail()) return NULL;
    char *copy = __real_strdup(text);
    record(copy);
    return copy;
}

/**
 * @brief Injects the allocation failure that getline may report.
 * @param line Input/output line buffer.
 * @param capacity Input/output buffer capacity.
 * @param input Input stream.
 * @return Line length, or -1 on EOF or failure.
 */
ssize_t __wrap_getline(char **line, size_t *capacity, FILE *input)
{
    if (should_fail()) {
        errno = getline_error;
        return -1;
    }
    return __real_getline(line, capacity, input);
}

/**
 * @brief Injects failures at each allocation in a single IR emission.
 * @param program Parsed and semantically validated input.
 * @param module Nonzero for module-mode output.
 * @param expected Uninjected status.
 * @return No value; aborts on a failure or leak.
 */
static void test_ir(const AstProgram *program, int module, CompilerStatus expected)
{
    size_t count = 0;
    for (persistent = 0; persistent <= 1; ++persistent) {
        for (size_t failure = 0; failure <= count; ++failure) {
            FILE *out = tmpfile();
            assert(out);
            Diagnostic diagnostic = {0};
            char message[512];
            allocation_count = live_count = 0;
            fail_at = failure;
            tracking = 1;
            CompilerStatus status = module
                ? crunch_ir_emit_module_with_diagnostic(program, out, &diagnostic,
                                                        message, sizeof(message))
                : crunch_ir_emit_with_diagnostic(program, out, &diagnostic,
                                                 message, sizeof(message));
            tracking = 0;
            if (!failure) count = allocation_count;
            if (status != (failure ? COMPILER_OUT_OF_MEMORY : expected) ||
                (status != COMPILER_OK && !diagnostic.message) || live_count ||
                (failure && (allocation_count < failure ||
                             strcmp(diagnostic.code, "E_OUT_OF_MEMORY") != 0))) {
                fprintf(stderr, "IR allocation %zu (persistent %d): status %d, "
                        "%zu allocations live, diagnostic: %s\n",
                        failure, persistent, status, live_count,
                        diagnostic.message ? diagnostic.message : "(none)");
                abort();
            }
            fclose(out);
        }
    }
    assert(count > 0);
}

/**
 * @brief Injects failures at each allocation in backend translation.
 * @param path Path of the IR input.
 * @param valid Nonzero when the input should translate without injection.
 * @return No value; aborts on a missing error, success on failure, or leak.
 */
static void test_backend(const char *path, int valid)
{
    size_t count = 0;
    for (persistent = 0; persistent <= 1; ++persistent) {
        for (size_t failure = 0; failure <= count; ++failure) {
            FILE *in = fopen(path, "rb"), *out = tmpfile();
            assert(in && out);
            Diagnostic diagnostic = {0};
            char message[512];
            allocation_count = live_count = 0;
            fail_at = failure;
            tracking = 1;
            int status = crunch_x86_64_translate_with_diagnostic(in, out, &diagnostic,
                                                                message, sizeof(message));
            tracking = 0;
            if (!failure) count = allocation_count;
            if ((failure ? status == 0 : (status == 0) != valid) ||
                (status != 0 && !diagnostic.message) || live_count ||
                (failure && (allocation_count < failure || !strstr(message, "memory")))) {
                fprintf(stderr, "backend allocation %zu (persistent %d): status %d, "
                        "%zu allocations live, diagnostic: %s\n",
                        failure, persistent, status, live_count,
                        diagnostic.message ? diagnostic.message : "(none)");
                abort();
            }
            fclose(in);
            fclose(out);
        }
    }
    assert(count > 0);
    if (valid) {
        FILE *in = fopen(path, "rb"), *out = tmpfile();
        assert(in && out);
        Diagnostic diagnostic = {0};
        char message[512];
        allocation_count = live_count = 0;
        fail_at = 1;
        persistent = 0;
        getline_error = EIO;
        tracking = 1;
        int status = crunch_x86_64_translate_with_diagnostic(in, out, &diagnostic,
                                                            message, sizeof(message));
        tracking = 0;
        getline_error = ENOMEM;
        assert(status != 0 && live_count == 0 && strstr(message, "failed to read IR"));
        fclose(in);
        fclose(out);
    }
}

/**
 * @brief Runs allocation-failure cases for an IR or assembly fixture.
 * @param argc Number of command-line arguments.
 * @param argv Stage, input path, and expected mode.
 * @return Zero when all checks pass.
 */
int main(int argc, char **argv)
{
    assert(argc == 4);
    if (!strcmp(argv[1], "backend")) {
        test_backend(argv[2], !strcmp(argv[3], "valid"));
    } else {
        assert(!strcmp(argv[1], "ir"));
        SourceFile source = {0};
        TokenArray tokens = {0};
        AstProgram *program = NULL;
        SemanticModel *model = NULL;
        assert(source_load(argv[2], &source) == COMPILER_OK);
        assert(lexer_run(&source, &tokens) == COMPILER_OK);
        assert(parser_run(&tokens, &program) == COMPILER_OK);
        assert(sema_run(program, &model) == COMPILER_OK);
        test_ir(program, !strcmp(argv[3], "module"),
                !strcmp(argv[3], "invalid") ? COMPILER_CODEGEN_ERROR : COMPILER_OK);
        sema_destroy(model);
        parser_destroy(program);
        lexer_destroy(&tokens);
        source_destroy(&source);
    }
    return 0;
}
