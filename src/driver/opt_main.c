#include "optimizer/optimizer.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Runs the pass-through IR optimizer.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return Process exit status.
 */
int main(int argc, char **argv)
{
    const char *path = "-";
    FILE *input;
    CompilerStatus status;
    if (argc > 2 || (argc == 2 && argv[1][0] == '-' && strcmp(argv[1], "-"))) {
        fprintf(stderr, "usage: crunch-opt [file.bir|-]\n");
        return 2;
    }
    if (argc == 2) path = argv[1];
    input = strcmp(path, "-") ? fopen(path, "rb") : stdin;
    if (!input) {
        fprintf(stderr, "crunch-opt: cannot open '%s': %s\n", path, strerror(errno));
        return 2;
    }
    status = crunch_optimize_ir(input, stdout);
    if (input != stdin && fclose(input) != 0)
        status = COMPILER_IO_ERROR;
    if (fflush(stdout) != 0)
        status = COMPILER_IO_ERROR;
    if (status != COMPILER_OK) {
        fprintf(stderr, "crunch-opt: I/O error copying IR\n");
        return 1;
    }
    return 0;
}
