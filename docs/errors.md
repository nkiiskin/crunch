# Diagnostics and error handling

Command-line diagnostics from `crunch-ir` and `crunch-asm` use the common `Diagnostic` record and renderer in `common/diagnostic.h`. Each diagnostic carries a severity, stable stage code, message, and source or IR location where available. Stage-specific detection remains in the lexer, parser, semantic analyzer, IR generator, and backend.

## Command-line behavior

- `crunch-lexer FILE.bc` reports lexical errors and returns a non-zero status when input cannot be tokenized.
- `crunch-parser` reads token records from standard input and reports parse errors to standard error.
- `crunch-ir [--module] [--c11-numeric] [-Wunused-variable] [-Wcase-fall-through] [-Wall] [FILE.bc|-]` (standard input when omitted or `-`) runs the frontend and semantic analysis before emitting IR. Diagnostics include the source filename and location when available, with codes such as `E_LEX_INVALID_TOKEN`, `E_PARSE`, `E_SEMANTIC`, and `E_IR_GENERATION`. All lexer invalid-token locations and semantic diagnostics are reported; parser diagnostics report each recorded syntax-error location. Parser and semantic diagnostics are followed by the offending source line (after macro expansion) and a `^` caret under the reported column; tabs before the column are preserved so the caret lines up. Failed compilation must be treated as unsuccessful even if diagnostic text is also written to standard error.
- `crunch-asm [FILE.bir|-]` reports malformed or unsupported IR with a `crunch-asm:` prefix, stable `E_IR` code, and IR line/function context when available; it returns non-zero.

### Warnings

`crunch-ir` reports non-fatal warnings (`W_SEMANTIC`, severity `warning`) that do not change the exit status or suppress IR output. By default these include `unreachable code` for the first statement after a `return`, `break`, or `continue` in the same statement list. `-Wcase-fall-through` enables `case falls through from the previous label` when a non-empty `case` body does not end in `break`, `return`, `continue`, or `goto` (stacked empty labels are exempt). `-Wunused-variable` enables `unused variable 'NAME'` for an unreferenced local variable; parameters are exempt. Both optional warnings are disabled by default and enabled by `-Wall`. These options also work on the `crunch` driver. The driver's `-Wall` additionally enables `-Wabi-mismatch` at link time; see [configuration](configure.md).

They appear as `semantic warning: MESSAGE [W_SEMANTIC]`, with the diagnostic
code retained after the message.

For the library `printf`, a literal format using supported conversions also
produces a warning if the number of supplied values differs from the number
of conversions. `%%` consumes no value and scanning stops at the first NUL.
Computed or unsupported formats and user-defined `printf` functions are not
checked by this warning. It does not check conversion argument types.

## IR validation currently performed by the backend

Before returning assembly, the backend checks the IR v2, v3, v4, or v5 header, function boundaries, duplicate function names, duplicate labels, branch-target spelling, undefined branch targets, and branches that target labels in another function. V2 and v5 require a defined `main`; v3 and v4 permit modules without one and validate typed external function declarations. V4 and v5 require a valid internal initializer function. A control-flow-aware stack verifier additionally rejects unknown instructions (including unreachable ones), unknown function calls, incorrect call argument counts, stack underflow, inconsistent stack depth at branch merges, and reachable fall-through past a function's `end`. Instruction-level diagnostics include the IR line and function; structural errors include their IR line where available.

This is an incremental safety boundary, not a complete proof that arbitrary IR is valid. The verifier checks stack depth and abstract stack types but does not establish pointer validity or general memory safety. Do not treat hand-authored IR as trusted input.

## Test expectations

The `make check` suite verifies that malformed IR is rejected with a non-zero exit status and a backend diagnostic. The test runner also imposes a per-command timeout so a hung compiler or generated program fails the test suite rather than blocking it indefinitely.

## Diagnostic improvements

The shared renderer standardizes severity and optional source or IR location formatting. Diagnostic codes identify the stage/category; exact message text remains owned by the detecting stage.


## Typed IR verification

`crunch-asm` performs an abstract stack-type pass after structural and stack-depth validation. Diagnostics identify the IR line and function for incompatible operation operands, casts, storage types, call arguments, and stack types at control-flow merges. Integer literals are context-polymorphic, and integer-width normalization is permitted where IR v2 defines it. Passing verification does not establish pointer validity or general memory safety.
