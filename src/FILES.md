# Source layout

`src/common/` contains source-file handling, diagnostics, constant-expression
evaluation, and common compiler status definitions. `src/lexer/` implements
lexical analysis and token serialization. `src/parser/` constructs the AST and
can serialize it as JSON.
`src/sema/` implements symbol/scope management and semantic checks.

`src/ir/ir.c` translates the supported AST subset into versioned textual
Crunch IR. `src/codegen/x86_64.c` independently translates that IR into GNU
x86-64 assembly. `src/driver/ir_main.c` and `src/driver/asm_main.c` are thin
command-line adapters; front-end stage modules themselves do not define
`main()`.

`src/driver/pp_main.c` implements the standalone include-expanding
preprocessor command. `src/driver/crunch_main.c` implements the `crunch`
compiler driver, which runs the stage tools and the configured GNU `as`/`ld`.

The current backend milestone supports `int main()`, integer scalar locals,
basic arithmetic, simple assignments, and returns. Control flow, function
calls, real values, and memory aggregates are planned extensions to the owned
IR and backend rather than dependencies on a third-party IR implementation.

The x86-64 backend emits no helper code; runtime and standard-library functions are implemented as
separate assembly objects under `lib/` and collected into `libcrunch.a`.
