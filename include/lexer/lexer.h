#ifndef CRUNCH_LEXER_LEXER_H
#define CRUNCH_LEXER_LEXER_H

#include "common/compiler.h"
#include "lexer/token.h"

/**
 * @brief Tokenizes a loaded source file.
 * @param source Loaded source; its buffer must remain alive while tokens are used.
 * @param output Output token array, populated on success.
 * @return COMPILER_OK on success, otherwise the applicable input, lexical, or memory error.
 */
CompilerStatus lexer_run(
    const SourceFile *source,
    TokenArray *output
);

/**
 * @brief Releases token-array storage.
 * @param tokens Token array returned by lexer_run.
 * @return No value.
 */
void lexer_destroy(TokenArray *tokens);

#endif
