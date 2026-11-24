#include "lexer/lexer.h"
#include "lexer/token.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Lexes a string and asserts the expected lexer status.
 * @param text Source text to tokenize.
 * @param expected Expected compiler status.
 * @return Token array owned by the caller.
 */
static TokenArray lex(const char *text, CompilerStatus expected)
{
    SourceFile source = { "test.bc", text, strlen(text) };
    TokenArray result = { 0 };
    assert(lexer_run(&source, &result) == expected);
    return result;
}

/**
 * @brief Asserts that one token has the expected kind and lexeme.
 * @param a Token array containing the token.
 * @param i Token index to check.
 * @param kind Expected token kind.
 * @param lexeme Expected token text.
 * @return No value; aborts the test process if any assertion fails.
 */
static void expect(TokenArray *a, size_t i, TokenKind kind, const char *lexeme)
{
    assert(i < a->count);
    if (a->items[i].kind != kind) {
        fprintf(stderr, "token %zu: expected %s, got %s\n", i, token_kind_name(kind), token_kind_name(a->items[i].kind));
        assert(a->items[i].kind == kind);
    }
    assert(a->items[i].length == strlen(lexeme));
    assert(memcmp(a->items[i].lexeme, lexeme, strlen(lexeme)) == 0);
}

/**
 * @brief Tests keyword recognition, identifiers, and source locations.
 * @note This test takes no arguments and returns no value.
 */
static void test_keywords_identifiers_and_locations(void)
{
    TokenArray automatic = lex("auto automatic", COMPILER_OK);
    expect(&automatic, 0, TOKEN_AUTO, "auto");
    expect(&automatic, 1, TOKEN_IDENTIFIER, "automatic");
    lexer_destroy(&automatic);
    TokenArray a = lex("int main() {\n  long total = 12; char _x = 12;\n return _x; }", COMPILER_OK);
    expect(&a, 0, TOKEN_INT, "int");
    expect(&a, 1, TOKEN_IDENTIFIER, "main");
    expect(&a, 4, TOKEN_LBRACE, "{");
    expect(&a, 5, TOKEN_LONG, "long");
    expect(&a, 6, TOKEN_IDENTIFIER, "total");
    expect(&a, 10, TOKEN_BYTE, "char");
    assert(a.items[10].location.line == 2 && a.items[10].location.column == 20);
    expect(&a, 13, TOKEN_INTEGER_LITERAL, "12");
    expect(&a, 15, TOKEN_RETURN, "return");
    expect(&a, a.count - 1, TOKEN_EOF, "");
    lexer_destroy(&a);
}

/**
 * @brief Tests recognition of supported operators and punctuation.
 * @note This test takes no arguments and returns no value.
 */
static void test_operators(void)
{
    const char *s = "+ - * / % = == != < <= > >= && || ! & | ^ ~ << >> ++ -- += -= *= /= %= &= |= ^= <<= >>=? : # ->";
    const TokenKind kinds[] = { TOKEN_PLUS,TOKEN_MINUS,TOKEN_STAR,TOKEN_SLASH,TOKEN_PERCENT,
        TOKEN_ASSIGN,TOKEN_EQUAL,TOKEN_NOT_EQUAL,TOKEN_LESS,TOKEN_LESS_EQUAL,TOKEN_GREATER,
        TOKEN_GREATER_EQUAL,TOKEN_LOGICAL_AND,TOKEN_LOGICAL_OR,TOKEN_LOGICAL_NOT,
        TOKEN_BITWISE_AND,TOKEN_BITWISE_OR,TOKEN_BITWISE_XOR,TOKEN_BITWISE_NOT,
        TOKEN_SHIFT_LEFT,TOKEN_SHIFT_RIGHT,TOKEN_INCREMENT,TOKEN_DECREMENT,TOKEN_PLUS_ASSIGN,
        TOKEN_MINUS_ASSIGN,TOKEN_STAR_ASSIGN,TOKEN_SLASH_ASSIGN,TOKEN_PERCENT_ASSIGN,
        TOKEN_AND_ASSIGN,TOKEN_OR_ASSIGN,TOKEN_XOR_ASSIGN,TOKEN_SHIFT_LEFT_ASSIGN,
        TOKEN_SHIFT_RIGHT_ASSIGN,TOKEN_QUESTION,TOKEN_COLON,TOKEN_HASH,TOKEN_ARROW };
    TokenArray a = lex(s, COMPILER_OK);
    assert(a.count == sizeof(kinds)/sizeof(kinds[0]) + 1);
    for (size_t i = 0; i < sizeof(kinds)/sizeof(kinds[0]); ++i) assert(a.items[i].kind == kinds[i]);
    lexer_destroy(&a);
}

