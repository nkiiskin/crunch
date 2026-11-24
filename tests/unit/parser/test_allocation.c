#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);

static size_t allocation_count, fail_at, live_count;
static int tracking, persistent;

/**
 * @brief Advances fault-injection state and decides whether to fail an allocation.
 * @note Takes no arguments and uses the configured global counters.
 * @return Nonzero when this allocation should fail; otherwise zero.
 */
static int fail_allocation(void)
{
    if (!tracking)
        return 0;
    ++allocation_count;
    return fail_at && (allocation_count == fail_at ||
                      (persistent && allocation_count > fail_at));
}

/**
 * @brief Linker wrapper that injects malloc failures and tracks live allocations.
 * @param size Requested allocation size in bytes.
 * @return Allocated memory, or NULL when fault injection triggers.
 */
void *__wrap_malloc(size_t size)
{
    if (fail_allocation())
        return NULL;
    void *ptr = __real_malloc(size);
    if (tracking && ptr)
        ++live_count;
    return ptr;
}

/**
 * @brief Linker wrapper that injects calloc failures and tracks live allocations.
 * @param count Number of elements requested.
 * @param size Size of each element in bytes.
 * @return Zeroed allocation, or NULL when fault injection triggers.
 */
void *__wrap_calloc(size_t count, size_t size)
{
    if (fail_allocation())
        return NULL;
    void *ptr = __real_calloc(count, size);
    if (tracking && ptr)
        ++live_count;
    return ptr;
}

/**
 * @brief Linker wrapper that injects realloc failures and tracks new allocations.
 * @param ptr Existing allocation, or NULL for a new allocation.
 * @param size Requested allocation size in bytes.
 * @return Resized allocation, or NULL when fault injection triggers.
 */
void *__wrap_realloc(void *ptr, size_t size)
{
    if (fail_allocation())
        return NULL;
    int was_null = ptr == NULL;
    void *grown = __real_realloc(ptr, size);
    if (tracking && was_null && grown)
        ++live_count;
    return grown;
}

/**
 * @brief Linker wrapper that tracks released allocations before freeing them.
 * @param ptr Allocation to release; NULL is ignored by the underlying free.
 * @return No value.
 */
void __wrap_free(void *ptr)
{
    if (tracking && ptr) {
        assert(live_count > 0);
        --live_count;
    }
    __real_free(ptr);
}

/**
 * @brief Runs parser allocation-failure cases and checks for leaks.
 * @param tokens Token stream supplied to the parser.
 * @param failure Allocation index to fail, or zero to run without failure.
 * @param expected Expected parser status.
 * @return Number of allocation attempts made.
 */
static size_t run(const TokenArray *tokens, size_t failure, CompilerStatus expected)
{
    AstProgram *program = NULL;
    allocation_count = live_count = 0;
    fail_at = failure;
    tracking = 1;
    CompilerStatus status = parser_run(tokens, &program);
    if (status != expected) {
        fprintf(stderr, "allocation %zu: expected status %d, got %d\n",
                failure, expected, status);
        abort();
    }
    if (expected != COMPILER_OK)
        assert(program == NULL);
    ast_program_destroy(program);
    if (live_count) {
        fprintf(stderr, "allocation %zu: %zu allocations leaked\n", failure, live_count);
        abort();
    }
    tracking = 0;
    return allocation_count;
}

/**
 * @brief Runs semantic-analysis allocation-failure cases and checks for leaks.
 * @param program AST supplied to semantic analysis.
 * @param failure Allocation index to fail, or zero to run without failure.
 * @param expected Expected semantic-analysis status.
 * @return Number of allocation attempts made.
 */
static size_t run_sema(AstProgram *program, size_t failure, CompilerStatus expected)
{
    SemanticModel *model = NULL;
    allocation_count = live_count = 0;
    fail_at = failure;
    tracking = 1;
    CompilerStatus status = sema_run(program, &model);
    if (status != expected) {
        fprintf(stderr, "semantic allocation %zu: expected status %d, got %d\n",
                failure, expected, status);
        abort();
    }
    if (failure)
        assert(model == NULL);
    else
        assert(model != NULL);
    sema_destroy(model);
    free(program->block_externals.items);
    program->block_externals.items = NULL;
    program->block_externals.count = 0;
    if (live_count) {
        fprintf(stderr, "semantic allocation %zu: %zu allocations leaked\n", failure, live_count);
        abort();
    }
    tracking = 0;
    return allocation_count;
}

