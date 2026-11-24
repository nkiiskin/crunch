# Semantic analysis: symbols and scopes (milestone 0.7)

The semantic phase implements declaration registration, basic name resolution, scope checking, and initial expression type checking. See `sema-types.md` for type rules.

## Public API

`sema_run(AstProgram *, SemanticModel **)` builds the semantic model. It returns `COMPILER_OK` when no semantic diagnostics are found, `COMPILER_SEMA_ERROR` when the program contains semantic errors, and other compiler statuses for invalid arguments or resource failures. On semantic errors, the output model is still returned so callers can inspect diagnostics. The analyzer never exits the process.

Use `sema_diagnostic_count()` and `sema_diagnostic_at()` to inspect diagnostics. Diagnostic locations are one-based line and column positions from the AST. The AST must remain alive while semantic analysis runs; the semantic model owns its diagnostic messages but does not own the AST.

## Names and scopes

- Global variables and functions share the ordinary identifier namespace.
- Named structures occupy a separate structure-tag namespace, so `struct Point` does not conflict with a variable named `Point`.
- Structure fields have their own namespace per structure. Duplicate field names are rejected.
- Compatible function prototypes may be repeated and may be followed by one definition. Parameter names do not affect compatibility. Conflicting signatures and multiple definitions are rejected.
- Duplicate variables or parameters in the same scope are rejected.
- Nested blocks introduce nested scopes. A declaration in an inner scope may shadow a name from an outer scope; lookup selects the nearest declaration.
- The function body's outer block shares a scope with its parameters. A `for` statement introduces a scope covering its initializer, condition, update, and body.
- All global declarations are registered before function bodies are analyzed, so calls to functions declared later in the file can resolve.
- Unknown structure type names, undefined identifiers, calls to undefined functions, and attempts to call a non-function identifier are diagnosed.
- `break` outside a `for` or `while` is diagnosed.

## Current limitations

Definite initialization, all return-path analysis, complete `main` ABI validation, and standalone JSON-AST semantic filtering are not checked yet. The standalone parser emits JSON AST, but a standalone JSON-AST reader/semantic filter is not part of this milestone; the semantic API currently consumes the in-memory AST. The test harness exercises the complete lexer/parser/semantic library chain in-process for each `.bc` fixture.
