# Repository instructions

## Build and tests

The supported build is Linux x86-64 with a strict C11 compiler, GNU make,
GNU `as`/`ld`, Python 3, and a POSIX shell. From the repository root:

```sh
./configure
make
make check
```

`make check` runs all test units; there is no test-runner option to select one
registered case. To run one semantic fixture after building, invoke the
semantic test driver directly, for example:

```sh
build/out/test-sema tests/fixtures/units/types/invalid_implicit_narrowing.bc expect-error
```

Use `expect-ok` for a valid semantic fixture. For focused stage checks, the
individual tools are in `build/out/`: `crunch-lexer FILE.bc`,
`crunch-parser` (reads the token stream from stdin), `crunch-ir FILE.bc`, and
`crunch-asm FILE.bir`. The C-level lexer test can be run with
`build/out/test-lexer`. Test fixtures and registrations live under
`tests/fixtures/` and in `tests/run-tests.py`; adding a fixture alone does not
register it.

`make clean` removes build outputs. The canonical configuration inputs are
`configure.ac`, `Makefile.am`, and `build/config.mk.in`; after changing them,
regenerate the checked-in Autotools files with `autoreconf -fi`.

## Architecture

Compilation is organized as lexer -> parser -> semantic analysis -> Crunch IR
-> GNU x86-64 assembly. Stage interfaces are declared in `include/`, with
implementations in `src/`; CLI programs in `src/driver/` adapt these APIs
rather than implementing stages separately. `crunch-ir` composes the frontend
through semantic validation and emits the versioned textual `CRUNCH-IR 2`.
`crunch-asm` is a separate backend that validates this IR before generating
assembly. The backend selectively emits the self-contained Linux runtime
helpers required by the IR; it does not depend on LLVM or a separate runtime
library.

The command-line tools and pipeline interchange formats are documented in
`docs/lexer-format.md`, `docs/ast-format.md`, and `docs/ir.md`. Keep machine-
readable output on stdout and diagnostics on stderr. Library stages should
return `CompilerStatus` rather than exiting the process; the CLI adapters
translate failures into diagnostics and exit codes.

## Codebase conventions

- Keep public stage contracts in the matching `include/<stage>/` header and
  stage implementation independent of `main()` and driver options.
- Preserve explicit ownership: pair returned lexer/parser/semantic data with
  its destroy function. Token lexemes refer to the source buffer, so keep the
  `SourceFile` alive until token consumers finish.
- Interchange formats are versioned contracts, not memory dumps. Update their
  corresponding docs and tests when changing serialized output or IR.
- Semantic analysis can return a `SemanticModel` with diagnostics even when it
  reports `COMPILER_SEMA_ERROR`; inspect the diagnostics rather than treating
  every non-success status as an absent model.
- Add source fixtures under the most relevant `tests/fixtures/units/<unit>/`
  directory and register them in `tests/run-tests.py`. Negative tests should
  check rejection by the intended stage; end-to-end tests should assemble,
  link, and execute generated code with the expected exit status. Behavior
  unsupported by the backend belongs in frontend-only tests until executable
  support exists.
- When changing Autotools configuration, edit the maintained inputs rather
  than only their generated `configure`/`Makefile.in` outputs.
- When adding a system call or public syscall wrapper, add and register its
  `3crunch` man page, including installation and distribution coverage.
