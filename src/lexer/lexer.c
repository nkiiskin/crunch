#include "lexer/lexer.h"
#include "common/identifier.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const SourceFile *source;
    size_t offset;
    uint32_t line;
    uint32_t column;
    Token *tokens;
    size_t count;
    size_t capacity;
    unsigned int pack_alignment;
    bool line_start;
    bool failed;
} Lexer;

/**
 * @brief Returns the current source character without advancing.
 * @param lx Lexer state whose source cursor or token output is inspected.
 * @return Character read or consumed from the source, or NUL at end of input.
 */
static char peek(const Lexer *lx)
{
    return lx->offset < lx->source->length ? lx->source->data[lx->offset] : '\0';
}

/**
 * @brief Returns the character following the current source character.
 * @param lx Lexer state whose source cursor or token output is inspected.
 * @return Character read or consumed from the source, or NUL at end of input.
 */
static char peek_next(const Lexer *lx)
{
    return lx->offset + 1 < lx->source->length ? lx->source->data[lx->offset + 1] : '\0';
}

/**
 * @brief Consumes a valid source-location marker emitted by crunch-pp.
 * @param lx Lexer state whose source cursor and location may be updated.
 * @return True if a complete marker was consumed; otherwise false without advancing.
 */
static bool skip_preprocessor_marker(Lexer *lx)
{
    static const char marker[] = "/*CRUNCH-PP-MAP:";
    size_t offset = lx->offset;
    uint32_t line = 0;
    if (lx->source->length - offset < sizeof(marker) - 1 ||
        memcmp(lx->source->data + offset, marker, sizeof(marker) - 1) != 0)
        return false;
    offset += sizeof(marker) - 1;
    if (offset >= lx->source->length || !isdigit((unsigned char)lx->source->data[offset]))
        return false;
    while (offset < lx->source->length && isdigit((unsigned char)lx->source->data[offset])) {
        unsigned digit = (unsigned)(lx->source->data[offset] - '0');
        if (line > (UINT32_MAX - digit) / 10)
            return false;
        line = line * 10 + digit;
        ++offset;
    }
    if (!line || offset >= lx->source->length || lx->source->data[offset++] != ':')
        return false;
    while (offset + 1 < lx->source->length &&
           !(lx->source->data[offset] == '*' && lx->source->data[offset + 1] == '/')) {
        if (lx->source->data[offset] == '\0')
            return false;
        ++offset;
    }
    if (offset + 1 >= lx->source->length)
        return false;
    offset += 2;
    if (offset < lx->source->length && lx->source->data[offset] == '\r')
        ++offset;
    if (offset >= lx->source->length || lx->source->data[offset] != '\n')
        return false;
    lx->offset = offset + 1;
    lx->line = line;
    lx->column = 1;
    lx->line_start = true;
    return true;
}

/**
 * @brief Consumes one source character and updates lexer location.
 * @param lx Lexer state whose source cursor or token output is updated.
 * @return Character consumed from the source, or NUL at end of input.
 */
static char advance(Lexer *lx)
{
    char c;
    if (lx->offset >= lx->source->length)
        return '\0';
    c = lx->source->data[lx->offset++];
    if (c == '\n') {
        ++lx->line;
        lx->column = 1;
        lx->line_start = true;
    } else {
        ++lx->column;
        if (!isspace((unsigned char)c))
            lx->line_start = false;
    }
    return c;
}

