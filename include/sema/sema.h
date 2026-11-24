#ifndef CRUNCH_SEMA_SEMA_H
#define CRUNCH_SEMA_SEMA_H

#include <stddef.h>
#include "common/compiler.h"
#include "parser/ast.h"
#include "sema/symbol.h"

typedef struct SemanticModel SemanticModel;

/**
 * @brief Builds symbol tables and checks declarations, expressions, and scopes.
 * @param program AST program to analyze; retain it and the resulting model through IR generation.
 * @param output Receives the semantic model, including diagnostics on semantic errors;
 *               set to NULL on allocation failure.
 * @return COMPILER_OK on success, COMPILER_SEMA_ERROR when semantic diagnostics exist,
 *         or another status for invalid input or allocation failure.
 */
CompilerStatus sema_run(AstProgram *program, SemanticModel **output);

/**
 * @brief Analyzes a program with optional unused-local-variable diagnostics.
 * @param program AST program to analyze; retain it and the resulting model through IR generation.
 * @param warn_unused_variable Whether to report unused local variables.
 * @param output Receives the semantic model, including diagnostics on semantic errors.
 * @return COMPILER_OK on success, COMPILER_SEMA_ERROR for errors, or another failure status.
 */
CompilerStatus sema_run_with_warnings(AstProgram *program, bool warn_unused_variable,
                                     SemanticModel **output);

/**
 * @brief Analyzes a program with configurable optional warnings.
 * @param program AST program to analyze; retain it and the resulting model through IR generation.
 * @param warn_unused_variable Whether to report unused local variables.
 * @param warn_case_fall_through Whether to report switch case fall-through.
 * @param output Receives the semantic model, including diagnostics on semantic errors.
 * @return COMPILER_OK on success, COMPILER_SEMA_ERROR for errors, or another failure status.
 */
CompilerStatus sema_run_with_warning_options(AstProgram *program, bool warn_unused_variable,
                                            bool warn_case_fall_through, SemanticModel **output);

/**
 * @brief Evaluates a semantically checked integer constant expression.
 * @param expression Checked expression to evaluate.
 * @param bits Receives the canonical two's-complement or unsigned value.
 * @param kind Receives the expression's integer type.
 * @return True when the expression is an evaluable integer constant.
 */
bool sema_integer_constant(const Expr *expression, uint64_t *bits, TypeKind *kind);

/**
 * @brief Evaluates a semantically checked numeric constant expression.
 * @param expression Checked expression to evaluate.
 * @param value Receives the numeric value.
 * @return True when the expression is an evaluable numeric constant.
 */
bool sema_numeric_constant(const Expr *expression, long double *value);

/**
 * @brief Evaluates a numeric constant converted to an integer storage type.
 * @param expression Checked expression to evaluate.
 * @param target Integer destination type.
 * @param bits Receives the destination's canonical integer bits.
 * @return True when the conversion can be evaluated.
 */
bool sema_constant_integer_conversion(const Expr *expression, TypeKind target, uint64_t *bits);

/**
 * @brief Converts a numeric value to round-to-nearest binary32 bits.
 * @param value Numeric value to convert.
 * @return Raw IEEE-754 binary32 bits.
 */
uint32_t sema_binary32_bits(long double value);

/**
 * @brief Releases a semantic model and its diagnostics.
 * @param model Semantic model returned by sema_run.
 * @return No value.
 */
void sema_destroy(SemanticModel *model);

/**
 * @brief Returns the number of diagnostics in a semantic model.
 * @param model Semantic model to query.
 * @return Diagnostic count, or zero when model is NULL.
 */
size_t sema_diagnostic_count(const SemanticModel *model);

/**
 * @brief Returns a semantic diagnostic by zero-based index.
 * @param model Semantic model to query.
 * @param index Zero-based diagnostic index.
 * @return Diagnostic pointer, or NULL when model is NULL or index is out of range.
 */
const SemanticDiagnostic *sema_diagnostic_at(const SemanticModel *model, size_t index);

/**
 * @brief Returns the number of global symbols in a semantic model.
 * @param model Semantic model to query.
 * @return Global-symbol count, or zero when model is NULL.
 */
size_t sema_global_symbol_count(const SemanticModel *model);

#endif
