#include "common/diagnostic.h"
#include "common/source.h"
#include "ir/ir.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "sema/sema.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Resolves the latest preprocessor-mapped filename before a source offset.
 * @param source Source buffer and optional preprocessor mapping markers.
 * @param offset Source byte offset up to which markers are scanned.
 * @param mapped_name Buffer receiving a decoded filename when a valid marker is found.
 * @param mapped_name_size Capacity of mapped_name, including its terminating NUL.
 * @return Pointer into mapped_name, the original source filename, or NULL without a source.
 */
static const char *mapped_source_filename(
    const SourceFile *source,
    size_t offset,
    char *mapped_name,
    size_t mapped_name_size
) {
    const char *filename = source ? source->filename : NULL;
    static const char marker[] = "/*CRUNCH-PP-MAP:";
    if (source && source->data && offset <= source->length) {
        size_t i = 0;
        while (i + sizeof(marker) - 1 <= offset) {
            if ((i == 0 || source->data[i - 1] == '\n') &&
                memcmp(source->data + i, marker, sizeof(marker) - 1) == 0) {
                size_t cursor = i + sizeof(marker) - 1;
                while (cursor < offset &&
                       source->data[cursor] >= '0' && source->data[cursor] <= '9')
                    ++cursor;
                if (cursor < offset && source->data[cursor++] == ':') {
                    size_t name_length = 0;
                    int valid = 1;
                    while (cursor + 1 < offset &&
                           !(source->data[cursor] == '*' &&
                             source->data[cursor + 1] == '/')) {
                        int high, low;
                        char a = source->data[cursor++];
                        char b = cursor < offset ? source->data[cursor++] : '\0';
                        high = a >= '0' && a <= '9' ? a - '0' :
                               a >= 'a' && a <= 'f' ? a - 'a' + 10 :
                               a >= 'A' && a <= 'F' ? a - 'A' + 10 : -1;
                        low = b >= '0' && b <= '9' ? b - '0' :
                              b >= 'a' && b <= 'f' ? b - 'a' + 10 :
                              b >= 'A' && b <= 'F' ? b - 'A' + 10 : -1;
                        if (high < 0 || low < 0 ||
                            name_length + 1 >= mapped_name_size) {
                            valid = 0;
                            break;
                        }
                        mapped_name[name_length++] = (char)((high << 4) | low);
                    }
                    if (valid && cursor + 1 < offset) {
                        mapped_name[name_length] = '\0';
                        filename = mapped_name;
                    }
                }
            }
            while (i < offset && source->data[i] != '\n') ++i;
            if (i < offset) ++i;
        }
    }
    return filename;
}

/**
 * @brief Reports a source diagnostic with severity and mapped source context.
 * @param source Source file used to find the line and preprocessor-mapped filename.
 * @param code Stable diagnostic code.
 * @param message Human-readable diagnostic message.
 * @param location Location associated with the diagnostic.
 * @param severity Diagnostic severity to report.
 * @return No value; writes the diagnostic to standard error.
 */
static void report_severity(
    const SourceFile *source,
    const char *code,
    const char *message,
    SourceLocation location,
    DiagnosticSeverity severity
) {
    char mapped_name[4096];
    const char *line = NULL;
    size_t line_length = 0;
    if (source && source->data && location.line && location.offset <= source->length) {
        size_t start = location.offset, end = location.offset;
        while (start > 0 && source->data[start - 1] != '\n') --start;
        while (end < source->length && source->data[end] != '\n' &&
               source->data[end] != '\r') ++end;
        line = source->data + start;
        line_length = end - start;
    }
    Diagnostic diagnostic = {
        .tool = "crunch-ir",
        .filename = mapped_source_filename(source, location.offset, mapped_name,
                                           sizeof(mapped_name)),
        .code = code,
        .message = message,
        .severity = severity,
        .source_location = location,
        .source_line = line,
        .source_line_length = line_length
    };
    diagnostic_report(stderr, &diagnostic);
}

/**
 * @brief Reports an error diagnostic with mapped source context.
 * @param source Source file used to resolve the mapped filename and source line.
 * @param code Stable diagnostic code.
 * @param message Human-readable diagnostic message.
 * @param location Location associated with the error.
 * @return No value; writes an error diagnostic to standard error.
 */
static void report(
    const SourceFile *source,
    const char *code,
    const char *message,
    SourceLocation location
) {
    report_severity(source, code, message, location, DIAGNOSTIC_ERROR);
}

