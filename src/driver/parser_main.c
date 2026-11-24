#define _POSIX_C_SOURCE 200809L
#include "parser/parser.h"
#include "parser/ast_json.h"
#include "lexer/token.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Maps a token-kind spelling to its enum value.
 * @param name Identifier name to search for or emit.
 * @return Enum value matching the input, or the documented invalid/fallback value.
 */
static TokenKind token_kind_from_name(const char *name) {
    int k;
    for (k = TOKEN_EOF; k <= TOKEN_WIDE_STRING_LITERAL; ++k)
        if (strcmp(token_kind_name((TokenKind)k), name) == 0) return (TokenKind)k;
    return TOKEN_INVALID;
}
/**
 * @brief Releases lexeme buffers in a partially read token array.
 * @param items Token records whose lexemes are owned by this reader.
 * @param count Number of initialized token records in items.
 * @return No value; releases each lexeme and the array storage.
 */
static void free_token_items(Token *items, size_t count) {
    for (size_t i = 0; i < count; ++i)
        free((void *)items[i].lexeme);
    free(items);
}

/**
 * @brief Parses an unsigned decimal field without accepting signs or overflow.
 * @param text Decimal field to parse.
 * @param maximum Largest permitted value.
 * @param value Output receiving the parsed value.
 * @return One when the field is a valid in-range decimal number; otherwise zero.
 */
static int parse_decimal(const char *text, uintmax_t maximum, uintmax_t *value) {
    char *end;
    uintmax_t parsed;
    if (!text || !*text) return 0;
    for (const char *p = text; *p; ++p)
        if (*p < '0' || *p > '9') return 0;
    errno = 0;
    parsed = strtoumax(text, &end, 10);
    if (errno == ERANGE || end == text || *end || parsed > maximum) return 0;
    *value = parsed;
    return 1;
}

/**
 * @brief Converts one hexadecimal digit to its numeric value.
 * @param c Character to decode.
 * @return Digit value from zero through fifteen, or -1 if c is not hexadecimal.
 */
static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief Preserves the fixed-width hex semantics of older token streams.
 * @param token Token whose owned lexeme may be rewritten in place.
 * @return One on success, or zero for an incomplete hexadecimal escape.
 */
static int normalize_legacy_hex(Token *token)
{
    if (token->kind != TOKEN_STRING_LITERAL && token->kind != TOKEN_CHAR_LITERAL)
        return 1;
    char *text = (char *)token->lexeme;
    size_t n = token->length;
    for (size_t i = 1; i + 1 < n; ++i) {
        if (text[i] != '\\') continue;
        if (++i + 1 >= n) break;
        if (text[i] == 'x') {
            if (i + 2 >= n - 1 || hex_value(text[i + 1]) < 0 ||
                hex_value(text[i + 2]) < 0)
                return 0;
            if (i + 3 < n - 1 && hex_value(text[i + 3]) >= 0) {
                unsigned value = (unsigned)(hex_value(text[i + 1]) * 16 +
                                            hex_value(text[i + 2]));
                text[i] = (char)('0' + (value >> 6));
                text[i + 1] = (char)('0' + ((value >> 3) & 7));
                text[i + 2] = (char)('0' + (value & 7));
            }
            i += 2;
        }
    }
    return 1;
}

