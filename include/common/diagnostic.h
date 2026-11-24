#ifndef CRUNCH_COMMON_DIAGNOSTIC_H
#define CRUNCH_COMMON_DIAGNOSTIC_H

#include <stdio.h>
#include "common/compiler.h"

typedef enum {
    DIAGNOSTIC_NOTE,
    DIAGNOSTIC_WARNING,
    DIAGNOSTIC_ERROR
} DiagnosticSeverity;

typedef struct {
    const char *tool;
    const char *filename;
    const char *code;
    const char *message;
    const char *function;
    DiagnosticSeverity severity;
    SourceLocation source_location;
    size_t ir_line;
    const char *source_line;
    size_t source_line_length;
    char function_storage[128];
} Diagnostic;

/**
 * @brief Writes a diagnostic and its available source or IR context.
 * @param stream FILE stream receiving the formatted diagnostic.
 * @param diagnostic Diagnostic record to format.
 * @return No value; writes the diagnostic to stream.
 */
void diagnostic_report(FILE *stream, const Diagnostic *diagnostic);

#endif