/**
 * @brief Returns a readable description for a token kind.
 * @param kind Token, AST-node, or type kind to process.
 * @return Readable token description, or "unknown" for an unrecognized token kind.
 */
static const char *token_description(TokenKind kind)
{
    switch (kind) {
    case TOKEN_EOF: return "end of file";
    case TOKEN_IDENTIFIER: return "identifier";
    case TOKEN_INTEGER_LITERAL: return "integer literal";
    case TOKEN_REAL_LITERAL: return "real literal";
    case TOKEN_STRING_LITERAL: return "string literal";
    case TOKEN_CHAR_LITERAL: return "character literal";
    case TOKEN_WIDE_CHAR_LITERAL: return "wide character literal";
    case TOKEN_WIDE_STRING_LITERAL: return "wide string literal";
    default: return token_kind_name(kind);
    }
}

/**
 * @brief Reports parser errors from collected tokens and locations.
 * @param source Source buffer used for diagnostic lines and mapped filenames.
 * @param tokens Token array produced by the lexer.
 * @param locations Array of parser-error source locations to report.
 * @param location_capacity Number of valid entries in locations.
 * @param error_count Number of elements or records.
 * @return No value; the function performs the documented output or state update.
 */
static void report_parser_errors(
    const SourceFile *source,
    const TokenArray *tokens,
    const SourceLocation *locations,
    size_t location_capacity,
    size_t error_count
) {
    for (size_t i = 0; i < error_count && i < location_capacity; ++i) {
        const Token *unexpected = NULL;
        for (size_t j = 0; j < tokens->count; ++j) {
            if (tokens->items[j].location.offset == locations[i].offset) {
                unexpected = &tokens->items[j];
                break;
            }
        }
        char message[128];
        snprintf(message, sizeof message, "unexpected %s",
                 unexpected ? token_description(unexpected->kind) : "token");
        report(source, "E_PARSE", message, locations[i]);
    }
    if (error_count > location_capacity) {
        char message[128];
        snprintf(message, sizeof message,
                 "%zu additional parser error%s not reported",
                 error_count - location_capacity,
                 error_count - location_capacity == 1 ? "" : "s");
        report(source, "E_PARSE_TRUNCATED", message, (SourceLocation){0});
    }
}

/**
 * @brief Parses options and runs the source-to-IR compiler driver.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return Process exit status: zero for success and nonzero for failure.
 */