/**
 * @brief Tests numeric, string, and character literals alongside comments.
 * @note This test takes no arguments and returns no value.
 */
static void test_literals_and_comments(void)
{
    TokenArray wide = lex("L'a' L'\\x1234' L'\\777' L'\\u03a9' L'Ω' L'😀'", COMPILER_OK);
    for (size_t i = 0; i < 6; ++i) assert(wide.items[i].kind == TOKEN_WIDE_CHAR_LITERAL);
    lexer_destroy(&wide);
    wide = lex("L'' L'ab' L'\\x' L'\\x100000000' L'\\ud800' L'\\u0041'", COMPILER_LEX_ERROR);
    for (size_t i = 0; i < 6; ++i) assert(wide.items[i].kind == TOKEN_INVALID);
    lexer_destroy(&wide);
    wide = lex("L\"\" L\"hello Ω\\u03a9\\x1234\\777\\0\" L\"'\\\"\"", COMPILER_OK);
    for (size_t i = 0; i < 3; ++i) assert(wide.items[i].kind == TOKEN_WIDE_STRING_LITERAL);
    lexer_destroy(&wide);
    wide = lex("L\"\\x\" L\"\\x100000000\" L\"\\ud800\" L\"\\u0041\" L\"\\q\"", COMPILER_LEX_ERROR);
    for (size_t i = 0; i < 5; ++i) assert(wide.items[i].kind == TOKEN_INVALID);
    lexer_destroy(&wide);
    TokenArray a = lex("// line\n/* block */ 0 42 0xff 1.25 .5 2e-3 12. .5E-2 1e3 \"hi\\n\" 'a' '\\x41'", COMPILER_OK);
    expect(&a, 0, TOKEN_INTEGER_LITERAL, "0");
    expect(&a, 1, TOKEN_INTEGER_LITERAL, "42");
    expect(&a, 2, TOKEN_INTEGER_LITERAL, "0xff");
    expect(&a, 3, TOKEN_REAL_LITERAL, "1.25");
    expect(&a, 4, TOKEN_REAL_LITERAL, ".5");
    expect(&a, 5, TOKEN_REAL_LITERAL, "2e-3");
    expect(&a, 6, TOKEN_REAL_LITERAL, "12.");
    expect(&a, 7, TOKEN_REAL_LITERAL, ".5E-2");
    expect(&a, 8, TOKEN_REAL_LITERAL, "1e3");
    expect(&a, 9, TOKEN_STRING_LITERAL, "\"hi\\n\"");
    expect(&a, 10, TOKEN_CHAR_LITERAL, "'a'");
    expect(&a, 11, TOKEN_CHAR_LITERAL, "'\\x41'");
    lexer_destroy(&a);
    a = lex("'\\a' '\\b' '\\f' '\\v' '\\?' \"\\a\\b\\f\\v\\?\"", COMPILER_OK);
    expect(&a, 0, TOKEN_CHAR_LITERAL, "'\\a'");
    expect(&a, 1, TOKEN_CHAR_LITERAL, "'\\b'");
    expect(&a, 2, TOKEN_CHAR_LITERAL, "'\\f'");
    expect(&a, 3, TOKEN_CHAR_LITERAL, "'\\v'");
    expect(&a, 4, TOKEN_CHAR_LITERAL, "'\\?'");
    expect(&a, 5, TOKEN_STRING_LITERAL, "\"\\a\\b\\f\\v\\?\"");
    lexer_destroy(&a);
    a = lex("'\\1' '\\12' '\\377' \"\\0\\01\\3777\"", COMPILER_OK);
    expect(&a, 0, TOKEN_CHAR_LITERAL, "'\\1'");
    expect(&a, 1, TOKEN_CHAR_LITERAL, "'\\12'");
    expect(&a, 2, TOKEN_CHAR_LITERAL, "'\\377'");
    expect(&a, 3, TOKEN_STRING_LITERAL, "\"\\0\\01\\3777\"");
    lexer_destroy(&a);
    a = lex("'\\x7' '\\x0000fF' \"\\x0z\\x000000001a\"", COMPILER_OK);
    expect(&a, 0, TOKEN_CHAR_LITERAL, "'\\x7'");
    expect(&a, 1, TOKEN_CHAR_LITERAL, "'\\x0000fF'");
    expect(&a, 2, TOKEN_STRING_LITERAL, "\"\\x0z\\x000000001a\"");
    lexer_destroy(&a);
    a = lex("'\\x' \"\\xG\" '\\x100' \"\\x100000000000000000000\"",
            COMPILER_LEX_ERROR);
    for (size_t i = 0; i < 4; ++i) assert(a.items[i].kind == TOKEN_INVALID);
    lexer_destroy(&a);
    a = lex("float floating 1.0f .5F 1e3f 12.F", COMPILER_OK);
    expect(&a, 0, TOKEN_FLOAT, "float");
    expect(&a, 1, TOKEN_IDENTIFIER, "floating");
    expect(&a, 2, TOKEN_REAL_LITERAL, "1.0f");
    expect(&a, 3, TOKEN_REAL_LITERAL, ".5F");
    expect(&a, 4, TOKEN_REAL_LITERAL, "1e3f");
    expect(&a, 5, TOKEN_REAL_LITERAL, "12.F");
    lexer_destroy(&a);
    a = lex("1.0L .5l 1e4932L 12.l", COMPILER_OK);
    expect(&a, 0, TOKEN_REAL_LITERAL, "1.0L");
    expect(&a, 1, TOKEN_REAL_LITERAL, ".5l");
    expect(&a, 2, TOKEN_REAL_LITERAL, "1e4932L");
    expect(&a, 3, TOKEN_REAL_LITERAL, "12.l");
    lexer_destroy(&a);
    a = lex("0x1p0 0X.8P+2 0x1.ffffp127L 0x1p-149f 0xffL", COMPILER_OK);
    expect(&a, 0, TOKEN_REAL_LITERAL, "0x1p0");
    expect(&a, 1, TOKEN_REAL_LITERAL, "0X.8P+2");
    expect(&a, 2, TOKEN_REAL_LITERAL, "0x1.ffffp127L");
    expect(&a, 3, TOKEN_REAL_LITERAL, "0x1p-149f");
    expect(&a, 4, TOKEN_INTEGER_LITERAL, "0xffL");
    lexer_destroy(&a);
    a = lex("short unsigned short shorter signed signedness __signed __signed__ __signed__x", COMPILER_OK);
    expect(&a, 0, TOKEN_SHORT, "short");
    expect(&a, 1, TOKEN_UNSIGNED, "unsigned");
    expect(&a, 2, TOKEN_SHORT, "short");
    expect(&a, 3, TOKEN_IDENTIFIER, "shorter");
    expect(&a, 4, TOKEN_SIGNED, "signed");
    expect(&a, 5, TOKEN_IDENTIFIER, "signedness");
    expect(&a, 6, TOKEN_SIGNED, "__signed");
    expect(&a, 7, TOKEN_SIGNED, "__signed__");
    expect(&a, 8, TOKEN_IDENTIFIER, "__signed__x");
    lexer_destroy(&a);
    a = lex("volatile volatile_value register registered", COMPILER_OK);
    expect(&a, 0, TOKEN_VOLATILE, "volatile");
    expect(&a, 1, TOKEN_IDENTIFIER, "volatile_value");
    expect(&a, 2, TOKEN_REGISTER, "register");
    expect(&a, 3, TOKEN_IDENTIFIER, "registered");
    lexer_destroy(&a);
    a = lex("_Alignas _Alignas_value alignas", COMPILER_OK);
    expect(&a, 0, TOKEN_ALIGNAS, "_Alignas");
    expect(&a, 1, TOKEN_IDENTIFIER, "_Alignas_value");
    expect(&a, 2, TOKEN_IDENTIFIER, "alignas");
    lexer_destroy(&a);
    a = lex("_Thread_local _Thread_local_value", COMPILER_OK);
    expect(&a, 0, TOKEN_THREAD_LOCAL, "_Thread_local");
    expect(&a, 1, TOKEN_IDENTIFIER, "_Thread_local_value");
    lexer_destroy(&a);
}