/**
 * @brief Appends a token whose lexeme refers to the source buffer.
 * @param lx Lexer state whose source cursor or token output is updated.
 * @param kind Token, AST-node, or type kind to process.
 * @param start Source-buffer offset where the token lexeme begins.
 * @param location Source location associated with the token or diagnostic.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool append_token(Lexer *lx, TokenKind kind, size_t start, SourceLocation location)
{
    Token *grown;
    size_t cap;
    if (lx->count == lx->capacity) {
        cap = lx->capacity ? lx->capacity * 2 : 64;
        if (cap < lx->capacity || cap > SIZE_MAX / sizeof(*lx->tokens))
            return false;
        grown = (Token *)realloc(lx->tokens, cap * sizeof(*lx->tokens));
        if (!grown)
            return false;
        lx->tokens = grown;
        lx->capacity = cap;
    }
    lx->tokens[lx->count].kind = kind;
    lx->tokens[lx->count].location = location;
    lx->tokens[lx->count].lexeme = lx->source->data ? lx->source->data + start : "";
    lx->tokens[lx->count].length = lx->offset - start;
    lx->tokens[lx->count].pack_alignment = lx->pack_alignment;
    ++lx->count;
    if (kind == TOKEN_INVALID)
        lx->failed = true;
    return true;
}

/**
 * @brief Creates and appends a token for the current source span.
 * @param lx Lexer state whose source cursor or token output is updated.
 * @param kind Token, AST-node, or type kind to process.
 * @param start Source-buffer offset where the token lexeme begins.
 * @param loc Source location associated with the token or diagnostic.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool emit(Lexer *lx, TokenKind kind, size_t start, SourceLocation loc)
{
    return append_token(lx, kind, start, loc);
}

/**
 * @brief Maps identifier text to a keyword or identifier token kind.
 * @param s Keyword spelling to look up.
 * @param n Length of the candidate keyword in bytes.
 * @return Enum value matching the input, or the documented invalid/fallback value.
 */
static TokenKind keyword_kind(const char *s, size_t n)
{
#define KW(word, token)                                                                            \
    if (n == sizeof(word) - 1 && memcmp(s, word, sizeof(word) - 1) == 0)                           \
    return token
    KW("void", TOKEN_VOID);
    KW("_Bool", TOKEN_BOOL);
    KW("enum", TOKEN_ENUM);
    KW("int", TOKEN_INT);
    KW("short", TOKEN_SHORT);
    KW("long", TOKEN_LONG);
    KW("signed", TOKEN_SIGNED);
    KW("__signed", TOKEN_SIGNED);
    KW("__signed__", TOKEN_SIGNED);
    KW("unsigned", TOKEN_UNSIGNED);
    KW("char", TOKEN_BYTE);
    KW("_Noreturn", TOKEN_NORETURN);
    KW("_Static_assert", TOKEN_STATIC_ASSERT);
    KW("_Thread_local", TOKEN_THREAD_LOCAL);
    KW("const", TOKEN_CONST);
    KW("volatile", TOKEN_VOLATILE);
    KW("restrict", TOKEN_RESTRICT);
    KW("__restrict", TOKEN_RESTRICT);
    KW("__restrict__", TOKEN_RESTRICT);
    KW("register", TOKEN_REGISTER);
    KW("auto", TOKEN_AUTO);
    KW("inline", TOKEN_INLINE);
    KW("goto", TOKEN_GOTO);
    KW("static", TOKEN_STATIC);
    KW("extern", TOKEN_EXTERN);
    KW("typedef", TOKEN_DEF);
    KW("double", TOKEN_REAL);
    KW("float", TOKEN_FLOAT);
    KW("if", TOKEN_IF);
    KW("else", TOKEN_ELSE);
    KW("switch", TOKEN_SWITCH);
    KW("case", TOKEN_CASE);
    KW("default", TOKEN_DEFAULT);
    KW("for", TOKEN_FOR);
    KW("while", TOKEN_LOOP);
    KW("do", TOKEN_DO);
    KW("break", TOKEN_BREAK);
    KW("continue", TOKEN_CONTINUE);
    KW("return", TOKEN_RETURN);
    KW("sizeof", TOKEN_SIZEOF);
    KW("_Alignof", TOKEN_ALIGNOF);
    KW("_Alignas", TOKEN_ALIGNAS);
    KW("struct", TOKEN_STRUCT);
    KW("union", TOKEN_UNION);
#undef KW
    return TOKEN_IDENTIFIER;
}

