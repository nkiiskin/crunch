#ifndef CRUNCH_PARSER_PARSER_H
#define CRUNCH_PARSER_PARSER_H

#include "common/compiler.h"
#include "lexer/token.h"
#include "parser/ast.h"

/**
 * @brief Parses a token array into an AST program.
 * @param tokens Token array produced by the lexer, ending with exactly one EOF token.
 * @param output Receives the AST on success; remains NULL on failure.
 * @return COMPILER_OK when parsing succeeds, otherwise the applicable input, parse, or memory error.
 */
CompilerStatus parser_run(
    const TokenArray *tokens,
    AstProgram **output
);

/**
 * @brief Parses tokens and optionally reports the first syntax-error location.
 * @param tokens Token array produced by the lexer, ending with exactly one EOF token.
 * @param output Receives the AST on success; remains NULL on failure.
 * @param error_location Optional output receiving the first syntax-error location.
 * @return COMPILER_OK when parsing succeeds, otherwise the applicable input, parse, or memory error.
 */
CompilerStatus parser_run_with_error_location(
    const TokenArray *tokens,
    AstProgram **output,
    SourceLocation *error_location
);

/**
 * @brief Parses tokens and collects syntax-error locations.
 * @param tokens Token array produced by the lexer, ending with exactly one EOF token.
 * @param output Receives the AST on success; remains NULL on failure.
 * @param error_locations Optional array receiving up to error_capacity locations.
 * @param error_capacity Number of locations that fit in error_locations.
 * @param error_count Optional output receiving the total number of syntax errors.
 * @return COMPILER_OK when parsing succeeds, otherwise the applicable input, parse, or memory error.
 */
CompilerStatus parser_run_with_errors(
    const TokenArray *tokens,
    AstProgram **output,
    SourceLocation *error_locations,
    size_t error_capacity,
    size_t *error_count
);

/**
 * @brief Parses tokens and reports whether a nesting limit was exceeded.
 * @param tokens Token array produced by the lexer, ending with exactly one EOF token.
 * @param output Receives the AST on success; remains NULL on failure.
 * @param error_locations Optional array receiving syntax-error locations.
 * @param error_capacity Number of locations that fit in error_locations.
 * @param error_count Optional output receiving the total syntax-error count.
 * @param nesting_limit_exceeded Optional output set when parser nesting exceeds a limit.
 * @return COMPILER_OK when parsing succeeds, otherwise the applicable input, parse, or memory error.
 */
CompilerStatus parser_run_with_errors_and_depth(
    const TokenArray *tokens,
    AstProgram **output,
    SourceLocation *error_locations,
    size_t error_capacity,
    size_t *error_count,
    bool *nesting_limit_exceeded
);

/**
 * @brief Releases an AST program and all of its owned data.
 * @param program AST program returned by a successful parser call.
 * @return No value.
 */
void parser_destroy(AstProgram *program);

#endif
