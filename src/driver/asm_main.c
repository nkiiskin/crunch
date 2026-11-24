#define _POSIX_C_SOURCE 200809L
#include "common/diagnostic.h"
#include "codegen/x86_64.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Parses command-line options and runs the IR-to-assembly driver.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return Process exit status: zero for success and nonzero for failure.
 */
int main(int argc, char **argv)
{
    FILE *input;
    char error[256] = {0};
    Diagnostic diagnostic = {0};
    int status;
    int validate_only = 0;
    int emit_start = 1;
    const char *path = NULL, *source_filename = NULL;
    int close_input;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--validate") && !validate_only) {
            validate_only = 1;
        } else if (!strcmp(argv[i], "--no-entry") && emit_start) {
            emit_start = 0;
        } else if (!strcmp(argv[i], "--source-file") && !source_filename && i + 1 < argc) {
            source_filename = argv[++i];
        } else if (!path && (argv[i][0] != '-' || !strcmp(argv[i], "-"))) {
            path = argv[i];
        } else {
            fprintf(stderr, "usage: crunch-asm [--validate] [--no-entry] [--source-file NAME] [file.bir|-]\n");
            return 2;
        }
    }
    if (validate_only && (source_filename || !emit_start)) {
        fprintf(stderr, "crunch-asm: --source-file and --no-entry cannot be used with --validate\n");
        return 2;
    }
    if (!path) path = "-";

    close_input = strcmp(path, "-") != 0;
    input = close_input ? fopen(path, "r") : stdin;
    if (!input) {
        Diagnostic diagnostic = {
            .tool = "crunch-asm",
            .filename = path,
            .code = "E_IO",
            .message = strerror(errno),
            .severity = DIAGNOSTIC_ERROR
        };
        diagnostic_report(stderr, &diagnostic);
        return 2;
    }

    status = validate_only
        ? crunch_x86_64_validate_with_diagnostic(input, &diagnostic, error, sizeof error)
        : crunch_x86_64_translate_source_options_with_diagnostic(input, stdout, source_filename,
                                                                  emit_start, &diagnostic, error,
                                                                  sizeof error);
    if (close_input && fclose(input) != 0 && status == 0) {
        snprintf(error, sizeof error, "failed to close IR input");
        status = 1;
    }
    if (status == 0 && ferror(stdout)) {
        snprintf(error, sizeof error, "failed to write assembly output");
        status = 1;
    }
    if (status != 0) {
        diagnostic.tool = "crunch-asm";
        diagnostic.filename = path;
        if (!diagnostic.message)
            diagnostic.message = error[0] ? error : "translation failed";
        diagnostic_report(stderr, &diagnostic);
        return 1;
    }
    return 0;
}