/**
 * @brief Consumes and validates a quoted-literal escape sequence.
 * @param lx Lexer state whose source cursor or token output is updated.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool scan_escape(Lexer *lx)
{
    char c = advance(lx);
    if (c == '\0')
        return false;
    if (c >= '0' && c <= '7') {
        unsigned value = (unsigned)(c - '0');
        for (int i = 1; i < 3 && peek(lx) >= '0' && peek(lx) <= '7'; ++i)
            value = value * 8 + (unsigned)(advance(lx) - '0');
        return value <= 255;
    }
    if (strchr("abefnrtvE\\'\"?", c))
        return true;
    if (c == 'x') {
        unsigned value = 0;
        bool valid = isxdigit((unsigned char)peek(lx)) != 0;
        while (isxdigit((unsigned char)peek(lx))) {
            unsigned char h = (unsigned char)advance(lx);
            unsigned digit = h <= '9' ? (unsigned)(h - '0') : (unsigned)(tolower(h) - 'a' + 10);
            if (value > (255U - digit) / 16U)
                valid = false;
            else
                value = value * 16U + digit;
        }
        return valid;
    }
    return false;
}

/**
 * @brief Scans and emits a quoted byte or string literal.
 * @param lx Lexer state whose source cursor or token output is updated.
 * @param start Source-buffer offset where the quoted literal begins.
 * @param loc Source location associated with the token or diagnostic.
 * @param quote Quote character delimiting the literal.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool scan_quoted(Lexer *lx, size_t start, SourceLocation loc, char quote)
{
    size_t decoded_chars = 0;
    bool valid = true;
    advance(lx); /* opening quote */
    while (lx->offset < lx->source->length && peek(lx) != quote && peek(lx) != '\n') {
        if (peek(lx) == '\\') {
            advance(lx);
            if (!scan_escape(lx))
                valid = false;
        } else {
            if (peek(lx) == '\0')
                valid = false;
            advance(lx);
        }
        ++decoded_chars;
    }
    if (peek(lx) == quote) {
        advance(lx);
    } else {
        valid = false;
    }
    if (quote == '\'' && decoded_chars != 1)
        valid = false;
    return emit(lx,
                valid ? (quote == '\'' ? TOKEN_CHAR_LITERAL : TOKEN_STRING_LITERAL) : TOKEN_INVALID,
                start, loc);
}

/**
 * @brief Scans and emits an integer or real literal.
 * @param lx Lexer state whose source cursor or token output is updated.
 * @param start Source-buffer offset where the numeric literal begins.
 * @param loc Source location associated with the token or diagnostic.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool scan_number(Lexer *lx, size_t start, SourceLocation loc)
{
    bool real = false;
    bool valid = true;

    if (peek(lx) == '0' && (peek_next(lx) == 'x' || peek_next(lx) == 'X')) {
        advance(lx);
        advance(lx);
        bool digits = false;
        while (isxdigit((unsigned char)peek(lx))) {
            digits = true;
            advance(lx);
        }
        if (peek(lx) == '.') {
            real = true;
            advance(lx);
            while (isxdigit((unsigned char)peek(lx))) {
                digits = true;
                advance(lx);
            }
        }
        if (!digits)
            return emit(lx, TOKEN_INVALID, start, loc);
        if (peek(lx) == 'p' || peek(lx) == 'P') {
            real = true;
            advance(lx);
            if (peek(lx) == '+' || peek(lx) == '-')
                advance(lx);
            if (!isdigit((unsigned char)peek(lx)))
                return emit(lx, TOKEN_INVALID, start, loc);
            while (isdigit((unsigned char)peek(lx)))
                advance(lx);
        } else if (real) {
            valid = false;
        }
    } else {
        /* A leading dot is accepted only when the caller sees a following digit. */
        if (peek(lx) == '.') {
            real = true;
            advance(lx);
            while (isdigit((unsigned char)peek(lx)))
                advance(lx);
        } else {
            while (isdigit((unsigned char)peek(lx)))
                advance(lx);
            if (peek(lx) == '.') {
                real = true;
                advance(lx);
                while (isdigit((unsigned char)peek(lx)))
                    advance(lx);
            }
        }

        if (peek(lx) == 'e' || peek(lx) == 'E') {
            real = true;
            advance(lx);
            if (peek(lx) == '+' || peek(lx) == '-')
                advance(lx);
            if (!isdigit((unsigned char)peek(lx)))
                return emit(lx, TOKEN_INVALID, start, loc);
            while (isdigit((unsigned char)peek(lx)))
                advance(lx);
        }
    }

    if (real && (peek(lx) == 'f' || peek(lx) == 'F' ||
                 peek(lx) == 'l' || peek(lx) == 'L')) {
        advance(lx);
    } else if (!real) {
        if (peek(lx) == 'u' || peek(lx) == 'U') {
            advance(lx);
            if (peek(lx) == 'l' || peek(lx) == 'L') {
                char first = peek(lx);
                advance(lx);
                if (peek(lx) == first)
                    advance(lx);
            }
        } else if (peek(lx) == 'l' || peek(lx) == 'L') {
            char first = peek(lx);
            advance(lx);
            if (peek(lx) == first)
                advance(lx);
            if (peek(lx) == 'u' || peek(lx) == 'U')
                advance(lx);
        }
    }
    if (isalnum((unsigned char)peek(lx)) || peek(lx) == '_') {
        while (isalnum((unsigned char)peek(lx)) || peek(lx) == '_')
            advance(lx);
        valid = false;
    }
    return emit(lx, !valid ? TOKEN_INVALID : real ? TOKEN_REAL_LITERAL : TOKEN_INTEGER_LITERAL,
                start, loc);
}