/**
 * @brief Exercises lexer allocation failures for valid and invalid input.
 * @note Takes no arguments and returns no value.
 */
static void test_lexer_allocations(void)
{
    char text[1024];
    for (size_t i = 0; i < sizeof(text); ++i)
        text[i] = i % 2 ? ' ' : 'x';
    const SourceFile sources[] = {
        {"many.bc", text, sizeof(text)},
        {"invalid.bc", "@", 1}
    };
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        size_t count = 0;
        for (persistent = 0; persistent <= 1; ++persistent) {
            for (size_t failure = 0; failure <= count; ++failure) {
                TokenArray tokens = {0};
                allocation_count = live_count = 0;
                fail_at = failure;
                tracking = 1;
                CompilerStatus status = lexer_run(&sources[i], &tokens);
                if (!failure) {
                    assert(status == (i ? COMPILER_LEX_ERROR : COMPILER_OK));
                    count = allocation_count;
                    assert(count >= (i ? 1u : 4u));
                } else {
                    assert(status == COMPILER_OUT_OF_MEMORY);
                    assert(tokens.items == NULL && tokens.count == 0);
                }
                lexer_destroy(&tokens);
                assert(live_count == 0);
                tracking = 0;
            }
        }
    }
}

/**
 * @brief Exercises semantic analysis under transient and persistent allocation failures.
 * @note Takes no arguments and returns no value.
 */