/**
 * @brief Tests structure keywords and ordinary identifiers.
 * @note This test takes no arguments and returns no value.
 */
static void test_structure_and_dim_keywords(void)
{
    TokenArray a = lex("struct Animal { double weight; long id; char name[]; } dim name < 80;", COMPILER_OK);
    expect(&a, 0, TOKEN_STRUCT, "struct");
    expect(&a, 1, TOKEN_IDENTIFIER, "Animal");
    expect(&a, 3, TOKEN_REAL, "double");
    expect(&a, 6, TOKEN_LONG, "long");
    expect(&a, 9, TOKEN_BYTE, "char");
    expect(&a, 15, TOKEN_IDENTIFIER, "dim");
    lexer_destroy(&a);
    a = lex("struct while type loop typeface", COMPILER_OK);
    expect(&a, 0, TOKEN_STRUCT, "struct");
    expect(&a, 1, TOKEN_LOOP, "while");
    expect(&a, 2, TOKEN_IDENTIFIER, "type");
    expect(&a, 3, TOKEN_IDENTIFIER, "loop");
    expect(&a, 4, TOKEN_IDENTIFIER, "typeface");
    lexer_destroy(&a);
    a = lex("char double byte real", COMPILER_OK);
    expect(&a, 0, TOKEN_BYTE, "char");
    expect(&a, 1, TOKEN_REAL, "double");
    expect(&a, 2, TOKEN_IDENTIFIER, "byte");
    expect(&a, 3, TOKEN_IDENTIFIER, "real");
    lexer_destroy(&a);
}