/**
 * @brief Consumes whitespace and source comments.
 * @param lx Lexer state whose source cursor or token output is updated.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool skip_space_and_comments(Lexer *lx)
{
    for (;;) {
        while (isspace((unsigned char)peek(lx)))
            advance(lx);
        if (lx->line_start && peek(lx) == '#') {
            static const char prefix[] = "#pragma pack(";
            size_t pos = lx->offset, end = pos;
            const char *data = lx->source->data;
            size_t length = lx->source->length;
            while (end < length && data[end] != '\n' && data[end] != '\r')
                ++end;
            if (end - pos >= sizeof(prefix) &&
                memcmp(data + pos, prefix, sizeof(prefix) - 1) == 0) {
                size_t value_start = pos + sizeof(prefix) - 1;
                unsigned int value = 0;
                size_t i = value_start;
                while (i < end && data[i] >= '0' && data[i] <= '9' &&
                       value <= 16) {
                    value = value * 10 + (unsigned int)(data[i++] - '0');
                }
                if (i > value_start && i + 1 == end && data[i] == ')' &&
                    (value == 0 || value == 1 || value == 2 || value == 4 ||
                     value == 8 || value == 16)) {
                    while (lx->offset < end) advance(lx);
                    lx->pack_alignment = value;
                    continue;
                }
            }
            static const char gcc_pragma[] = "#pragma GCC ";
            if (end - pos >= sizeof(gcc_pragma) - 1 &&
                memcmp(data + pos, gcc_pragma, sizeof(gcc_pragma) - 1) == 0) {
                SourceLocation loc = {lx->offset, lx->line, lx->column};
                while (lx->offset < end) advance(lx);
                if (!emit(lx, TOKEN_INVALID, pos, loc)) return false;
                continue;
            }
        }
        if (peek(lx) == '/' && peek_next(lx) == '*' && skip_preprocessor_marker(lx))
            continue;
        if (peek(lx) == '/' && peek_next(lx) == '/') {
            while (peek(lx) && peek(lx) != '\n')
                advance(lx);
            continue;
        }
        if (peek(lx) == '/' && peek_next(lx) == '*') {
            size_t start = lx->offset;
            SourceLocation loc = {lx->offset, lx->line, lx->column};
            bool line_start = lx->line_start;
            uint32_t line = lx->line;
            advance(lx);
            advance(lx);
            while (peek(lx) && !(peek(lx) == '*' && peek_next(lx) == '/'))
                advance(lx);
            if (!peek(lx))
                return emit(lx, TOKEN_INVALID, start, loc);
            advance(lx);
            advance(lx);
            lx->line_start = lx->line != line || line_start;
            continue;
        }
        return true;
    }
}

/**
 * @brief Tokenizes a source file into the output token array.
 * @param source Loaded source buffer and its metadata.
 * @param output Output token array populated on success.
 * @return COMPILER_OK on success, otherwise an input, lexical, or memory error status.
 */
