# Lexer interface and token stream format (versions 1 through 6)

## C interface

`lexer_run(const SourceFile *, TokenArray *)` returns `COMPILER_OK` on success,
`COMPILER_LEX_ERROR` if one or more invalid tokens were found, or another
`CompilerStatus` for invalid arguments or allocation failure. On lexical errors,
the returned array is still populated with `TOKEN_INVALID` entries and a final
`TOKEN_EOF`, allowing callers to inspect diagnostics. The caller must call
`lexer_destroy()` for every successful allocation, including when the return
status is `COMPILER_LEX_ERROR`.

Token lexemes are slices of the original `SourceFile.data`; the source buffer
must remain alive until the token array is no longer used. Token offsets and
columns are zero-based and one-based respectively: `offset` is a zero-based
byte offset, `line` starts at 1, and `column` starts at 1. Newlines increment
the line and reset the column to 1.

The numeric grammar recognizes decimal integer literals, hexadecimal integer
literals prefixed by `0x` or `0X`, decimal real literals with an optional
leading or trailing decimal point (when at least one digit is present), and
decimal exponents. Hexadecimal real literals use a mandatory `p` or `P`
binary exponent, such as `0x1.8p2` or `0x.8p-1L`.
Thus `.5`, `12.`, `1e3`, and `.5E-2` are real literals. A
standalone `.` remains a `DOT` token. Integers also recognize the
suffixes `U`, `L`, `UL`, `LU`, `LL`, `ULL`, and `LLU` in either case. The two
letters in `LL` must have the same case. Real literals may use
the suffix `f` or `F` to specify `float`, or `l` or `L` for `long double`;
`1f` is not a real literal. Other
numeric suffixes and digit separators are not supported. The `float` keyword
is emitted as `FLOAT`; suffixed literals retain the `REAL_LITERAL` token name
and their suffix in the lexeme. The `unsigned` keyword is emitted as `UNSIGNED`.
The `short` and `signed` keywords are emitted as `SHORT` and `SIGNED`.
GNU `__signed` and `__signed__` spellings are emitted as `SIGNED` as well.
`restrict`, `__restrict`, and `__restrict__` are emitted as `RESTRICT`.
The `_Bool` and `enum` keywords are emitted as `BOOL` and `ENUM`.
The source keywords `char` and `double` retain the version 1 token names
`BYTE` and `REAL`, respectively; `byte` and `real` are ordinary identifiers.
The `struct` and `union` keywords are emitted as `STRUCT` and `UNION`, and `while` retains the version 1
token name `LOOP`; `type` and `loop` are ordinary identifiers. `long` is
emitted as `LONG`; `do` is emitted as `DO`. `sizeof` as `SIZEOF`, `const` as `CONST`, and `typedef` as
the version 1 token `DEF`. `volatile` and `register` are emitted as `VOLATILE`
and `REGISTER`; `auto` is emitted as `AUTO`. The former keywords `def` and `dim` and the removed built-in
names `print` and `length` are ordinary identifiers. `inline` and `goto` are
emitted as `INLINE` and `GOTO`.
`_Noreturn` is emitted as `NORETURN`; `noreturn` is an identifier.
`_Static_assert` is emitted as `STATIC_ASSERT`.
`_Alignof` is emitted as `ALIGNOF`.
`_Alignas` is emitted as `ALIGNAS`.
`_Thread_local` is emitted as `THREAD_LOCAL`.
The parser still accepts the historical `PRINT` and `LENGTH` tokens as names in
version 1 streams; they no longer have built-in behavior.
String and character escape sequences
are validated against the language specification; literal lexemes remain raw
source slices for later decoding by the parser or semantic/literal layer.
Wide character literals are emitted as `WIDE_CHAR_LITERAL`, including the `L`
prefix and quotes in the lexeme. They must contain one ASCII character, one
valid UTF-8 scalar value, or one supported escape as described in the language
specification. Invalid encodings and unsupported values are lexical errors.
Wide string literals are emitted as `WIDE_STRING_LITERAL`, with the same
element encoding and escape rules.
Raw NUL bytes are invalid source characters, including inside quoted literals;
use an escape such as `\0` when a literal needs a NUL byte.
The preprocessor rejects raw NUL bytes in all input lines, including directives
and inactive conditional branches.

Comments are C-style `//` line comments and `/* ... */` block comments.
Nested block comments are not supported.

The lexer consumes canonical `#pragma pack(n)` lines emitted by `crunch-pp`.
Each token, including EOF and closing braces, carries `pack_alignment`: zero
means the default alignment; otherwise it is 1, 2, 4, 8, or 16. Directives
change the state at their position and do not produce tokens.
Preserved `#pragma GCC` option lines produce an `INVALID` token spanning the
directive and a lexical error; they are not applied by the compiler.

`**` is emitted as two consecutive `STAR` records, not a separate token.
The parser recognizes power in infix position only when their byte offsets
are adjacent. Whitespace or a comment between the stars separates them.
The adjacent characters `->` form one `ARROW` token; whitespace or a comment
between them produces separate `MINUS` and `GREATER` tokens.
An ellipsis is three adjacent `DOT` records; the parser recognizes it only
when their byte offsets are consecutive.

## CLI token stream

`crunch-lexer [source-file|-]` reads a file or standard input (the default).
It writes a UTF-8/ASCII textual format to standard output and diagnostics to
standard error. For a version 1 stream, the first line is exactly:

```text
CRUNCH-TOKENS 1
```

Each subsequent line contains five tab-separated fields:

1. Token kind name, as returned by `token_kind_name()`.
2. Zero-based source byte offset.
3. One-based line number.
4. One-based column number.
5. Original lexeme bytes encoded as lowercase hexadecimal; `-` denotes an
   empty lexeme (used by EOF and an unterminated block-comment error).

If any token has nonzero packing and no newer-version feature occurs,
the header is `CRUNCH-TOKENS 2` and each record adds a sixth tab-separated
decimal field, `pack_alignment`.
When any wide character literal occurs without a newer-version feature, the header is `CRUNCH-TOKENS 3`;
records have the same six fields as version 2, including zero alignments.
When an `auto` keyword occurs without a newer-version feature, the header is
`CRUNCH-TOKENS 4`. A wide string literal selects `CRUNCH-TOKENS 5` unless
version 6 is required.
Narrow literals containing a hexadecimal escape with other than two digits
select `CRUNCH-TOKENS 6`. Versions 4 through 6 use the same fields as version 3.
`crunch-parser` accepts all six versions and validates
the alignment values; new token kinds are not accepted in earlier versions.
Versions 1 through 5 retain their original two-digit narrow hex escape
semantics, including when another hex digit follows in the same string token.

Hex encoding ensures that whitespace, tabs, newlines, and arbitrary literal
contents cannot corrupt the record format. The source text itself is not
embedded in the stream; token offsets refer to the original source and are
primarily useful for diagnostics. Exit status is zero on success, one for
lexical errors, and two for input/output or resource failures.

Example:

```sh
printf 'int main() { return 7; }\n' | build/out/crunch-lexer
```
