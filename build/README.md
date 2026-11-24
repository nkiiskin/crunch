# Build notes

Run `../configure` from the project root before invoking build targets. The
script verifies the supported Linux x86-64 host, the C11 compiler, GNU make,
GNU assembler/linker, Python 3, and shell, then writes `config.mk`. The Makefile
refuses build/test targets when configuration has not been run; `make clean`
remains available without a configuration file.

The current build produces independently callable lexer, parser, Crunch IR
emitter, and x86-64 assembly translator executables, plus the semantic-analysis
test runner. The IR emitter and assembly translator have separate interfaces:
`crunch-ir FILE.bc` emits `CRUNCH-IR 2`, while `crunch-asm FILE.bir` translates
that format to GNU assembly. LLVM is not detected or required. See
`../docs/ir.md` and `../docs/configure.md`.