static void test_sema(void)
{
    const struct {
        const char *text;
        CompilerStatus status;
    } cases[] = {
        {"int f(int a[restrict], int b[const volatile 2]) {"
         "return a[0] + b[0]; }", COMPILER_OK},
        {"int f(a, b, c) int c; int *a, b; { return *a+b+c; }", COMPILER_OK},
        {"int f(void) { return sizeof(struct { unsigned bit : 1; }); }", COMPILER_OK},
        {"int f(int i) { return __builtin_offsetof("
         "struct { char c; int a[2]; }, a[i]); }", COMPILER_OK},
        {"struct S; extern struct S value;"
         "struct S *f(void) { extern struct S value; return &value; }", COMPILER_OK},
        {"struct S; int f(struct S *p) { return p->x; }"
         "struct S { int x; };", COMPILER_SEMA_ERROR},
        {"struct S; struct S value; struct S { int x; };"
         "int f(void) { return value.x; }", COMPILER_OK},
        {"_Static_assert(1, \"file\"); struct S {_Static_assert(1, \"field\"); int x;};"
         "int f() {_Static_assert(1, \"block\"); return 0;}", COMPILER_OK},
        {"struct S {_Static_assert(0, \"bad field\"); int x;};"
         "int f() {_Static_assert(0, \"bad block\"); return 0;}", COMPILER_SEMA_ERROR},
        {"struct pair { int x, y; }; int global = 1;"
         "int f(int x, int y) { int *p = &x; { int z = *p; y = z; }"
         "for (int i = 0; i < x; i++) { y += i; }"
         "switch (y) { case 1: y = 2; break; default: y = 3; }"
         "while (x) { x--; } return y; }", COMPILER_OK},
        {"int f() { int unused; return 0; unused = 1; }", COMPILER_OK},
        {"int f(int x, int y, int z) { return missing; }", COMPILER_SEMA_ERROR},
        {"int a; int a; struct pair { int x; int x; };"
         "int f() { a = missing; a = missing; a = missing; a = missing;"
         "a = missing; a = missing; a = missing; a = missing; a = missing;"
         "return missing; }", COMPILER_SEMA_ERROR},
        {"struct pair { int x; }; struct pair { int y; };", COMPILER_SEMA_ERROR},
        {"int a[] = {missing}; int b = missing;", COMPILER_SEMA_ERROR},
        {"int f(int x) { if (x) goto done; goto missing; done: return x; }",
         COMPILER_SEMA_ERROR},
        {"static inline int f(int x) { goto done; done: return x; }", COMPILER_OK},
        {"typedef unsigned short Word; short f(Word x) { short y = x; return y; }",
         COMPILER_OK},
        {"typedef unsigned char Byte; signed char f(Byte x) {"
         "signed char y = x; return y; }", COMPILER_OK},
        {"struct item { int x; }; int f() { struct item x;"
         "const struct item y = x; const int *p = &y.x;"
         "int z; const int *q = z ? &z : p; return *q; }", COMPILER_OK},
        {"int f() { const int x = 1; int *p = &x; x++; return x; }", COMPILER_SEMA_ERROR},
        {"typedef int (*Callback)(int); int inc(int x) { return x + 1; }"
         "int f(Callback cb) { return cb(1); }"
         "int main() { Callback cb = inc; return f(cb); }", COMPILER_OK},
        {"int inc(int x) { return x + 1; }"
         "int (*choose(int x))(int) { return inc; }"
         "int main() { return choose(0)(1); }", COMPILER_OK},
        {"int inc(int x) { return x; } typedef int (*C)(int);"
         "C select(int x) { return inc; }"
         "int (*(*f)(int))(int) = select;"
         "int main() { return f(0)(1); }", COMPILER_OK},
        {"typedef int Value, *Pointer; typedef const int Constant;"
         "Value f(Value x) { typedef Value Local; Local y = x; Pointer p = &y;"
         "Constant z = *p; return z; }", COMPILER_OK},
        {"typedef int Value; typedef long Value;", COMPILER_SEMA_ERROR},
        {"enum E { A=-1, B=A+2 }; typedef enum E E;"
         "int f() { enum E x=B; { enum E { A=5 }; int y=A; } return x; }",
         COMPILER_OK},
        {"struct S { enum E { A=1, B=A+1 } e; }; int f() { return B; }",
         COMPILER_OK},
        {"int g=sizeof(enum G { A=2 });"
         "int f(enum P { B=3 } x) { struct S { enum E { C=2 } e; int a[C]; };"
         "struct S s; s.a[0]=(enum T { D=1 })D; return s.a[0]+x+B; }",
         COMPILER_OK},
        {"struct S { int values[3]; int x; }; "
         "struct S a[] = {[2].values[1] = 4, [0].x = 2}; "
         "int f() { int values[] = {[3] = 1, [1] = 2}; return values[1]; }",
         COMPILER_OK},
        {"typedef int A[]; struct S { int n; A values; }; "
         "int f(struct S *s) { return s->values[0]; }", COMPILER_OK},
        {"typedef __builtin_va_list List; List g;"
         "int f(List a) { List b; return sizeof(b) + a[0].gp_offset; }",
         COMPILER_OK},
        {"int f(int n, ...) { __builtin_va_list a, b;"
         "__builtin_va_start(a, n); __builtin_va_copy(b, a);"
         "int x = __builtin_va_arg(b, int); __builtin_va_end(b);"
         "__builtin_va_end(a); return x; }", COMPILER_OK},
        {"struct S { int n; int values[]; int extra; };", COMPILER_SEMA_ERROR},
        {"struct S { unsigned a:1, :2, b:(1 + 2), :0; int c; };",
         COMPILER_OK},
        {"union U { unsigned a:3, b:5; }; union U u = {.b=31, .a=1};",
         COMPILER_OK},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        SourceFile source = {"allocation.bc", cases[i].text, strlen(cases[i].text)};
        TokenArray tokens = {0};
        AstProgram *program = NULL;
        assert(lexer_run(&source, &tokens) == COMPILER_OK);
        assert(parser_run(&tokens, &program) == COMPILER_OK);
        size_t count = run_sema(program, 0, cases[i].status);
        for (persistent = 0; persistent <= 1; ++persistent)
            for (size_t failure = 1; failure <= count; ++failure)
                run_sema(program, failure, COMPILER_OUT_OF_MEMORY);
        ast_program_destroy(program);
        lexer_destroy(&tokens);
    }
}

/**
 * @brief Rejects malformed token arrays and clears parser outputs.
 * @note Takes no arguments and returns no value.
 */