CompilerStatus lexer_run(const SourceFile *source, TokenArray *output)
{
    Lexer lx;
    if (!output)
        return COMPILER_INVALID_INPUT;
    output->items = NULL;
    output->count = 0;
    if (!source || (!source->data && source->length != 0))
        return COMPILER_INVALID_INPUT;
    memset(&lx, 0, sizeof(lx));
    lx.source = source;
    lx.line = 1;
    lx.column = 1;
    lx.line_start = true;

    while (lx.offset < source->length) {
        size_t start;
        SourceLocation loc;
        char c;
        TokenKind kind = TOKEN_INVALID;
        if (!skip_space_and_comments(&lx))
            goto oom;
        if (lx.offset >= source->length)
            break;
        start = lx.offset;
        loc.offset = start;
        loc.line = lx.line;
        loc.column = lx.column;
        c = peek(&lx);
        if (c == 'L' && (peek_next(&lx) == '\'' || peek_next(&lx) == '"')) {
            int32_t value;
            char quote = peek_next(&lx);
            advance(&lx);
            advance(&lx);
            while (lx.offset < source->length && peek(&lx) != quote && peek(&lx) != '\n') {
                if (peek(&lx) == '\\') {
                    advance(&lx);
                    if (lx.offset >= source->length || peek(&lx) == '\n') break;
                }
                advance(&lx);
            }
            bool closed = lx.offset < source->length && peek(&lx) == quote;
            if (closed) advance(&lx);
            if (quote == '\'') {
                kind = token_decode_wide_character(source->data + start, lx.offset - start, &value)
                           ? TOKEN_WIDE_CHAR_LITERAL : TOKEN_INVALID;
            } else {
                size_t pos = start + 2, end = closed ? lx.offset - 1 : lx.offset;
                bool valid = closed;
                while (valid && pos < end) {
                    size_t consumed;
                    valid = token_decode_wide_element(source->data + pos, end - pos,
                                                      quote, &consumed, &value);
                    if (valid) pos += consumed;
                }
                kind = valid ? TOKEN_WIDE_STRING_LITERAL : TOKEN_INVALID;
            }
            if (!emit(&lx, kind, start, loc)) goto oom;
            continue;
        }
        if (crunch_identifier_start((unsigned char)c)) {
            advance(&lx);
            while (crunch_identifier_continue((unsigned char)peek(&lx)))
                advance(&lx);
            kind = keyword_kind(source->data + start, lx.offset - start);
            if (!emit(&lx, kind, start, loc))
                goto oom;
            continue;
        }
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)peek_next(&lx)))) {
            if (!scan_number(&lx, start, loc))
                goto oom;
            continue;
        }
        if (c == '"' || c == '\'') {
            if (!scan_quoted(&lx, start, loc, c))
                goto oom;
            continue;
        }
#define TWO(a, b, two, one)                                                                        \
    advance(&lx);                                                                                  \
    if (peek(&lx) == b) {                                                                          \
        advance(&lx);                                                                              \
        kind = two;                                                                                \
    } else                                                                                         \
        kind = one;                                                                                \
    break
