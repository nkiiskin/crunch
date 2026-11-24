#include "common/source.h"
#include "lexer/lexer.h"
#include "lexer/token.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Writes tokens in the parser-driver interchange format.
 * @param tokens Token array produced by the lexer.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int write_tokens(const TokenArray *tokens)
{
    size_t i, j;
    int version = 1;
    for (i = 0; i < tokens->count; ++i) {
        if (token_has_variable_hex_escape(&tokens->items[i])) version = 6;
        else if (tokens->items[i].kind == TOKEN_WIDE_STRING_LITERAL && version < 5) version = 5;
        else if (tokens->items[i].kind == TOKEN_AUTO && version < 4) version = 4;
        else if (tokens->items[i].kind == TOKEN_WIDE_CHAR_LITERAL && version < 3) version = 3;
        else if (tokens->items[i].pack_alignment && version < 2) version = 2;
    }
    if (printf("CRUNCH-TOKENS %d\n", version) < 0) return 0;
    for (i = 0; i < tokens->count; ++i) {
        const Token *t = &tokens->items[i];
        if (printf("%s\t%zu\t%u\t%u\t", token_kind_name(t->kind),
                   t->location.offset, t->location.line, t->location.column) < 0) return 0;
        if (t->length == 0) {
            if (putchar('-') == EOF) return 0;
        } else {
            for (j = 0; j < t->length; ++j)
                if (printf("%02x", (unsigned char)t->lexeme[j]) < 0) return 0;
        }
        if (version >= 2 && printf("\t%u", t->pack_alignment) < 0) return 0;
        if (putchar('\n') == EOF) return 0;
    }
    return !ferror(stdout);
}

/**
 * @brief Parses options and runs the lexer driver.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return Process exit status: zero for success and nonzero for failure.
 */
int main(int argc, char **argv)
{
    SourceFile source;
    TokenArray tokens;
    CompilerStatus status;
    const char *path = "-";
    if (argc > 2) {
        fprintf(stderr, "usage: crunch-lexer [source-file|-]\n");
        return 2;
    }
    if (argc == 2) path = argv[1];
    status = source_load(path, &source);
    if (status != COMPILER_OK) {
        fprintf(stderr, "crunch-lexer: could not read input '%s'\n", path);
        return 2;
    }
    status = lexer_run(&source, &tokens);
    if (status == COMPILER_OUT_OF_MEMORY) {
        fprintf(stderr, "crunch-lexer: out of memory\n");
        source_destroy(&source);
        return 2;
    }
    if (!write_tokens(&tokens)) {
        fprintf(stderr, "crunch-lexer: failed writing token stream\n");
        lexer_destroy(&tokens); source_destroy(&source); return 2;
    }
    if (status == COMPILER_LEX_ERROR) {
        fprintf(stderr, "crunch-lexer: invalid token(s) in input\n");
        lexer_destroy(&tokens); source_destroy(&source); return 1;
    }
    lexer_destroy(&tokens); source_destroy(&source);
    return 0;
}