/**
 * @brief Reads and validates the token interchange format.
 * @param out Token array being populated, read, or released.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int read_tokens(TokenArray *out) {
    char *line = NULL;
    size_t cap = 0, count = 0, alloc = 0, previous_offset = 0;
    ssize_t n;
    Token *items = NULL;
    int have_previous_offset = 0, have_eof = 0, version;
    if (!out) return 0;
    *out = (TokenArray){0};
    n = getline(&line, &cap, stdin);
    if (n < 0 || (strcmp(line, "CRUNCH-TOKENS 1\n") != 0 &&
                  strcmp(line, "CRUNCH-TOKENS 2\n") != 0 &&
                  strcmp(line, "CRUNCH-TOKENS 3\n") != 0 &&
                  strcmp(line, "CRUNCH-TOKENS 4\n") != 0 &&
                  strcmp(line, "CRUNCH-TOKENS 5\n") != 0 &&
                  strcmp(line, "CRUNCH-TOKENS 6\n") != 0)) {
        free(line);
        return 0;
    }
    version = line[14] - '0';
    while ((n = getline(&line, &cap, stdin)) >= 0) {
        char *fields[6], *cursor;
        size_t field_count = version >= 2 ? 6 : 5;
        size_t bytes = 0;
        uintmax_t offset, source_line, column, alignment = 0;
        Token token = {0};
        if (memchr(line, '\0', (size_t)n)) goto invalid;
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        cursor = line;
        for (size_t i = 0; i < field_count; ++i) {
            char *tab;
            fields[i] = cursor;
            tab = strchr(cursor, '\t');
            if (i + 1 < field_count) {
                if (!tab) goto invalid;
                *tab = '\0';
                cursor = tab + 1;
            } else if (tab) {
                goto invalid;
            }
        }
        if (have_eof) goto invalid;
        token.kind = token_kind_from_name(fields[0]);
        if (version < 5 && token.kind == TOKEN_WIDE_STRING_LITERAL) goto invalid;
        if (version < 4 && token.kind == TOKEN_AUTO) goto invalid;
        if (version < 3 && token.kind == TOKEN_WIDE_CHAR_LITERAL) goto invalid;
        if (token.kind == TOKEN_INVALID && strcmp(fields[0], "INVALID") != 0)
            goto invalid;
        if (!parse_decimal(fields[1], SIZE_MAX, &offset) ||
            !parse_decimal(fields[2], UINT32_MAX, &source_line) ||
            !parse_decimal(fields[3], UINT32_MAX, &column) ||
            (version >= 2 && !parse_decimal(fields[5], 16, &alignment)) ||
            (alignment != 0 && alignment != 1 && alignment != 2 &&
             alignment != 4 && alignment != 8 && alignment != 16) ||
            source_line == 0 || column == 0 ||
            (have_previous_offset && offset <= previous_offset))
            goto invalid;
        token.location.offset = (size_t)offset;
        token.location.line = (uint32_t)source_line;
        token.location.column = (uint32_t)column;
        token.pack_alignment = (unsigned int)alignment;

        if (strcmp(fields[4], "-") == 0) {
            if (token.kind != TOKEN_EOF) goto invalid;
            token.lexeme = (const char *)calloc(1, 1);
            if (!token.lexeme) goto invalid;
        } else {
            size_t hex_length = strlen(fields[4]);
            if (token.kind == TOKEN_EOF || hex_length == 0 || hex_length % 2 != 0)
                goto invalid;
            bytes = hex_length / 2;
            if (bytes == SIZE_MAX) goto invalid;
            token.lexeme = (const char *)malloc(bytes + 1);
            if (!token.lexeme) goto invalid;
            for (size_t j = 0; j < bytes; ++j) {
                int high = hex_value(fields[4][j * 2]);
                int low = hex_value(fields[4][j * 2 + 1]);
                if (high < 0 || low < 0) {
                    free((void *)token.lexeme);
                    goto invalid;
                }
                ((char *)token.lexeme)[j] = (char)((high << 4) | low);
            }
            ((char *)token.lexeme)[bytes] = '\0';
            if (token.kind != TOKEN_INVALID && memchr(token.lexeme, '\0', bytes)) {
                free((void *)token.lexeme);
                goto invalid;
            }
        }
        token.length = bytes;
        if (version < 6 && !normalize_legacy_hex(&token)) {
            free((void *)token.lexeme);
            goto invalid;
        }
        if (count == alloc) {
            size_t next;
            Token *grown;
            if (alloc > SIZE_MAX / 2) {
                free((void *)token.lexeme);
                goto invalid;
            }
            next = alloc ? alloc * 2 : 128;
            if (next > SIZE_MAX / sizeof(*items)) {
                free((void *)token.lexeme);
                goto invalid;
            }
            grown = (Token *)realloc(items, next * sizeof(*items));
            if (!grown) {
                free((void *)token.lexeme);
                goto invalid;
            }
            items = grown;
            alloc = next;
        }
        items[count++] = token;
        previous_offset = (size_t)offset;
        have_previous_offset = 1;
        have_eof = token.kind == TOKEN_EOF;
    }
    free(line);
    if (ferror(stdin) || count == 0 || !have_eof) {
        free_token_items(items, count);
        return 0;
    }
    out->items = items;
    out->count = count;
    return 1;

invalid:
    free(line);
    free_token_items(items, count);
    return 0;
}
/**
 * @brief Releases token lexemes and token-array storage.
 * @param t Token array being populated, read, or released.
 * @return No value; the function performs the documented output or state update.
 */
static void free_tokens(TokenArray *t){free_token_items(t->items,t->count);t->items=NULL;t->count=0;}
/**
 * @brief Reads token interchange input, parses it, and writes the AST result.
 * @return Process exit status: zero for success and nonzero for failure.
 */
int main(void) {
    TokenArray tokens = {0};
    AstProgram *program = NULL;
    CompilerStatus status;
    bool nesting_limit_exceeded = false;

    if (!read_tokens(&tokens)) {
        fprintf(stderr, "crunch-parser: invalid token stream (expected CRUNCH-TOKENS 1 through 5)\n");
        return 2;
    }
    for (size_t i = 0; i < tokens.count; ++i) {
        if (tokens.items[i].kind == TOKEN_INVALID) {
            fprintf(stderr, "crunch-parser: input contains invalid tokens\n");
            free_tokens(&tokens);
            return 1;
        }
    }
    status = parser_run_with_errors_and_depth(
        &tokens, &program, NULL, 0, NULL, &nesting_limit_exceeded);
    if (status != COMPILER_OK) {
        fprintf(stderr, nesting_limit_exceeded
                ? "crunch-parser: maximum nesting depth exceeded\n"
                : "crunch-parser: syntax error\n");
        free_tokens(&tokens);
        return status == COMPILER_OUT_OF_MEMORY ? 2 : 1;
    }
    if (!ast_write_json(stdout, program)) {
        fprintf(stderr, "crunch-parser: failed to write AST\n");
        parser_destroy(program);
        free_tokens(&tokens);
        return 2;
    }
    parser_destroy(program);
    free_tokens(&tokens);
    return ferror(stdout) ? 2 : 0;
}