#define THREE(a, b, cch, three, two, one)                                                          \
    case a:                                                                                        \
        advance(&lx);                                                                              \
        if (peek(&lx) == b) {                                                                      \
            advance(&lx);                                                                          \
            if (peek(&lx) == cch) {                                                                \
                advance(&lx);                                                                      \
                kind = three;                                                                      \
            } else                                                                                 \
                kind = two;                                                                        \
        } else                                                                                     \
            kind = one;                                                                            \
        break
        switch (c) {
        case '(':
            advance(&lx);
            kind = TOKEN_LPAREN;
            break;
        case ')':
            advance(&lx);
            kind = TOKEN_RPAREN;
            break;
        case '{':
            advance(&lx);
            kind = TOKEN_LBRACE;
            break;
        case '}':
            advance(&lx);
            kind = TOKEN_RBRACE;
            break;
        case '[':
            advance(&lx);
            kind = TOKEN_LBRACKET;
            break;
        case ']':
            advance(&lx);
            kind = TOKEN_RBRACKET;
            break;
        case ';':
            advance(&lx);
            kind = TOKEN_SEMICOLON;
            break;
        case ',':
            advance(&lx);
            kind = TOKEN_COMMA;
            break;
        case '.':
            advance(&lx);
            kind = TOKEN_DOT;
            break;
        case '?':
            advance(&lx);
            kind = TOKEN_QUESTION;
            break;
        case ':':
            advance(&lx);
            kind = TOKEN_COLON;
            break;
        case '#':
            advance(&lx);
            kind = TOKEN_HASH;
            break;
        case '+':
            advance(&lx);
            if (peek(&lx) == '+') {
                advance(&lx);
                kind = TOKEN_INCREMENT;
            } else if (peek(&lx) == '=') {
                advance(&lx);
                kind = TOKEN_PLUS_ASSIGN;
            } else
                kind = TOKEN_PLUS;
            break;
        case '-':
            advance(&lx);
            if (peek(&lx) == '>') {
                advance(&lx);
                kind = TOKEN_ARROW;
            } else if (peek(&lx) == '-') {
                advance(&lx);
                kind = TOKEN_DECREMENT;
            } else if (peek(&lx) == '=') {
                advance(&lx);
                kind = TOKEN_MINUS_ASSIGN;
            } else
                kind = TOKEN_MINUS;
            break;
        case '*':
            TWO('*', '=', TOKEN_STAR_ASSIGN, TOKEN_STAR);
            break;
        case '/':
            TWO('/', '=', TOKEN_SLASH_ASSIGN, TOKEN_SLASH);
            break;
        case '%':
            TWO('%', '=', TOKEN_PERCENT_ASSIGN, TOKEN_PERCENT);
            break;
        case '=':
            TWO('=', '=', TOKEN_EQUAL, TOKEN_ASSIGN);
            break;
        case '!':
            TWO('!', '=', TOKEN_NOT_EQUAL, TOKEN_LOGICAL_NOT);
            break;
        case '<':
            advance(&lx);
            if (peek(&lx) == '<') {
                advance(&lx);
                if (peek(&lx) == '=') {
                    advance(&lx);
                    kind = TOKEN_SHIFT_LEFT_ASSIGN;
                } else
                    kind = TOKEN_SHIFT_LEFT;
            } else if (peek(&lx) == '=') {
                advance(&lx);
                kind = TOKEN_LESS_EQUAL;
            } else
                kind = TOKEN_LESS;
            break;
        case '>':
            advance(&lx);
            if (peek(&lx) == '>') {
                advance(&lx);
                if (peek(&lx) == '=') {
                    advance(&lx);
                    kind = TOKEN_SHIFT_RIGHT_ASSIGN;
                } else
                    kind = TOKEN_SHIFT_RIGHT;
            } else if (peek(&lx) == '=') {
                advance(&lx);
                kind = TOKEN_GREATER_EQUAL;
            } else
                kind = TOKEN_GREATER;
            break;
        case '&':
            advance(&lx);
            if (peek(&lx) == '&') {
                advance(&lx);
                kind = TOKEN_LOGICAL_AND;
            } else if (peek(&lx) == '=') {
                advance(&lx);
                kind = TOKEN_AND_ASSIGN;
            } else
                kind = TOKEN_BITWISE_AND;
            break;
        case '|':
            advance(&lx);
            if (peek(&lx) == '|') {
                advance(&lx);
                kind = TOKEN_LOGICAL_OR;
            } else if (peek(&lx) == '=') {
                advance(&lx);
                kind = TOKEN_OR_ASSIGN;
            } else
                kind = TOKEN_BITWISE_OR;
            break;
        case '^':
            TWO('^', '=', TOKEN_XOR_ASSIGN, TOKEN_BITWISE_XOR);
            break;
        case '~':
            advance(&lx);
            kind = TOKEN_BITWISE_NOT;
            break;
        default:
            advance(&lx);
            kind = TOKEN_INVALID;
            break;
        }
#undef TWO
#undef THREE
        if (!emit(&lx, kind, start, loc))
            goto oom;
    }
    {
        SourceLocation loc = {source->length, lx.line, lx.column};
        if (!append_token(&lx, TOKEN_EOF, source->length, loc))
            goto oom;
    }
    output->items = lx.tokens;
    output->count = lx.count;
    return lx.failed ? COMPILER_LEX_ERROR : COMPILER_OK;

oom:
    free(lx.tokens);
    return COMPILER_OUT_OF_MEMORY;
}

/**
 * @brief Releases the token array without freeing source-buffer lexemes.
 * @param tokens Token array being populated, read, or released.
 * @return No value; the function performs the documented output or state update.
 */
void lexer_destroy(TokenArray *tokens)
{
    if (!tokens)
        return;
    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
}
