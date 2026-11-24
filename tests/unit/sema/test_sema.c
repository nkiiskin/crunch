#define _POSIX_C_SOURCE 200809L
#include "common/source.h"
#include "common/constant_expr.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Looks up a structure in a parsed program.
 * @param context Parsed program.
 * @param name Structure name.
 * @return Matching declaration, or NULL.
 */
static const Decl *find_structure(const void *context, const char *name) {
    const AstProgram *program = context;
    for (size_t i = 0; i < program->declarations.count; ++i) {
        const Decl *decl = program->declarations.items[i];
        if (decl && decl->kind == DECL_STRUCT && decl->as.structure.name &&
            strcmp(decl->as.structure.name, name) == 0)
            return decl;
    }
    return NULL;
}

/**
 * @brief Runs semantic analysis for one fixture and checks its expected result.
 * @param argc Number of command-line arguments; expects fixture and expectation.
 * @param argv Fixture path and expect-ok or expect-error mode.
 * @return Zero when the fixture matches the expectation; otherwise nonzero.
 */
int main(int argc, char **argv) {
    SourceFile source = {0}; TokenArray tokens = {0}; AstProgram *program = NULL;
    SemanticModel *model = NULL; CompilerStatus status; int expect_ok;
    if (argc != 3) { fprintf(stderr, "usage: test-sema file.bc expect-ok|expect-ok-repeat|expect-error|expect-error-repeat|expect-ok-layout|expect-ok-plan\n"); return 2; }
    expect_ok = strcmp(argv[2], "expect-ok") == 0 || strcmp(argv[2], "expect-ok-layout") == 0 ||
                strcmp(argv[2], "expect-ok-plan") == 0 || strcmp(argv[2], "expect-ok-repeat") == 0;
    status = source_load(argv[1], &source);
    if (status != COMPILER_OK) { fprintf(stderr, "cannot load source\n"); return 2; }
    status = lexer_run(&source, &tokens);
    if (status != COMPILER_OK) { fprintf(stderr, "lexer failed\n"); source_destroy(&source); return 2; }
    status = parser_run(&tokens, &program);
    if (status != COMPILER_OK) { fprintf(stderr, "parser failed for %s\n", argv[1]); lexer_destroy(&tokens); source_destroy(&source); return 2; }
    status = sema_run(program, &model);
    for (size_t i = 0; i < sema_diagnostic_count(model); ++i) {
        const SemanticDiagnostic *d = sema_diagnostic_at(model, i);
        fprintf(stderr, "%u:%u: %s\n", d->location.line, d->location.column, d->message);
    }
    int pass = expect_ok ? status == COMPILER_OK : status == COMPILER_SEMA_ERROR && sema_diagnostic_count(model) > 0;
    if (!pass) fprintf(stderr, "unexpected semantic status %d with %zu diagnostics\n", (int)status, sema_diagnostic_count(model));
    if (pass && strcmp(argv[2], "expect-ok-layout") == 0) {
        for (size_t i = 0; i < program->declarations.count; ++i) {
            const Decl *decl = program->declarations.items[i];
            FieldLayout fields[256];
            size_t size, alignment;
            if (!decl || decl->kind != DECL_STRUCT)
                continue;
            if (!type_layout_struct_fields(decl, find_structure, program, fields, &size,
                                           &alignment)) {
                fprintf(stderr, "cannot lay out %s\n", decl->as.structure.name);
                pass = 0;
                break;
            }
            printf("%s %zu %zu", decl->as.structure.name, size, alignment);
            for (size_t j = 0; j < decl->as.structure.field_count; ++j)
                printf(" %zu:%u:%u", fields[j].offset, fields[j].bit_offset,
                       fields[j].bit_width);
            putchar('\n');
        }
    }
    if (pass && strcmp(argv[2], "expect-ok-plan") == 0) {
        for (size_t i = 0; i < program->declarations.count; ++i) {
            const Decl *decl = program->declarations.items[i];
            if (!decl || decl->kind != DECL_VARIABLE ||
                !decl->as.variable.initializer.semantic_plan)
                continue;
            const InitializerPlan *plan = decl->as.variable.initializer.semantic_plan;
            printf("%s", decl->as.variable.name);
            for (size_t j = 0; j < plan->count; ++j) {
                const InitializerOperation *item = &plan->items[j];
                printf(" %zu:%zu:%u:%u:%u:%u", item->offset, item->size,
                       item->clear, item->bit_field, item->bit_offset, item->bit_width);
            }
            putchar('\n');
        }
    }
    sema_destroy(model);
    if (pass && (strcmp(argv[2], "expect-error-repeat") == 0 ||
                 strcmp(argv[2], "expect-ok-repeat") == 0)) {
        model = NULL;
        status = sema_run(program, &model);
        pass = expect_ok ? status == COMPILER_OK
                         : status == COMPILER_SEMA_ERROR && sema_diagnostic_count(model) > 0;
        if (!pass) fprintf(stderr, "repeated analysis unexpectedly returned status %d\n", (int)status);
        sema_destroy(model);
    }
    parser_destroy(program); lexer_destroy(&tokens); source_destroy(&source);
    return pass ? 0 : 1;
}