int main(int argc, char **argv)
{
    SourceFile source = {0};
    TokenArray tokens = {0};
    AstProgram *ast = NULL;
    SemanticModel *model = NULL;
    CompilerStatus status;
    char error[256] = {0};
    Diagnostic ir_diagnostic = {0};
    SourceLocation *parse_locations = NULL;
    size_t parse_capacity = 0;
    size_t parse_error_count = 0;
    bool parse_nesting_limit_exceeded = false;
    int had_frontend_diagnostic = 0;
    int module = 0, c11_numeric = 1, warn_unused_variable = 0, warn_case_fall_through = 0;
    const char *path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--module"))
            module = 1;
        else if (!strcmp(argv[i], "--c11-numeric"))
            c11_numeric = 1;
        else if (!strcmp(argv[i], "-Wunused-variable"))
            warn_unused_variable = 1;
        else if (!strcmp(argv[i], "-Wcase-fall-through"))
            warn_case_fall_through = 1;
        else if (!strcmp(argv[i], "-Wall")) {
            warn_unused_variable = 1;
            warn_case_fall_through = 1;
        }
        else if (!path && (argv[i][0] != '-' || !strcmp(argv[i], "-")))
            path = argv[i];
        else {
            fprintf(stderr, "usage: crunch-ir [--module] [--c11-numeric] [-Wunused-variable] [-Wcase-fall-through] [-Wall] [source.bc|-]\n");
            return 2;
        }
    }

    status = source_load(path ? path : "-", &source);
    if (status != COMPILER_OK) {
        report(&source, status == COMPILER_OUT_OF_MEMORY ? "E_OUT_OF_MEMORY" :
               status == COMPILER_IO_ERROR ? "E_IO" : "E_INPUT",
               status == COMPILER_OUT_OF_MEMORY ? "out of memory reading source" :
               status == COMPILER_IO_ERROR ? "could not read source" :
               "invalid source input", (SourceLocation){0});
        return 2;
    }

    status = lexer_run(&source, &tokens);
    if (status == COMPILER_LEX_ERROR) {
        for (size_t i = 0; i < tokens.count; ++i) {
            if (tokens.items[i].kind == TOKEN_INVALID) {
                static const char gcc_pragma[] = "#pragma GCC ";
                int is_pragma = tokens.items[i].length >= sizeof(gcc_pragma) - 1 &&
                    !memcmp(tokens.items[i].lexeme, gcc_pragma, sizeof(gcc_pragma) - 1);
                report(&source, is_pragma ? "E_UNSUPPORTED_PRAGMA" : "E_LEX_INVALID_TOKEN",
                       is_pragma ? "unsupported GCC pragma in compilation" : "invalid token",
                       tokens.items[i].location);
                had_frontend_diagnostic = 1;
            }
        }
    } else if (status == COMPILER_OK) {
        if (tokens.count > SIZE_MAX / 4 / sizeof(*parse_locations)) {
            status = COMPILER_OUT_OF_MEMORY;
        } else {
            parse_capacity = tokens.count * 4;
            parse_locations = calloc(parse_capacity, sizeof(*parse_locations));
            if (!parse_locations) status = COMPILER_OUT_OF_MEMORY;
        }
        if (status == COMPILER_OK) {
            status = parser_run_with_errors_and_depth(
                &tokens, &ast, parse_locations, parse_capacity,
                &parse_error_count, &parse_nesting_limit_exceeded);
            if (status == COMPILER_PARSE_ERROR) {
                if (parse_nesting_limit_exceeded) {
                    SourceLocation location = {0};
                    if (parse_error_count && parse_capacity) {
                        size_t index = parse_error_count < parse_capacity
                            ? parse_error_count - 1 : parse_capacity - 1;
                        location = parse_locations[index];
                    }
                    report(&source, "E_PARSE_DEPTH",
                           "maximum parser nesting depth exceeded", location);
                } else {
                    report_parser_errors(&source, &tokens, parse_locations,
                                         parse_capacity, parse_error_count);
                }
                had_frontend_diagnostic = 1;
            }
        }
    }

    if (status == COMPILER_OK) {
        ast->c11_numeric = c11_numeric != 0;
        status = sema_run_with_warning_options(ast, warn_unused_variable != 0,
                                               warn_case_fall_through != 0, &model);
        if ((status == COMPILER_SEMA_ERROR || status == COMPILER_OK) && model) {
            for (size_t i = 0; i < sema_diagnostic_count(model); ++i) {
                const SemanticDiagnostic *diagnostic = sema_diagnostic_at(model, i);
                if (diagnostic->warning) {
                    report_severity(&source, "W_SEMANTIC", diagnostic->message,
                                    diagnostic->location, DIAGNOSTIC_WARNING);
                    continue;
                }
                report(&source, "E_SEMANTIC", diagnostic->message,
                       diagnostic->location);
                had_frontend_diagnostic = 1;
            }
        }
    }
    if (status == COMPILER_OK)
        status = module
            ? crunch_ir_emit_module_with_diagnostic(ast, stdout, &ir_diagnostic,
                                                     error, sizeof error)
            : crunch_ir_emit_with_diagnostic(ast, stdout, &ir_diagnostic, error, sizeof error);

    if (status != COMPILER_OK && !had_frontend_diagnostic) {
        if (ir_diagnostic.message) {
            ir_diagnostic.tool = "crunch-ir";
            char mapped_name[4096];
            ir_diagnostic.filename = mapped_source_filename(
                &source, ir_diagnostic.source_location.offset, mapped_name,
                sizeof(mapped_name));
            diagnostic_report(stderr, &ir_diagnostic);
        } else {
            report(&source, status == COMPILER_OUT_OF_MEMORY ? "E_OUT_OF_MEMORY" :
                   status == COMPILER_IO_ERROR ? "E_IO" : "E_COMPILER",
                   status == COMPILER_OUT_OF_MEMORY ? "out of memory" :
                   status == COMPILER_IO_ERROR ? "failed to write IR" :
                   "compiler stage failed", (SourceLocation){0});
        }
    }

    free(parse_locations);
    sema_destroy(model);
    parser_destroy(ast);
    lexer_destroy(&tokens);
    source_destroy(&source);
    return status == COMPILER_OK ? 0 : 1;
}