static void test_invalid_token_arrays(void)
{
    Token missing_eof[] = {
        {.kind = TOKEN_INT, .lexeme = "int", .length = 3}
    };
    Token early_eof[] = {
        {.kind = TOKEN_EOF, .lexeme = "", .length = 0},
        {.kind = TOKEN_EOF, .lexeme = "", .length = 0}
    };
    Token null_lexeme[] = {
        {.kind = TOKEN_IDENTIFIER, .lexeme = NULL, .length = 1},
        {.kind = TOKEN_EOF, .lexeme = "", .length = 0}
    };
    Token empty_lexeme[] = {
        {.kind = TOKEN_INT, .lexeme = "", .length = 0},
        {.kind = TOKEN_EOF, .lexeme = "", .length = 0}
    };
    Token invalid_kind[] = {
        {.kind = (TokenKind)-1, .lexeme = "x", .length = 1},
        {.kind = TOKEN_EOF, .lexeme = "", .length = 0}
    };
    Token embedded_nul[] = {
        {.kind = TOKEN_IDENTIFIER, .lexeme = "a\0b", .length = 3},
        {.kind = TOKEN_EOF, .lexeme = "", .length = 0}
    };
    Token eof_with_lexeme[] = {
        {.kind = TOKEN_EOF, .lexeme = "x", .length = 1}
    };
    TokenArray cases[] = {
        {missing_eof, 1}, {early_eof, 2}, {null_lexeme, 2},
        {empty_lexeme, 2}, {invalid_kind, 2}, {embedded_nul, 2}, {eof_with_lexeme, 1},
        {NULL, 0}
    };
    AstProgram marker = {0};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        AstProgram *output = &marker;
        size_t error_count = 1;
        bool nesting_limit = true;
        assert(parser_run_with_errors_and_depth(&cases[i], &output, NULL, 0, &error_count,
                                                &nesting_limit) == COMPILER_INVALID_INPUT);
        assert(output == NULL && error_count == 0 && !nesting_limit);
    }
    AstProgram *output = &marker;
    assert(parser_run(NULL, &output) == COMPILER_INVALID_INPUT && output == NULL);
}

/**
 * @brief Runs parser and semantic-analysis allocation-failure tests.
 * @param argc Number of command-line arguments; accepts optional --sema or --lexer.
 * @param argv Command-line argument vector.
 * @return Zero when all checks pass; assertions or failures terminate the test.
 */
