#include "lexer/token.h"
#include <ctype.h>

/**
 * @brief Identifies a narrow literal with a variable-length hex escape.
 * @param token Token to inspect.
 * @return True when a hex escape has other than two digits.
 */
bool token_has_variable_hex_escape(const Token *token)
{
    if (!token || !token->lexeme ||
        (token->kind != TOKEN_STRING_LITERAL && token->kind != TOKEN_CHAR_LITERAL))
        return false;
    for (size_t i = 1; i + 1 < token->length; ++i) {
        if (token->lexeme[i] != '\\') continue;
        if (++i + 1 >= token->length) break;
        if (token->lexeme[i] == 'x') {
            size_t start = ++i;
            while (i + 1 < token->length && isxdigit((unsigned char)token->lexeme[i]))
                ++i;
            if (i - start != 2) return true;
            --i;
        }
    }
    return false;
}

/**
 * @brief Returns the printable spelling of a token kind.
 * @param kind Token kind whose printable spelling is requested.
 * @return Static token name, or "UNKNOWN_TOKEN" for an unrecognized kind.
 */
const char *token_kind_name(TokenKind kind)
{
    switch (kind) {
    case TOKEN_EOF: return "EOF";
    case TOKEN_INVALID: return "INVALID";
    case TOKEN_IDENTIFIER: return "IDENTIFIER";
    case TOKEN_INTEGER_LITERAL: return "INTEGER_LITERAL";
    case TOKEN_REAL_LITERAL: return "REAL_LITERAL";
    case TOKEN_STRING_LITERAL: return "STRING_LITERAL";
    case TOKEN_CHAR_LITERAL: return "CHAR_LITERAL";
    case TOKEN_WIDE_CHAR_LITERAL: return "WIDE_CHAR_LITERAL";
    case TOKEN_WIDE_STRING_LITERAL: return "WIDE_STRING_LITERAL";
    case TOKEN_VOID: return "VOID";
    case TOKEN_BOOL: return "BOOL";
    case TOKEN_ENUM: return "ENUM";
    case TOKEN_INT: return "INT";
    case TOKEN_SHORT: return "SHORT";
    case TOKEN_LONG: return "LONG";
    case TOKEN_SIGNED: return "SIGNED";
    case TOKEN_UNSIGNED: return "UNSIGNED";
    case TOKEN_BYTE: return "BYTE";
    case TOKEN_REAL: return "REAL";
    case TOKEN_FLOAT: return "FLOAT";
    case TOKEN_IF: return "IF";
    case TOKEN_ELSE: return "ELSE";
    case TOKEN_SWITCH: return "SWITCH";
    case TOKEN_CASE: return "CASE";
    case TOKEN_DEFAULT: return "DEFAULT";
    case TOKEN_FOR: return "FOR";
    case TOKEN_LOOP: return "LOOP";
    case TOKEN_DO: return "DO";
    case TOKEN_BREAK: return "BREAK";
    case TOKEN_CONTINUE: return "CONTINUE";
    case TOKEN_RETURN: return "RETURN";
    case TOKEN_SIZEOF: return "SIZEOF";
    case TOKEN_ALIGNOF: return "ALIGNOF";
    case TOKEN_ALIGNAS: return "ALIGNAS";
    case TOKEN_THREAD_LOCAL: return "THREAD_LOCAL";
    case TOKEN_STRUCT: return "STRUCT";
    case TOKEN_UNION: return "UNION";
    case TOKEN_PRINT: return "PRINT";
    case TOKEN_LENGTH: return "LENGTH";
    case TOKEN_NORETURN: return "NORETURN";
    case TOKEN_STATIC_ASSERT: return "STATIC_ASSERT";
    case TOKEN_CONST: return "CONST";
    case TOKEN_VOLATILE: return "VOLATILE";
    case TOKEN_RESTRICT: return "RESTRICT";
    case TOKEN_REGISTER: return "REGISTER";
    case TOKEN_AUTO: return "AUTO";
    case TOKEN_INLINE: return "INLINE";
    case TOKEN_GOTO: return "GOTO";
    case TOKEN_STATIC: return "STATIC";
    case TOKEN_EXTERN: return "EXTERN";
    case TOKEN_DEF: return "DEF";
    case TOKEN_LPAREN: return "LPAREN";
    case TOKEN_RPAREN: return "RPAREN";
    case TOKEN_LBRACE: return "LBRACE";
    case TOKEN_RBRACE: return "RBRACE";
    case TOKEN_LBRACKET: return "LBRACKET";
    case TOKEN_RBRACKET: return "RBRACKET";
    case TOKEN_SEMICOLON: return "SEMICOLON";
    case TOKEN_COMMA: return "COMMA";
    case TOKEN_DOT: return "DOT";
    case TOKEN_PLUS: return "PLUS";
    case TOKEN_MINUS: return "MINUS";
    case TOKEN_STAR: return "STAR";
    case TOKEN_SLASH: return "SLASH";
    case TOKEN_PERCENT: return "PERCENT";
    case TOKEN_ASSIGN: return "ASSIGN";
    case TOKEN_EQUAL: return "EQUAL";
    case TOKEN_NOT_EQUAL: return "NOT_EQUAL";
    case TOKEN_LESS: return "LESS";
    case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
    case TOKEN_GREATER: return "GREATER";
    case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
    case TOKEN_LOGICAL_AND: return "LOGICAL_AND";
    case TOKEN_LOGICAL_OR: return "LOGICAL_OR";
    case TOKEN_LOGICAL_NOT: return "LOGICAL_NOT";
    case TOKEN_BITWISE_AND: return "BITWISE_AND";
    case TOKEN_BITWISE_OR: return "BITWISE_OR";
    case TOKEN_BITWISE_XOR: return "BITWISE_XOR";
    case TOKEN_BITWISE_NOT: return "BITWISE_NOT";
    case TOKEN_SHIFT_LEFT: return "SHIFT_LEFT";
    case TOKEN_SHIFT_RIGHT: return "SHIFT_RIGHT";
    case TOKEN_INCREMENT: return "INCREMENT";
    case TOKEN_DECREMENT: return "DECREMENT";
    case TOKEN_PLUS_ASSIGN: return "PLUS_ASSIGN";
    case TOKEN_MINUS_ASSIGN: return "MINUS_ASSIGN";
    case TOKEN_STAR_ASSIGN: return "STAR_ASSIGN";
    case TOKEN_SLASH_ASSIGN: return "SLASH_ASSIGN";
    case TOKEN_PERCENT_ASSIGN: return "PERCENT_ASSIGN";
    case TOKEN_AND_ASSIGN: return "AND_ASSIGN";
    case TOKEN_OR_ASSIGN: return "OR_ASSIGN";
    case TOKEN_XOR_ASSIGN: return "XOR_ASSIGN";
    case TOKEN_SHIFT_LEFT_ASSIGN: return "SHIFT_LEFT_ASSIGN";
    case TOKEN_SHIFT_RIGHT_ASSIGN: return "SHIFT_RIGHT_ASSIGN";
    case TOKEN_QUESTION: return "QUESTION";
    case TOKEN_COLON: return "COLON";
    case TOKEN_HASH: return "HASH";
    case TOKEN_ARROW: return "ARROW";
    }
    return "UNKNOWN_TOKEN";
}