/**
 * @brief Tests output keywords and similarly named ordinary identifiers.
 * @note This test takes no arguments and returns no value.
 */
static void test_output_keywords(void)
{
    TokenArray a = lex("print length puts strlen typedef def", COMPILER_OK);
    expect(&a, 0, TOKEN_IDENTIFIER, "print");
    expect(&a, 1, TOKEN_IDENTIFIER, "length");
    expect(&a, 2, TOKEN_IDENTIFIER, "puts");
    expect(&a, 3, TOKEN_IDENTIFIER, "strlen");
    expect(&a, 4, TOKEN_DEF, "typedef");
    expect(&a, 5, TOKEN_IDENTIFIER, "def");
    lexer_destroy(&a);
}

/**
 * @brief Tests that malformed characters and literals produce invalid tokens.
 * @note This test takes no arguments and returns no value.
 */
static void test_invalid_input(void)
{
    TokenArray a = lex("@", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "@"); lexer_destroy(&a);
    a = lex("\"unterminated", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "\"unterminated"); lexer_destroy(&a);
    a = lex("'ab'", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "'ab'"); lexer_destroy(&a);
    a = lex("'\\400' \"\\777\" '\\08'", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "'\\400'");
    expect(&a, 1, TOKEN_INVALID, "\"\\777\"");
    expect(&a, 2, TOKEN_INVALID, "'\\08'");
    lexer_destroy(&a);
    a = lex("1e+", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "1e+"); lexer_destroy(&a);
    a = lex("1f 1.0ff", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "1f");
    expect(&a, 1, TOKEN_INVALID, "1.0ff"); lexer_destroy(&a);
    a = lex("1.0LL 1.0lf 1e+L", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "1.0LL");
    expect(&a, 1, TOKEN_INVALID, "1.0lf");
    expect(&a, 2, TOKEN_INVALID, "1e+"); lexer_destroy(&a);
    a = lex("0x1.0 0x1p+ 0x.p1", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "0x1.0");
    expect(&a, 1, TOKEN_INVALID, "0x1p+");
    expect(&a, 2, TOKEN_INVALID, "0x."); lexer_destroy(&a);
    a = lex("/* never closed", COMPILER_LEX_ERROR);
    expect(&a, 0, TOKEN_INVALID, "/* never closed"); lexer_destroy(&a);

    const char string_with_nul[] = {'"', 'a', '\0', 'b', '"', ';'};
    SourceFile string_source = {"nul-string.bc", string_with_nul, sizeof string_with_nul};
    assert(lexer_run(&string_source, &a) == COMPILER_LEX_ERROR);
    assert(a.count == 3 && a.items[0].kind == TOKEN_INVALID && a.items[0].length == 5 &&
           a.items[1].kind == TOKEN_SEMICOLON && a.items[2].kind == TOKEN_EOF);
    lexer_destroy(&a);

    const char character_with_nul[] = {'\'', '\0', '\'', ';'};
    SourceFile character_source = {"nul-char.bc", character_with_nul, sizeof character_with_nul};
    assert(lexer_run(&character_source, &a) == COMPILER_LEX_ERROR);
    assert(a.count == 3 && a.items[0].kind == TOKEN_INVALID && a.items[0].length == 3 &&
           a.items[1].kind == TOKEN_SEMICOLON && a.items[2].kind == TOKEN_EOF);
    lexer_destroy(&a);

    const char marker_with_nul[] = "/*CRUNCH-PP-MAP:1:file\0name*/\nint x;";
    SourceFile marker_source = {"nul-marker.bc", marker_with_nul, sizeof marker_with_nul - 1};
    assert(lexer_run(&marker_source, &a) == COMPILER_LEX_ERROR);
    assert(a.items[0].kind == TOKEN_INVALID);
    lexer_destroy(&a);
}

/**
 * @brief Runs the lexer unit-test cases.
 * @note Takes no arguments; returns zero after all assertions pass.
 */
int main(void)
{
    test_keywords_identifiers_and_locations();
    test_operators();
    test_literals_and_comments();
    test_structure_and_dim_keywords();
    test_output_keywords();
    test_invalid_input();
    puts("lexer unit tests: OK");
    return 0;
}