int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--lexer") == 0) {
        test_lexer_allocations();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--sema") == 0) {
        test_sema();
        return 0;
    }
    test_invalid_token_arrays();
    const struct {
        const char *text;
        CompilerStatus status;
    } cases[] = {
        {"int f(int a[restrict], int b[const volatile 2]) {"
         "return a[0] + b[0]; }", COMPILER_OK},
        {"int f(int a[3][restrict 2]);", COMPILER_PARSE_ERROR},
        {"int a[restrict];", COMPILER_PARSE_ERROR},
        {"int f(a, b, c) int c; int *a, b; { return *a+b+c; }", COMPILER_OK},
        {"int f(a, b) int a; { return a+b; }", COMPILER_OK},
        {"int f(a, a) int a; { return a; }", COMPILER_PARSE_ERROR},
        {"int f(a) int a; int a; { return a; }", COMPILER_PARSE_ERROR},
        {"int f(a) int b; { return a; }", COMPILER_PARSE_ERROR},
        {"int f(a) float a; { return 0; }", COMPILER_PARSE_ERROR},
        {"int f(a) int *; { return 0; }", COMPILER_PARSE_ERROR},
        {"int f(a);", COMPILER_PARSE_ERROR},
        {"int f(void) { return sizeof(struct { unsigned bit : 1; }); }", COMPILER_OK},
        {"int f(void) { return sizeof(struct { unsigned bit : ; }); }", COMPILER_PARSE_ERROR},
        {"int f(int i) { return __builtin_offsetof("
         "struct { char c; int a[2]; }, a[i]); }", COMPILER_OK},
        {"int f(void) { return __builtin_offsetof("
         "struct { int a[2]; }, a[); }", COMPILER_PARSE_ERROR},
        {"char text[] = \"\\1\" \"\" \"7\";", COMPILER_OK},
        {"int text[] = L\"\\u03a9\" \"abc\";", COMPILER_OK},
        {"typedef __builtin_va_list List; List g;"
         "int f(List a) { List b; return sizeof(b) + a[0].gp_offset; }",
         COMPILER_OK},
        {"int f(int n, ...) { __builtin_va_list a, b;"
         "__builtin_va_start(a, n); __builtin_va_copy(b, a);"
         "int x = __builtin_va_arg(b, int); __builtin_va_end(b);"
         "__builtin_va_end(a); return x; }", COMPILER_OK},
        {"int f(int n, ...) { __builtin_va_list a; return __builtin_va_arg(a,); }",
         COMPILER_PARSE_ERROR},
        {"struct S; struct S; union U; struct S { int x; };"
         "int f() { struct S; struct S *p = 0; return p != 0; }", COMPILER_OK},
        {"struct S { const int **p; struct { int value; } **q; };"
         "typedef const int **P; typedef struct { int value; } **Q;"
         "const int **global;", COMPILER_OK},
        {"struct pair { int x, y; char text[4]; struct pair next; };"
        "int *a, b; struct pair item;"
        "int helper(int x, char *names[]) { return x; }"
        "int main() { int x = 1, y = 2; char text[] = \"abc\";"
        "double f = 1.25; int values[] = {1, 2, 3};"
        "x = helper(x + y, text); item.x++; text[0] = 'a';"
        "if (x) { x += 1; } else x = -y;"
        "for (x = 0; x < 3; x++) { if (x) continue; }"
        "for (int z = 0, w = 1; z < w; z++) break;"
        "while (x) { x--; } switch (x) { case 1: x = 2; break; default: x = 0; }"
        "item.text[0] = 'x'; values[1] = values[0];"
        "x = (int)sizeof(values); x = (int)f;"
        "return x ? ~y : !x; }", COMPILER_OK},
        {"int f() { for (;;) ; return 0; }", COMPILER_OK},
        {"_Static_assert(1, \"file\"); struct S { int x; _Static_assert(1, \"field\"); };"
         "int f() { _Static_assert(1, \"block\"); return 0; }", COMPILER_OK},
        {"struct S { _Static_assert(1, 2); int x; };", COMPILER_PARSE_ERROR},
        {"static inline int f(int x) { goto done; switch (x) {"
         "done: return x; case 0: return 0; } }", COMPILER_OK},
        {"int f() { goto ; }", COMPILER_PARSE_ERROR},
        {"struct pair { int x, y; }; struct pair a, b;"
         "int f() { a.x = b.y; return a.x; }", COMPILER_OK},
        {"int f() { for (int x = 0; x; x--) ; return f()(1)[0]++; }", COMPILER_OK},
        {"int ;", COMPILER_PARSE_ERROR},
        {"int f(int) { return 0; }", COMPILER_PARSE_ERROR},
        {"typedef int (*Callback)(int, int (*)(long));"
         "struct S { int (*field)(int), value; };"
         "int f(int, Callback); int (*global)(int);"
         "int main() { int (*local)(int); return 0; }", COMPILER_OK},
        {"typedef int (*Callbacks[2])(int);"
         "struct S { int (*field[2][3])(int); };"
         "int run(int (*callbacks[2])(int)) { return callbacks[0](1); }"
         "int main() { int (*local[2])(int); return 0; }", COMPILER_OK},
        {"int (*select(int))(int);"
         "int (*select(int x, int (*callback)(int)))(int) { return callback; }"
         "int (*empty(void))(int) { return 0; }", COMPILER_OK},
        {"int (*select(int x)(int) { return 0; }", COMPILER_PARSE_ERROR},
        {"typedef int (*(*Factory)(int))(int);"
         "typedef int Unary(int); int apply(int callback(int)); int (*higher)(int (int));"
         "struct S { int (*(*field)(int))(int); };"
         "int (*(*choose(int x))(int))(int) { return 0; }"
         "int main() { Factory f; return sizeof(int (*(*)(int))(int)); }", COMPILER_OK},
        {"int (*(*broken)(int))(int;", COMPILER_PARSE_ERROR},
        {"typedef int A[]; struct S { int n; A values; };", COMPILER_OK},
        {"struct S { unsigned a:1, :2, b:(1 + 2), :0; int c; };", COMPILER_OK},
        {"struct S { unsigned a:; };", COMPILER_PARSE_ERROR},
        {"typedef int (*Callback)(int, int (*)(long);", COMPILER_PARSE_ERROR},
        {"struct pair { int ; };", COMPILER_PARSE_ERROR},
        {"int f() { for (int x = 1, ; x; x--) ; }", COMPILER_PARSE_ERROR},
        {"int f() { int dim = 1; return dim; }", COMPILER_OK},
        {"const { int x = 1, y = 2; char text[] = \"hi\";"
         "const { double z = 1.5; }; };"
         "int f() { const { int x = 1; int y = 2; }; return x + y; }", COMPILER_OK},
        {"const { int x = 1;", COMPILER_PARSE_ERROR},
        {"static { int x, y; char text[]; const { int z = 1; };"
         "static { int n; }; }; int main() { return x + y + z + n; }",
         COMPILER_OK},
        {"static const { int x, y; char text[]; static { int n; }; };"
         "int main() { return x + y + n; }", COMPILER_OK},
        {"const int *const p = 0; int const a = 1, b = 2;"
         "struct item { const int x; const char *text; };"
         "const struct item *const q = 0;"
         "int f(const int *const x) { const int *p = (const int*)x; return *p; }",
         COMPILER_OK},
        {"typedef int Value, *Pointer, *const Fixed; typedef Value Other;"
         "struct item { Value value; }; typedef struct item Item;"
         "Value f(Value x) { typedef long Value; Value y = 1;"
         "{ typedef Other Local; Local z = (Local)x; }"
         "for (int Value = 0; Value < 1; Value++) { } return x; }", COMPILER_OK},
        {"typedef const struct point { double x, y; } Point, *PointPtr;"
         "int main() { Point p; PointPtr ptr = &p; return 0; }", COMPILER_OK},
        {"volatile typedef int Value, *Pointer; volatile static int x;"
         "int f() { Value y = 1; Pointer p = &y; return *p + x; }", COMPILER_OK},
        {"typedef struct point { int x; } Point, *;", COMPILER_PARSE_ERROR},
        {"typedef unsigned short int Word; short f(Word x) {"
         "short y[2] = {1, (short)x}; return y[0]; }", COMPILER_OK},
        {"typedef char unsigned Byte; signed char f(Byte x) {"
         "signed char y[2] = {-1, (signed char)x}; return y[0]; }", COMPILER_OK},
        {"typedef struct { int x; struct { char c; long y; }; } Point;"
         "int f() { typedef struct { int x; } Local; Local a;"
         "struct { int y; } b; return 0; }", COMPILER_OK},
        {"typedef struct { struct { int x; }; } Point, *;", COMPILER_PARSE_ERROR},
        {"typedef int Value, *;", COMPILER_PARSE_ERROR},
        {"enum E { A=-1, B=A+2 }; typedef enum E E;"
         "int f() { enum E x=B; { enum E { A=5 }; int y=A; } return x; }",
         COMPILER_OK},
        {"struct S { enum E { A=1, B=A+1 } e; }; int f() { return B; }",
         COMPILER_OK},
        {"int g=sizeof(enum G { A=2 });"
         "int f(enum P { B=3 } x) { struct S { enum E { C=2 } e; int a[C]; };"
         "struct S s; s.a[0]=(enum T { D=1 })D; return s.a[0]+x+B; }",
         COMPILER_OK},
        {"struct S { int values[3]; int x; }; "
         "struct S a[] = {[2].values[1] = 4, [0].x = 2}; "
         "int f() { int values[] = {[3] = 1, [1] = 2}; return values[1]; }",
         COMPILER_OK},
        {"int a[] = {[2] 1};", COMPILER_PARSE_ERROR},
        {"enum E { A=1, B= };", COMPILER_PARSE_ERROR},
        {"typedef int *P; struct outer { struct { int n; } *p, value;"
         "int *x, y, *items[2]; };"
         "int main() { static int n = 1, *p = &n; P a = p, *b = &a;"
         "return **b; }", COMPILER_OK},
        {"int x, *;", COMPILER_PARSE_ERROR},
        {"struct item { int x, *; };", COMPILER_PARSE_ERROR},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        SourceFile source = {"allocation.bc", cases[i].text, strlen(cases[i].text)};
        TokenArray tokens = {0};
        assert(lexer_run(&source, &tokens) == COMPILER_OK);
        size_t count = run(&tokens, 0, cases[i].status);
        for (persistent = 0; persistent <= 1; ++persistent)
            for (size_t failure = 1; failure <= count; ++failure)
                run(&tokens, failure, COMPILER_OUT_OF_MEMORY);
        lexer_destroy(&tokens);
    }
    return 0;
}
