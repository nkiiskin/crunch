#ifndef CRUNCH_CODEGEN_X86_64_H
#define CRUNCH_CODEGEN_X86_64_H

#include <stdio.h>
#include "common/diagnostic.h"

/**
 * @brief Validates Crunch IR and translates it to Linux x86-64 assembly.
 * @param input FILE stream supplying textual Crunch IR.
 * @param output FILE stream receiving generated assembly.
 * @param diagnostic Optional diagnostic record receiving failure context.
 * @param message Optional buffer receiving a diagnostic message.
 * @param message_size Capacity of message in bytes.
 * @return Zero on success; nonzero when validation, translation, or I/O fails.
 */
int crunch_x86_64_translate_with_diagnostic(
    FILE *input,
    FILE *output,
    Diagnostic *diagnostic,
    char *message,
    size_t message_size
);

/**
 * @brief Translates IR and emits an optional source filename in the assembly.
 * @param input FILE stream supplying textual Crunch IR.
 * @param output FILE stream receiving generated assembly.
 * @param source_filename Source filename for .file, or NULL to omit it.
 * @param diagnostic Diagnostic record receiving failure context.
 * @param message Buffer receiving a diagnostic message.
 * @param message_size Capacity of message in bytes.
 * @return Zero on success; nonzero when validation, translation, or I/O fails.
 */
int crunch_x86_64_translate_source_with_diagnostic(
    FILE *input, FILE *output, const char *source_filename,
    Diagnostic *diagnostic, char *message, size_t message_size
);

/**
 * @brief Translates IR with optional source filename and startup entry emission.
 * @param input FILE stream supplying textual Crunch IR.
 * @param output FILE stream receiving generated assembly.
 * @param source_filename Source filename for .file, or NULL to omit it.
 * @param emit_start Nonzero to emit the Crunch process entry point.
 * @param diagnostic Diagnostic record receiving failure context.
 * @param message Buffer receiving a diagnostic message.
 * @param message_size Capacity of the message buffer.
 * @return Zero on success; nonzero when validation, translation, or I/O fails.
 */
int crunch_x86_64_translate_source_options_with_diagnostic(
    FILE *input, FILE *output, const char *source_filename, int emit_start,
    Diagnostic *diagnostic, char *message, size_t message_size
);

/**
 * @brief Validates Crunch IR without emitting assembly.
 * @param input FILE stream supplying textual Crunch IR.
 * @param diagnostic Diagnostic record receiving failure context.
 * @param message Buffer receiving the diagnostic message.
 * @param message_size Capacity of the message buffer.
 * @return Zero on success; nonzero when validation or I/O fails.
 */
int crunch_x86_64_validate_with_diagnostic(
    FILE *input,
    Diagnostic *diagnostic,
    char *message,
    size_t message_size
);

#endif
