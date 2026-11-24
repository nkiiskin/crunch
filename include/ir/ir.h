#ifndef CRUNCH_IR_H
#define CRUNCH_IR_H
#include <stdio.h>
#include "common/compiler.h"
#include "common/diagnostic.h"
#include "parser/ast.h"
/* Crunch's stable, textual, target-independent stack IR. */
/**
 * @brief Emits a semantically validated AST as textual Crunch IR.
 * @param program AST program to validate and emit.
 * @param output FILE stream receiving the IR.
 * @param error Optional buffer receiving a diagnostic message on failure.
 * @param error_size Capacity of error in bytes.
 * @return COMPILER_OK on success, otherwise the applicable input, semantic, code-generation, or I/O error.
 */
CompilerStatus crunch_ir_emit(const AstProgram *program, FILE *output, char *error, size_t error_size);

/**
 * @brief Emits Crunch IR and fills a structured diagnostic on failure.
 * @param program AST program to validate and emit.
 * @param output FILE stream receiving the IR.
 * @param diagnostic Optional diagnostic record receiving failure context.
 * @param message Optional buffer receiving the diagnostic message.
 * @param message_size Capacity of message in bytes.
 * @return COMPILER_OK on success, otherwise the applicable input, semantic, code-generation, or I/O error.
 */
CompilerStatus crunch_ir_emit_with_diagnostic(
    const AstProgram *program,
    FILE *output,
    Diagnostic *diagnostic,
    char *message,
    size_t message_size
);

/**
 * @brief Emits separately compilable module IR and fills a diagnostic on failure.
 * @param program AST program to validate and emit.
 * @param output FILE stream receiving the IR.
 * @param diagnostic Diagnostic record receiving failure context.
 * @param message Buffer receiving the diagnostic message.
 * @param message_size Capacity of message in bytes.
 * @return COMPILER_OK on success, otherwise the applicable compiler or I/O error.
 */
CompilerStatus crunch_ir_emit_module_with_diagnostic(
    const AstProgram *program,
    FILE *output,
    Diagnostic *diagnostic,
    char *message,
    size_t message_size
);
#endif
