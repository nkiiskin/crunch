# Crunch compiler architecture

## Goals

Crunch is a sequence of small compiler stages. Frontend and backend stages
expose C interfaces, and standalone tools expose their supported command-line
operations:

1. As a C library function called by another stage.
2. From a small standalone `main()` that calls that stage's public interface.
3. Where applicable, as a command-line filter reading standard input and
   writing its result to standard output.

The output target is GNU assembler source in AT&T syntax for x86-64 Linux.
The driver can also invoke GNU `as` and `ld` to produce objects and executables.

## Stage boundaries

The source pipeline begins with the standalone preprocessor, which expands
`#include <filename>` directives from its configured include directory and
`#include "filename"` directives relative to the including file:

```text
source text -> crunch-pp -> lexer -> parser -> semantics -> IR -> GNU x86-64 assembly
```

The `crunch` driver runs this whole pipeline: `crunch-pp | crunch-ir |
crunch-asm | as`, followed by `ld`. The first stages are connected by pipes
(`crunch-ir` and `crunch-asm` read `/dev/stdin`, although they also accept `-`
or no argument for standard input). Each Crunch source is compiled separately
to a temporary object; each `.s` input is assembled directly and each `.S`
input is preprocessed in assembly mode and then assembled, each into its own
temporary object. Supplied `.o` files go directly to the linker. With an
assembly input, an explicit `.o` output selects a
relocatable link; `-c` selects it for any inputs. Executable links require a
global `_start` from Crunch-generated code or supplied objects. The stage
tools are found next to the `crunch` executable, and the `as`/`ld` paths are
compiled in from `configure`.
Diagnostics of a stage are shown only if all earlier stages succeeded.

The preprocessor performs textual include expansion and supports nested
`#if`/`#elif`/`#ifdef`/`#ifndef`/`#else`/`#endif` conditional sections. Both valueless
`#define` and `#macro` names are tested by these
conditions, and `#undef NAME` removes either kind of definition.
Replacement macros use `#define NAME replacement` or
`#define NAME(parameters) replacement`; `#macro` supports the same forms.
function-like macros support zero or more parameters, nested invocations, and
identifier-based parameter substitution outside strings and comments.
A valueless `#define NAME` expands to nothing and remains defined for include guards.
Expression conditions use macro expansion, `defined`, and C-style 64-bit
integer operations with short-circuit evaluation. Backslash-newline continues
`#define` and `#macro` definitions without inserting whitespace.
`#error` reports an active preprocessing error, `#warning` reports a nonfatal
warning, and `#line` changes the logical
source file and line used for predefined macros and downstream diagnostics.
Function-like macros with parameters support limited `##` concatenation of
nonempty operands; empty operands, object-like and zero-parameter token
pasting and full token validation are not supported. Function-like macros also
support stringification of unexpanded arguments. The preprocessor reads source from a file or
standard input, writes expanded source to standard output, and adds
comment-form source markers so `crunch-ir` diagnostics retain the original file
and line through nested includes.

The in-process interfaces pass typed C data structures and explicit ownership:

- The lexer reads a `SourceFile` and returns a `TokenArray`.
- The parser reads a `TokenArray` and returns an `AstProgram`.
- Semantic analysis reads the AST and returns a `SemanticModel`, including
  diagnostics on semantic errors.
- IR generation checks and emits the AST as textual IR to a caller-owned
  `FILE *`.
- Code generation validates textual IR from a `FILE *` and writes assembly to
  a caller-owned `FILE *`.

Each public stage interface belongs in `include/<stage>/`. Implementations must
not depend on `main()` or the command-line driver. Standalone executables are
thin adapters around the same public functions, not separate implementations.

## Shell filter contract

A shell pipeline needs a stable serialized representation between processes;
raw C structs must never be written to a pipe. Each filter follows these rules:

- Read its input from standard input and write its primary result to standard
  output, so ordinary pipelines work without temporary files.
- Send diagnostics and progress messages to standard error, never into the
  machine-readable output stream.
- Return zero on success and nonzero on failure.
- Do not close standard input or standard output owned by the caller.
- Support a file argument as a convenience, while treating `-` or no file
  argument as standard input where that makes sense for the stage.
- Use documented, versioned textual interchange formats where stages exchange
  tokens, AST, or IR. Formats must represent arbitrary source lexemes safely
  and must not depend on compiler-specific struct layout or pointer values.

The serialized formats are separate from the public C API, so the internal
representation can evolve without silently breaking shell pipelines.

Example frontend and backend pipeline:

```sh
crunch-pp hello.c | crunch-ir | crunch-asm > hello.s
```

`crunch-ir` runs the lexer, parser, and semantic analyzer in-process; it reads
source, not the parser's JSON output. `crunch-lexer | crunch-parser` is a
separate token-to-AST pipeline.

## Error handling and ownership

Frontend stages return a `CompilerStatus`; the backend reports success or
failure with an integer status. A stage must not report success with a
partially valid result. Each allocated result has a matching destroy function
or a clearly documented owner. Source buffers must outlive token arrays whose
lexemes point into those buffers.

Diagnostics should include a source location when one is available. Library
functions should not terminate the process or print directly to standard
output. CLI adapters translate statuses and diagnostics into stderr messages
and exit codes.

## Build organization

Stage implementations are compiled into reusable objects. Standalone tools
link the common support and stages they need; the main driver starts those
tools as separate processes. This keeps the modules independently testable and
allows custom programs to call their interfaces without a compiler `main()`.
Programs linking `src/ir/ir.c` must also link `lib/pow.s` (or its object from
`libcrunch.a`) for compile-time evaluation of static power initializers.

## Parser and token-stream filter

`parser_run()` accepts the in-memory `TokenArray` produced by `lexer_run()` and
returns a `CompilerStatus`; it never terminates the process. The standalone
`crunch-parser` executable consumes versioned `CRUNCH-TOKENS` streams from
standard input, so it can be used as `cat file.bc | crunch-lexer | crunch-parser`.
It emits a complete feature-versioned `CRUNCH-AST` JSON document on standard output
and reports failures on standard error; see `ast-format.md`.

The parser handles primitive and pointer types, fixed-array declarators,
function prototypes and definitions, named structure definitions, variable
declarations, blocks, `if`/`else`, `for`, `while`, `break`, `return`, `goto` and labels,
explicit casts, calls, indexing, member access,
unary/postfix operators, conditional expressions, assignments, and standard
operator precedence. Semantic checks and code generation are separate phases.
Anonymous structure types and some advanced declarator forms are not
implemented. Header
inclusion and basic include guards are handled by the standalone preprocessor
before lexical analysis.


## Runtime helpers and static library

The backend emits a local static-array ownership check when needed, as well as
calls to `__crunch_*` runtime symbols. Assembly-backed runtime functions
(`alloc_bytes`, `free_bytes`, `print`, `end`, `memcpy`, `memset`, `memcmp`,
`strcmp`, `strlen`, the internal `printf` formatters, `printf`, and `env_get`)
live in separate `lib/` objects collected into `libcrunch.a`; only the members
used by a program are linked.
The low-level file and memory syscall implementations live in `lib/syscalls/`
and are assembled into separate archive members alongside their wrappers.
Declarations remain in the corresponding standard headers, including
`<environment.h>`, and
calling contracts are documented in `docs/ir.md`. String lengths use `strlen`
from `<string.h>`; source output uses `printf` from
`<stdio.h>`. The `print` runtime helper remains available for legacy IR only.

The explicit `malloc` allocator uses 64 KiB arenas for requests up to 256
bytes and individual mappings for larger requests. Each arena serves one of
12 user-capacity classes (16-byte steps through 128, then 32-byte steps through
256). A 32-byte header precedes each returned pointer and records its kind,
requested size, arena pointer or mapping length, and capacity. Free blocks
form per-arena lists; class lists contain arenas with available blocks. Full
arenas rejoin the class list on their first free, and empty arenas are unmapped.
If unmapping an empty arena fails, it remains on the class list for reuse and
the error is exposed through `errno`. If this occurs during a moving `realloc`,
the original block is restored and the resize fails. If unmapping a temporary
large replacement also fails during rollback, that mapping is retained for
reuse by a later compatible large allocation. Retained mappings form a
first-fit list with no size limit or automatic unmap retry. `allocator_trim`
explicitly retries retained mappings and empty arenas, removing successful
unmaps and retaining failures; it continues after failures and reports the
first error. Without reuse or a successful trim, they remain mapped until
process exit.
Pool state is process-global and is not synchronized. Dynamic arrays use the
separate `alloc_bytes` and `free_bytes` helpers, not these pools.


### Local structure layout

Local structures are emitted as aligned stack-frame blocks, with typed aliases at the same calculated offsets used for global structure objects. The backend supports `localblock`, `localalias`, `lload`, `lstore`, and `laddr` IR forms for these blocks. Fixed-array fields occupy inline storage and are copied with their containing structure.
