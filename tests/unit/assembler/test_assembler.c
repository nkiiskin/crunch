#include "assembler/assembler.h"
#include <stdio.h>
#include <string.h>

#ifdef ASSEMBLER_TEST_FAIL_STRDUP
char *__real_strdup(const char *text);
static int fail_copy_at, matching_copies;

/**
 * @brief Injects failures when copying the test symbol.
 * @param text String to copy.
 * @return Allocated copy, or NULL at the selected failure point.
 */
char *__wrap_strdup(const char *text)
{
    if (fail_copy_at && strcmp(text, "marker") == 0 &&
        ++matching_copies == fail_copy_at)
        return NULL;
    return __real_strdup(text);
}

/**
 * @brief Checks cleanup and status after a symbol-copy allocation failure.
 * @param path Assembly input path.
 * @param output Object output path.
 * @param fail_at Index of the matching symbol copy to fail.
 * @return Nonzero if the injected failure was correctly reported.
 */
static int check_copy_failure(const char *path, const char *output, int fail_at)
{
    const char *input[] = {path};
    matching_copies = 0;
    fail_copy_at = fail_at;
    CompilerStatus status = crunch_assemble_files(input, 1, output);
    fail_copy_at = 0;
    return matching_copies >= fail_at && status == COMPILER_CODEGEN_ERROR;
}
#endif

/**
 * @brief Exercises repeated assembler API calls across success and failure.
 * @param argc Number of command-line arguments.
 * @param argv Input, missing-input, two output paths, and invalid input paths.
 * @return Zero if all assembler calls return the expected status.
 */
int main(int argc, char **argv)
{
    if (argc == 6 && strcmp(argv[1], "--includes") == 0) {
        const char *input[] = {argv[2]};
        const char *missing[] = {argv[3]};
        const char *directories[] = {argv[4]};
        if (crunch_assemble_files_with_includes(input, 1, argv[5], directories, 1) != COMPILER_OK ||
            crunch_assemble_files(missing, 1, argv[5]) != COMPILER_IO_ERROR ||
            crunch_assemble_files(input, 1, argv[5]) != COMPILER_IO_ERROR ||
            crunch_assemble_files_with_includes(input, 1, argv[5], directories, 1) != COMPILER_OK ||
            crunch_assemble_files_with_includes(input, 1, argv[5], NULL, 1) != COMPILER_INVALID_INPUT)
            return 1;
        return 0;
    }
#ifdef ASSEMBLER_TEST_FAIL_STRDUP
    if (argc == 5 && strcmp(argv[1], "--fail-copy") == 0) {
        const char *data[] = {argv[2]};
        const char *branch[] = {argv[3]};
        if (!check_copy_failure(argv[2], argv[4], 1) ||
            !check_copy_failure(argv[2], argv[4], 2) ||
            !check_copy_failure(argv[3], argv[4], 1))
            return 1;
        if (crunch_assemble_files(data, 1, argv[4]) != COMPILER_OK ||
            crunch_assemble_files(branch, 1, argv[4]) != COMPILER_OK)
            return 1;
        return 0;
    }
#endif
    if (argc < 6) return 2;
    const char *input[] = {argv[1]};
    const char *missing[] = {argv[2]};
    for (int repeat = 0; repeat < 3; ++repeat) {
        if (crunch_assemble_files(input, 1, argv[3]) != COMPILER_OK) return 1;
        for (int i = 5; i < argc; ++i) {
            const char *invalid[] = {argv[i]};
            if (crunch_assemble_files(invalid, 1, argv[3]) != COMPILER_CODEGEN_ERROR)
                return 1;
        }
        if (crunch_assemble_files(missing, 1, argv[3]) != COMPILER_IO_ERROR) return 1;
        if (crunch_assemble_files(input, 1, argv[4]) != COMPILER_OK) return 1;
    }
    if (crunch_assemble_files(NULL, 0, argv[4]) != COMPILER_INVALID_INPUT) return 1;
    return 0;
}
