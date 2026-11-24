#include "common/diagnostic.h"

#include <string.h>

/**
 * @brief Returns the printable name of a diagnostic severity.
 * @param severity Diagnostic severity to render.
 * @return Static severity name, or "error" for an unrecognized severity.
 */
static const char *severity_name(DiagnosticSeverity severity)
{
    switch (severity) {
    case DIAGNOSTIC_NOTE: return "note";
    case DIAGNOSTIC_WARNING: return "warning";
    case DIAGNOSTIC_ERROR: return "error";
    }
    return "error";
}

/**
 * @brief Writes a diagnostic with available source and IR context.
 * @param stream Output FILE stream receiving the formatted diagnostic.
 * @param diagnostic Diagnostic record to format.
 * @return No value; the function performs the documented output or state update.
 */
void diagnostic_report(FILE *stream, const Diagnostic *diagnostic)
{
    if (!stream || !diagnostic || !diagnostic->message) return;

    if (diagnostic->tool && diagnostic->tool[0])
        fprintf(stream, "%s: ", diagnostic->tool);
    if (diagnostic->filename && diagnostic->filename[0])
        fprintf(stream, "%s", diagnostic->filename);

    if (diagnostic->source_location.line) {
        if (diagnostic->filename && diagnostic->filename[0]) fputc(':', stream);
        fprintf(stream, "%u:%u", diagnostic->source_location.line,
                diagnostic->source_location.column);
    } else if (diagnostic->ir_line) {
        if (diagnostic->filename && diagnostic->filename[0]) fputc(':', stream);
        fprintf(stream, "IR line %zu", diagnostic->ir_line);
    }

    if (diagnostic->function && diagnostic->function[0])
        fprintf(stream, " (function '%s')", diagnostic->function);
    if ((diagnostic->filename && diagnostic->filename[0]) ||
        diagnostic->source_location.line || diagnostic->ir_line)
        fputs(": ", stream);

    if (diagnostic->severity == DIAGNOSTIC_WARNING && diagnostic->code &&
        !strcmp(diagnostic->code, "W_SEMANTIC")) {
        fprintf(stream, "semantic warning: %s [%s]\n", diagnostic->message, diagnostic->code);
    } else {
        fprintf(stream, "%s", severity_name(diagnostic->severity));
        if (diagnostic->code && diagnostic->code[0])
            fprintf(stream, "[%s]", diagnostic->code);
        fprintf(stream, ": %s\n", diagnostic->message);
    }

    if (diagnostic->source_line && diagnostic->source_location.line &&
        diagnostic->source_location.column) {
        fprintf(stream, "  %.*s\n  ", (int)diagnostic->source_line_length,
                diagnostic->source_line);
        for (unsigned i = 1; i < diagnostic->source_location.column &&
                             i <= diagnostic->source_line_length; ++i)
            fputc(diagnostic->source_line[i - 1] == '\t' ? '\t' : ' ', stream);
        fputs("^\n", stream);
    }
}