/**
 * @brief Converts an ASCII hexadecimal digit.
 * @param c Input byte.
 * @return The digit value or -1.
 */
static int wide_hex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief Decodes one element of a wide literal.
 * @param text Unquoted element spelling.
 * @param length Available bytes.
 * @param quote Enclosing quote character.
 * @param consumed Receives the number of bytes consumed.
 * @param value Receives the target value.
 * @return True when the element is valid and supported.
 */
bool token_decode_wide_element(const char *text, size_t length, char quote,
                               size_t *consumed, int32_t *value)
{
    const unsigned char *s = (const unsigned char *)text;
    uint32_t v = 0;
    size_t i = 0, end = length;
    if (!s || !value || !consumed || !length || (quote != '\'' && quote != '"'))
        return false;
    if (s[i] == '\\') {
        unsigned char c;
        if (++i == end) return false;
        c = s[i++];
        if (c >= '0' && c <= '7') {
            v = c - '0';
            for (int j = 1; j < 3 && i < end && s[i] >= '0' && s[i] <= '7'; ++j)
                v = v * 8 + (s[i++] - '0');
        } else if (c == 'x') {
            size_t start = i;
            while (i < end && wide_hex(s[i]) >= 0) {
                unsigned digit = (unsigned)wide_hex(s[i++]);
                if (v > (UINT32_MAX - digit) / 16) return false;
                v = v * 16 + digit;
            }
            if (i == start) return false;
        } else if (c == 'u' || c == 'U') {
            size_t digits = c == 'u' ? 4 : 8;
            if (end - i < digits) return false;
            for (size_t j = 0; j < digits; ++j) {
                int digit = wide_hex(s[i++]);
                if (digit < 0) return false;
                v = v * 16 + (unsigned)digit;
            }
            if (v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff) ||
                (v < 0xa0 && v != 0x24 && v != 0x40 && v != 0x60))
                return false;
        } else {
            switch (c) {
            case 'a': v = 7; break; case 'b': v = 8; break;
            case 'f': v = 12; break; case 'n': v = 10; break;
            case 'r': v = 13; break; case 't': v = 9; break;
            case 'v': v = 11; break; case 'e': case 'E': v = 27; break;
            case '\\': case '\'': case '"': case '?': v = c; break;
            default: return false;
            }
        }
    } else {
        unsigned char c = s[i++];
        unsigned count;
        uint32_t minimum;
        if (c < 0x80) {
            if (!c || c == '\n' || c == (unsigned char)quote || c == '\\') return false;
            v = c;
            count = 0; minimum = 0;
        } else if (c >= 0xc2 && c <= 0xdf) {
            v = c & 0x1f; count = 1; minimum = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            v = c & 0x0f; count = 2; minimum = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            v = c & 0x07; count = 3; minimum = 0x10000;
        } else return false;
        if (end - i < count) return false;
        while (count--) {
            unsigned char next = s[i++];
            if ((next & 0xc0) != 0x80) return false;
            v = (v << 6) | (next & 0x3f);
        }
        if (v < minimum || v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff))
            return false;
    }
    *consumed = i;
    *value = v <= INT32_MAX ? (int32_t)v : (int32_t)((int64_t)v - INT64_C(4294967296));
    return true;
}

/**
 * @brief Decodes a complete wide character literal.
 * @param text Literal spelling including prefix and quotes.
 * @param length Length of the spelling in bytes.
 * @param value Output receiving the signed value.
 * @return True when the spelling is valid and supported.
 */
bool token_decode_wide_character(const char *text, size_t length, int32_t *value)
{
    size_t consumed;
    return text && length >= 4 && text[0] == 'L' && text[1] == '\'' &&
           text[length - 1] == '\'' &&
           token_decode_wide_element(text + 2, length - 3, '\'', &consumed, value) &&
           consumed == length - 3;
}
