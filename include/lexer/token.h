#ifndef CRUNCH_LEXER_TOKEN_H
#define CRUNCH_LEXER_TOKEN_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/compiler.h"

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_INVALID,

    /* Identifiers and literals. */
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER_LITERAL,
    TOKEN_REAL_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,

    /* Types and control-flow keywords. */
    TOKEN_VOID,
    TOKEN_INT,
    TOKEN_LONG,
    TOKEN_UNSIGNED,
    TOKEN_BYTE,
    TOKEN_REAL,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_FOR,
    TOKEN_LOOP,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_RETURN,
    TOKEN_SIZEOF,
    TOKEN_STRUCT,
    TOKEN_PRINT,
    TOKEN_LENGTH,
    TOKEN_NORETURN,
    TOKEN_CONST,
    TOKEN_STATIC,
    TOKEN_EXTERN,
    TOKEN_DEF,

    /* Delimiters. */
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_DOT,

    /* Arithmetic operators. */
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    /* Assignment and comparison. */
    TOKEN_ASSIGN,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,

    /* Logical operators. */
    TOKEN_LOGICAL_AND,
    TOKEN_LOGICAL_OR,
    TOKEN_LOGICAL_NOT,

    /* Bitwise operators. */
    TOKEN_BITWISE_AND,
    TOKEN_BITWISE_OR,
    TOKEN_BITWISE_XOR,
    TOKEN_BITWISE_NOT,
    TOKEN_SHIFT_LEFT,
    TOKEN_SHIFT_RIGHT,

    /* Increment and decrement. */
    TOKEN_INCREMENT,
    TOKEN_DECREMENT,

    /* Compound assignments. */
    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,
    TOKEN_STAR_ASSIGN,
    TOKEN_SLASH_ASSIGN,
    TOKEN_PERCENT_ASSIGN,
    TOKEN_AND_ASSIGN,
    TOKEN_OR_ASSIGN,
    TOKEN_XOR_ASSIGN,
    TOKEN_SHIFT_LEFT_ASSIGN,
    TOKEN_SHIFT_RIGHT_ASSIGN,

    /* Conditional expression punctuation. */
    TOKEN_QUESTION,
    TOKEN_COLON,

    /* Preprocessor marker. */
    TOKEN_HASH,
    TOKEN_ARROW,
    TOKEN_FLOAT,
    TOKEN_SHORT,
    TOKEN_SIGNED,
    TOKEN_BOOL,
    TOKEN_ENUM,
    TOKEN_VOLATILE,
    TOKEN_REGISTER,
    TOKEN_INLINE,
    TOKEN_GOTO,
    TOKEN_UNION,
    TOKEN_DO,
    TOKEN_STATIC_ASSERT,
    TOKEN_ALIGNOF,
    TOKEN_ALIGNAS,
    TOKEN_THREAD_LOCAL,
    TOKEN_RESTRICT,
    TOKEN_WIDE_CHAR_LITERAL,
    TOKEN_AUTO,
    TOKEN_WIDE_STRING_LITERAL
} TokenKind;

typedef struct {
    TokenKind kind;
    SourceLocation location;

    /*
     * Slice into the original SourceFile.data buffer.
     * The source buffer must outlive all tokens referencing it.
     */
    const char *lexeme;
    size_t length;
    unsigned int pack_alignment;
} Token;

typedef struct {
    Token *items;
    size_t count;
} TokenArray;

/**
 * @brief Returns the textual name of a token kind.
 * @param kind Token kind to name.
 * @return Static token name, including a fallback for unknown values.
 */
const char *token_kind_name(TokenKind kind);

/**
 * @brief Identifies a narrow literal with a variable-length hex escape.
 * @param token Token to inspect.
 * @return True when a hex escape has other than two digits.
 */
bool token_has_variable_hex_escape(const Token *token);

/**
 * @brief Decodes a complete wide character token as a signed 32-bit value.
 * @param text Literal spelling, including prefix and quotes.
 * @param length Length of the spelling in bytes.
 * @param value Output value on success.
 * @return True for a supported, well-formed literal.
 */
bool token_decode_wide_character(const char *text, size_t length, int32_t *value);

/**
 * @brief Decodes one wide literal element from its unquoted spelling.
 * @param text Element spelling.
 * @param length Available bytes.
 * @param quote Enclosing quote character.
 * @param consumed Receives the number of bytes consumed.
 * @param value Receives the target wchar_t value.
 * @return True for a supported, well-formed element.
 */
bool token_decode_wide_element(const char *text, size_t length, char quote,
                               size_t *consumed, int32_t *value);

#endif
