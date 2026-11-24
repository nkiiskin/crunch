#ifndef CRUNCH_CODEGEN_CODEGEN_H
#define CRUNCH_CODEGEN_CODEGEN_H

#include <stdio.h>

#include "common/compiler.h"
#include "ir/ir.h"

typedef struct {
    const char *target;
    bool emit_debug_info;
} CodegenOptions;

/**
 * @brief Generates target code from a validated IR program.
 * @param program IR program to generate code from.
 * @param options Target and code-generation options.
 * @param output FILE stream receiving generated code.
 * @return COMPILER_OK on success, otherwise the applicable input, code-generation, or I/O error.
 */
CompilerStatus codegen_run(
    const IRProgram *program,
    const CodegenOptions *options,
    FILE *output
);

#endif
