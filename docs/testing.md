# Crunch test suite

`make check` runs the test suite in named units. Each fixture is reported
individually as `UNIT / filename: passed` or `failed`; ANSI green/red colors can
be controlled with `CRUNCH_COLOR=always`, `CRUNCH_COLOR=never`, or
`CRUNCH_COLOR=auto`.

The GitHub Actions workflow runs `./configure`, `make`, and `make check` on
Ubuntu x86-64 for pushes and pull requests. It can also be started manually
from the repository's Actions tab. Results and the full test output are
available in each workflow run.

The suite vendors the assembler and linker in their existing source locations
from [Crunch assembler](https://github.com/nkiiskin/crunch-assembler).
`vendor/crunch-assembler.rev` pins the upstream commit and
`vendor/crunch-assembler.sha256` records the vendored files. CI checks the
hashes, compares those files with the pinned upstream commit, and separately
builds and tests that standalone project. The compiler's `make check` exercises
the vendored tools together with the compiler and runtime.

Develop and test tool changes in the standalone project first, then publish
the commit. From the compiler checkout, update to its full commit ID:

```sh
tools/update-crunch-assembler <commit>
sha256sum -c vendor/crunch-assembler.sha256
./configure && make && make check
CRUNCH_TEST_TOOLCHAIN=crunch make check
```

The updater fetches that exact commit, verifies the existing hashes, refuses
uncommitted changes to vendored files or tracked pin metadata, and reads all
listed files before updating them and the pin. The checksum manifest is also
the list of upstream-owned files; review changes to it separately. A local
repository can be supplied with `--source /path/to/checkout`, but CI requires
the pin to be published in the upstream repository. Normal builds do not need
Git or network access; the updater regression tests in `make check` require Git.

Review compiler driver and runtime build calls for interface changes on each
update. The compiler's shared `include/common/compiler.h` and
`include/version.h` are maintained separately and checked through the suite
build, tests and self-bootstrap.

The longest tests are excluded by default. To include the two-generation
bootstraps and exhaustive binary80 formatting tests, run:

```sh
CRUNCH_TEST_SLOW=1 make check
```

Their test code and fixtures remain in the tree.

To exercise the experimental native linker through the driver and direct
execution-stage links, run `CRUNCH_TEST_LINKER=crunch make check`. The default
is `CRUNCH_TEST_LINKER=gnu`. Native mode uses a temporary driver configuration;
tests that explicitly check configuration, installation, or GNU interoperability
keep their own tool choices. The differential runtime corpus compares both
linkers. GNU tools remain necessary for reference comparisons, C compiler
tooling, and assembler-specific tests.

For the full native execution mode, run:

```sh
CRUNCH_TEST_TOOLCHAIN=crunch make check
```

This selects `crunch-as` for the executable fixture pipelines and the default
driver configuration, `crunch-ld` for execution-stage links, and a separate
test runtime assembled by `crunch-as` and archived by `crunch-ar`, regardless
of the configured runtime archiver. A test-only driver links that runtime
without changing the normal build or installed driver. Explicit GNU
interoperability, configuration, and installation tests retain their own tool
choices. The mode includes the existing differential corpus against GNU tools;
it does not remove the build or test dependency on GNU binutils. An explicitly
set `CRUNCH_TEST_LINKER` must be `crunch` in this mode.

When configured with `--enable-native-only`, `make check` instead runs a
separate native-only suite covering the driver, native assembly, static
archives, relocatable and executable linking, frontend checks and executable
fixtures. The lexer, parser, AST JSON and semantic fixture checks are shared
with the default suite. It also loads native shared objects using the system
dynamic loader,
checking GOT and PLT symbol resolution, initialized data pointers, constructor
and destructor order, weak references, interposition and rejection of
unsupported dynamic features, as well as shared dependencies, SONAME handling,
library search and archive order.
It also loads the shared Crunch runtime and exercises its exports, private
`errno`, registry helper and use as a shared-library dependency.
The suite places failing substitutes for binutils on `PATH` to catch
accidental direct use. It does not run GNU differential or interoperability
tests; use a normal configuration and `make check` for those. The host C
compiler may still invoke system tools while building Crunch.

The normal suite generates programs from four fixed seeds, compiles them with
Crunch and the host C compiler, and compares their output and exit status.
Each program exercises unsigned expressions, bounded `for` and `while`
loops, branches, initialized local and structure-member arrays, and function
calls including structure arguments. Arithmetic is unsigned, shifts are
bounded, divisors are nonzero, and array indices remain in bounds. A failure
reports the seed and complete generated source so it can be reproduced and
saved as a regression fixture. To add 32 deterministic seeds, run:

```sh
CRUNCH_TEST_DIFFERENTIAL_EXTENDED=1 make check
```

The native-only suite skips this host-compiler comparison.

To inventory the C headers visible to the installed GCC and classify whether
they preprocess independently with GCC, run:

```sh
python3 tests/gnu_header_inventory.py > header-inventory.json
```

Use `--cc /path/to/gcc` to select a driver and `--timeout SECONDS` to change
the per-header, per-profile limit. The JSON records the compiler version and
target, host glibc version when available, include search order, selected and
shadowed paths, and preprocessing results for default, `_POSIX_C_SOURCE=200809L`
and `_GNU_SOURCE=1` profiles, with summary counts. Diagnostics are limited to
2,000 characters per probe. Both discovery and probes use `-std=gnu11
-funsigned-char`, with include-path environment overrides removed.

The inventory covers `.h` files reachable through GCC's C angle-include
directories; those directories can also contain third-party headers. Directory
symlinks are followed, except when they would create a traversal cycle. Namespace
labels `internal-candidate` and `public-candidate` are heuristics, not an
authoritative GNU ownership or public-API classification. A header failing all
three standalone probes may need another context, options or prerequisites;
timeouts, crashes and unsafe include spellings are reported separately.
Headers behind include guards or conditionals may still have untested paths.
This inventory does not itself compare Crunch or test frontend compatibility.

To compare a representative set of headers with GCC:

```sh
python3 tests/gnu_header_compare.py > header-comparison.json
```

Use `--header stdio.h` (repeatable) to choose individual headers. For an
opt-in scan of the entire inventory, including third-party headers, run:

```sh
python3 tests/gnu_header_compare.py --inventory header-inventory.json --all > header-comparison.json
```

Both tools accept `--cc /path/to/gcc`; the comparison also accepts
`--crunch /path/to/crunch` and `--timeout SECONDS`. An inventory must match
the current GCC version, target, flags and search paths. Regenerate it after
header updates or search-path changes. The scan retries GCC preprocessing
for every probeable header and profile, even if the inventory recorded a
failure; Crunch is run only when GCC succeeds. Unprobeable names, reference
failures, Crunch failures, crashes, timeouts and output-limit failures remain
separate results. Output is limited to 16 MiB per process, and diagnostics
to 2,000 characters; tokenization is limited to 500,000 tokens per output.

The report compares preprocessing-token spellings after removing whitespace
and comments, including Crunch's source-map comments. It records counts,
hashes and the first differing token and surrounding tokens. For selected
representative headers it also compares macro definedness and expanded
tokens, and checks selected integer macros against fixed Linux x86-64 values.
These checks do not establish numeric equivalence for every macro.
GCC and Crunch advertise different builtins and predefined macros, so token
or macro-expansion differences require review: they are not automatically
compiler bugs or proof of semantic differences. A completed scan exits zero
even when it reports differences or per-header failures; inspect its summary
and results. The normal suite runs the representative scan and fails if
Crunch fails to preprocess a header that GCC accepted, while reporting token
differences without treating them as failures. The harness is also tested
with controlled inputs. This comparison does not compile the output.

For example, on GCC 12's x86-64 headers, `mmintrin.h` and `ammintrin.h`
initially differ because Crunch does not advertise GCC's MMX and SSE feature
macros; `x86gprintrin.h` initially differs because GCC's `stddef.h` emits
`max_align_t` when `__STDC_VERSION__` indicates C11. Further differences
include GCC's `__FXSR__` and glibc's GCC-version-dependent declarations.
In that environment, undefining `__SSE__`, `__SSE2__`, `__MMX__`,
`__MMX_WITH_SSE__`, `__FXSR__`, `__STDC_VERSION__`, and `__GNUC__` in the
GCC reference made all four targeted intrinsic headers match in all three
profiles. This is a diagnostic experiment, not a change to the reference
configuration or evidence of compilation support. Defining unsupported
features merely to match GCC is not a fix.

The normal suite also checks repeated and differently ordered GNU headers,
including partial `stddef.h` inclusion, under all three feature profiles.
Two temporary wrapper layers use `#include_next` and positive and negative
`__has_include_next` queries before reaching the real system `stdio.h` and
`sys/stat.h`. The test compares selected feature and availability branches
with GCC, checks that repeated headers do not change either tool's token
output, and verifies that normal and ASan/UBSan-instrumented Crunch
preprocessors produce the same tokens. It uses bounded subprocesses and
does not require the complete GCC and Crunch header output to match.

The normal suite builds temporary ASan/UBSan-instrumented preprocessor, parser
and IR-to-assembly tools, checks valid and invalid controls, and runs 24
deterministic mutations per stage with timeouts. Some mutations remain valid;
the test checks for crashes, hangs and sanitizer errors rather than requiring
every mutation to be rejected. Failures report the stage, seed, case and full
input as a Python string literal for reproduction. It requires a host C
compiler with AddressSanitizer and UndefinedBehaviorSanitizer support. For 128
mutations per stage, run:

```sh
CRUNCH_TEST_MUTATION_EXTENDED=1 make check
```

The same setting controls a separate sanitized native assembler and ELF linker
corpus. It mutates assembly and ELF headers, section headers, symbols and
relocations, exercising both relocatable and executable links. Failures report
the input bytes in hexadecimal. These tests use temporary files and do not run
the resulting executables.

The native-only suite skips this corpus.

An optional external-project smoke test builds a user-provided `sssnake`
checkout with Crunch in glibc mode and with the host C compiler, then compares
their noninteractive `-h` output and exit status:

```sh
CRUNCH_TEST_SSSNAKE=/path/to/sssnake make check
```

It requires GNU headers, glibc, and a working host C compiler. It uses
`-D_GNU_SOURCE=1` for the project's system interfaces, writes build outputs only
to a temporary directory, and does not run interactive gameplay. An invalid
explicit checkout fails the test. The checkout is not fetched or distributed
with Crunch. This optional test runs only in the normal suite.

Both suites install the tools to a temporary prefix and run a corpus of
programs using the installed `crunch`, `crunch-as`, and `crunch-ld`. The normal
suite also tests all three combinations containing GNU `as` or `ld`, including
mixed native/GNU toolchains. GNU links explicitly use the installed runtime,
the configured dynamic interpreter, and the installed runtime RUNPATH.

With `CRUNCH_TEST_SLOW=1`, both suites also build two native generations of the compiler, assembler,
linker, archiver and static and shared runtimes. They run the ten-program
corpus through each generation in static, dynamic and PIE modes, comparing
compiler diagnostics, exit status, output and file effects. Negative tests
compare preprocessor, frontend, backend, assembler and linker diagnostics.
The native bootstrap test places failing GNU tool substitutes on `PATH`.
These single-threaded tool builds use `-D_Thread_local=`; they do not
demonstrate general thread-local storage support.
The normal slow suite also builds two glibc-linked generations, checks their
system-header provenance and dynamic dependencies, and compares their output
and diagnostics. The glibc bootstrap preserves thread-local storage.

When GNU headers and glibc are available, the GNU interoperability test also
self-builds the core driver, compiler stages, assembler, archiver and linker
with `--glibc` and real thread-local storage, without that workaround. The
resulting driver uses the self-built assembler to compile a program linked
against glibc by the system C compiler, without setting `CRUNCH_INCLUDE_PATH`.
Tests check include discovery ordering, overrides and failure handling. The
driver also uses the self-built assembler,
archiver and linker together for static archive links and dynamic and PIE
shared-library links. Assembler output and errors are compared with the
baseline. The native-only suite skips these GNU/glibc self-builds. A pthread
harness checks TLS initialization, address-taking, and isolation across threads.

In the installed-toolchain matrix, each assembler/linker combination links
every program statically, dynamically, and as a dynamic PIE. Executables are
run normally, after linking with `-s`, and after external `strip`. The normal
suite requires `strip`; the native-only suite reports that external-strip
interoperability is skipped if it is unavailable. Loader environment overrides
are removed for these tests and the native-generation corpus.

## Scalar type baseline

The scalar pair matrix covers all 169 ordered pairs of `char`, `signed char`,
`unsigned char`, `short`, `unsigned short`, `int`, `unsigned int`, `long`, `unsigned long`,
`long long`, `unsigned long long`, `float`, and
`double`. It checks representative arithmetic, comparisons, result sizes,
conditionals, integer operators, conversion acceptance, and accepted
assignments, arguments and returns. It also checks selected integer and
floating-point boundaries. In the default suite the same defined-behavior
programs are compiled and run with the host C compiler using `-funsigned-char`;
the native-only suite uses the same expected results without invoking it.

The matrix runs with and without the compatibility option `--c11-numeric`;
both use the same rules. Tests also cover constant expressions, switch labels,
driver option propagation and separately compiled modules.
`sizeof` tests exercise unsigned arithmetic in runtime and constant
expressions, switch labels, array bounds and separately compiled modules.
`offsetof` tests cover aggregate layout, anonymous types and members, nested
array designators, constant and runtime indices, diagnostics, and allocation
failure cleanup.
Enum tests cover scoped definitions, constants, storage, diagnostics, parser
allocation failures and separately compiled modules. The default suite also
compares execution and cross-language calls with a host C compiler.
Designated-initializer tests cover nested fields and indices, inferred bounds,
positional continuation, overrides, relocations, diagnostics and allocation
failures, with host C execution in the default suite.
Static local structure tests cover persistence, zero initialization, scoped
names, static addresses and relocations, invalid initializers, and separate
modules in static, dynamic, and PIE links.

Boundary tests cover truncate-first signed conversions, unsigned
wrapping, static and runtime results, rounding, unevaluated branches and
out-of-range conversion results. The default suite also compares defined
cases with C; invalid conversions are checked only against Crunch's rules.

Scalar storage and ABI tests exercise all 13 types in globals, fixed arrays,
structure fields and field arrays, and pointer reads and writes across
separately compiled modules. They check representative
limits, adjacent storage, scalar parameters and returns, and mixed integer
and floating-point arguments overflowing both System V register classes. The
normal suite also runs both Crunch-to-C and C-to-Crunch calls and structure
pointer accesses using host C compiled with `-funsigned-char`. Separate tests
exercise small integer-only structure parameters in registers and on the
stack, including C interoperability and argument snapshots. Union tests cover
INTEGER, SSE, mixed and memory-class parameters and returns, nested unions,
register exhaustion, snapshots, and bidirectional C interoperability.
Returns of structures without unions remain unsupported.

Separate formatted-output tests check all scalar arguments, including
small-integer and `float` promotions, against expected output and, in the
normal suite, host C output.
Crunch's formatted-output helpers use a private tagged ABI; this does not
test or imply support for general C variadic calls.

Compatibility tests check AST and IR version precedence, the compatibility
option, and separate-object linking. Numeric byte arithmetic and
floating casts are executed from every supported IR version 2 through 19,
including historical module and executable formats.
Integer literal tests cover unsigned hexadecimal and octal candidate types,
octal unsigned suffixes, and the type of unary minus applied to a decimal literal.

These are representative tests, not exhaustive coverage of every operator,
value, conversion context, floating-point environment, or C ABI interaction.
Out-of-range floating-to-integer conversions and signed integer overflow are
not compared with C, where those results are not defined.

## Test units

- **build** — Autoconf/configuration smoke tests.
- **documentation** — function contracts and documentation-checker regressions.
- **lexer** — C unit tests and source fixtures for valid tokens and lexical errors.
- **allocation failures** — lexer, parser, semantic analysis, preprocessor, IR
  generation, and assembly translation paths are exercised with transient and
  persistent injected failures, checking error handling and cleanup under
  sanitizers. IR-reader tests also simulate `getline` allocation failures.
- **parser** — valid and invalid source fixtures, tested through lexer and parser.
- **AST JSON** — per-source checks for the versioned AST JSON format and selected
  structural invariants.
- **semantic / scopes** — duplicate declarations, undefined names, shadowing,
  and scope visibility.
- **semantic / expressions** — operator compatibility, assignment validation,
  member access, and array initialization.
- **semantic / function calls** — argument count/types and return-type checks.
- **semantic / globals** — global declaration conflicts and global/local naming.
- **semantic / structures** and **IR + x86-64 execution / local_global_members.bc** — direct scalar member reads and writes for local/global structure variables, including access through a function.
- **semantic / types** — numeric widening/narrowing, casts, and range checks.
- **type aliases** — scoped `typedef` declarations, shadowing, pointer
  qualification, AST serialization, shared headers, and invalid forms.
- **anonymous structures** — unnamed type identity and scope, promoted members,
  nested layout, copying, const qualification, and conflicting field names.
  Structure fixtures cover named and unnamed `typedef` forms and reject incompatible
  unnamed types and duplicate promoted members during semantic analysis.
- **power** — precedence, static and runtime evaluation, numerical edge cases,
  module linking, preprocessing, operand rejection, and invalid power IR.
- **preprocessor expressions** — macro expansion, `defined`, `#if`/`#elif`
  branch selection, integer operators, short-circuiting, and invalid expressions.
- **multiline macros** — line splicing, inactive branches, CRLF, diagnostics,
  and original source locations across included files.
- **semantic / pointers** — pointer type compatibility and address operations.
- **void pointers** — return values, variables, fields, parameters, conversions,
  const checks, invalid operations, and independent module linkage.
- **structure-pointer returns** — allocation, null and global storage, indexed
  access, type and const checking, cross-module linkage, and C ABI calls.
- **stream I/O** — open modes, item counts, seeking and positions, append, EOF and error
  indicators and reset, partial transfers, invalid arguments, and injected I/O failures.
- **character I/O** — zero and 255 bytes, EOF and error indicators, standard
  descriptors, stream closure, and cross-module standard-stream linkage.
- **line I/O** — line boundaries, partial lines and EOF, buffer limits, standard
  streams, and failures after partial reads or writes.
- **IR + x86-64 execution** — source-to-IR-to-assembly-to-executable tests; each
  fixture verifies the process exit status. Legacy dynamic-array IR fixtures
  separately cover allocation, resizing, and global descriptors.
- **legacy byte-array pointer initialization IR** — independent copies of arguments and
  const and mutable string pointers, single evaluation, embedded NULs, failed
  allocation, semantic rejection, and executable/module global and local behavior.
- **IR format** — typed instruction and function-call IR assertions.
- **IR validation** — malformed IR is rejected by the independent assembly backend.
- **separate modules** — current v3 and historical v4 modules are assembled and
  linked directly and via `ld -r`, exercising typed mixed-register calls,
  runtime-name overrides, external `noreturn` calls, and malformed module IR.
- **external globals** — scalar, pointer, and array declarations, cross-object
  reads, writes and resizing, shared runtime `errno`, invalid declarations, and
  missing definitions across direct, object, and relocatable links.
- **internal errno accessor** — assembly linkage, shared address and writes,
  initial value, and interaction with the syscall-result helper.
- **Linux memory mapping** — internal and public mmap, munmap, and mremap calls,
  failure conventions, fixed relocation, file offsets, shared/private mappings,
  shrinking, invalid arguments, and v2/v3 execution.
- **heap allocation** — allocation, release, resizing, zero and oversized requests,
  alignment and boundary sizes, independent blocks and repeated resizing, and
  injected mapping failures in executable and module builds. Pool checks cover
  size classes, fragmented multi-arena reuse and reclamation, allocation and
  remapping failures under pressure, realloc rollback on cleanup failure and
  repeated retained-mapping reuse and list removal,
  explicit trim with partial failures and retries,
  repeated resizing across class boundaries,
  recovery after a child process reaches an address-space limit, and stress
  workloads through both executable and module builds.
- **Linux file syscalls** — raw syscall results, wrapper error conversion,
  directory enumeration and malformed record decoding, directory-relative
  opens, metadata, links, renames, creation and removal, and symbolic-link inspection,
  errno preservation, file creation, transfer and seeking, and stat layout in both
  executable and separate-module builds; descriptor metadata, rename, unlink,
  directory creation/removal, and their failure paths.
- **multi-input driver** — source/object combinations, relocatable links,
  per-source preprocessing, error handling, cleanup, and large input lists.
- **assembly driver differential** — `.s` and preprocessed `.S` inputs with
  explicit GNU/native assembler and linker selections, symbol and relocation
  comparisons, executable and partial links, mixed Crunch sources, diagnostics,
  failure-safe output, and rejection of documented unsupported syntax.
- **assembler conditionals** — GNU comparisons for source-order symbol
  definitions, `.ifdef`/`.ifndef` and `.elseif`, nested and expanded blocks,
  inactive branches, malformed directives, and diagnostics.
- **assembler equations** — GNU comparisons for `.equ`/`.set` reassignment,
  captured values, forward absolute references, macros, and conditionals.
- **assembler address sizes** — GNU encoding and relocation comparisons for
  32-bit addressing registers, plus invalid operand and executable checks.
- **assembler includes** — GNU comparisons of search paths, nested inclusion,
  macros and conditionals, driver `.s`/`.S` integration, diagnostics, output
  preservation, and include recursion limits.
- **shared-library assembler interoperability** — GNU and native assembler
  objects linked as shared libraries with GNU ld, comparing PIC relocations
  and testing dynamic loading, external symbols, visibility, and interposition;
  native shared linking also consumes GNU and native shared dependencies and
  interoperates on initialized data pointer relocations and compiler-generated
  initialization and finalization arrays.
- **native dynamic executables** — interpreter and dependency metadata, eager
  function and data binding, executable exports, shared-library constructors,
  prioritized executable constructors and entry arguments, initialized Crunch
  modules, reverse-order managed finalization and exit status, explicit Crunch
  shared-runtime links, registry boundaries, opt-in executable and shared-object
  RUNPATH including relocated bundles and non-transitive lookup, opt-in RELRO
  segment layout and loader page protection, opt-in dynamic PIE relocation,
  address randomization, constructors and finalizers, and unsupported input rejection.
- **descriptor and process syscalls** — access checks, pipes, descriptor
  duplication, poll/select layouts and readiness, raw and translated errors,
  fork/getpid/kill, and child reaping with static, shared, and PIE runtimes.
- **dynamic compiler driver** — default and explicit shared-runtime links, explicit static links,
  runtime RUNPATH including ordered explicit paths and relocated bundles, direct
  and searched shared inputs, archive precedence, configured tool selection and
  safe rejection of incompatible modes. Shared external-global reads, writes,
  and addresses are exercised with GNU and native assemblers and static and
  dynamic links, including shared standard streams. Driver RELRO checks cover both option
  forms, loader protection, writable data, constructors and finalizers, and
  dynamic Crunch programs using arrays, exponentiation, and process arguments.
  Driver PIE coverage includes multiple sources, partial links, shared errno,
  RELRO, and incompatible output modes.
- **native relocatable linker** — chained partial links, GNU and native final
  links, archive extraction, symbol bindings, preserved debug sections and
  relocations, and explicit rejection of unsupported inputs.
- **native GOT relocations** — GNU assembler and PIC objects, GOTPCREL variants,
  weak and local symbols, archives, garbage collection, partial links, and
  displacement boundaries compared with GNU ld.
- **projects** — small multi-file builds with shared and nested headers,
  internal symbols, global data, mixed-ABI calls, runtime-name overrides, and
  cross-object static and external arrays, aliases with power expressions and
  external globals, bounded input/output, and an allocated file reader with
  growing buffers, indexed element addresses, and injected failure cleanup.
  Each executable is run after individual object, direct source, and
  relocatable-object builds;
  invalid projects check missing and duplicate definitions and conflicting
  prototypes.

Fixtures are under `tests/fixtures/`. Legacy fixtures remain in `parser/`,
`sema/`, and `ir/`; new focused fixtures are grouped in `units/` by purpose.
The project fixtures are under `tests/fixtures/projects/`; their Makefiles use
`CRUNCH=...` to select the compiler and are run in temporary copies.

## Allocator benchmark

After building, run `python3 benchmarks/allocator.py` to compare Crunch's
allocator with the host C allocator in separate processes. Three workloads
each request 2 MiB in differently sized blocks. The benchmark reports median
allocation, page-touching, and release times, plus virtual and resident memory
changes while all blocks are live. It requires Linux `/proc/self/statm` and
`/proc/self/smaps_rollup`. The pointer bookkeeping array is committed before
the baseline and is not included in the requested-byte total.

These are illustrative measurements, not pass/fail tests. Results depend on
the host allocator, kernel, load, and memory overcommit; memory figures include
allocator and process overhead and can be affected by unrelated activity.
allocation timing excludes touching the entire user buffer; touching and
release are timed separately from memory sampling.

## Function documentation

`make check` runs `tests/check_function_docs.py`. To check documentation alone:

```sh
python3 tests/check_function_docs.py
```

The check covers maintained C definitions in `src/` and `tests/`, function
declarations and inline definitions in `include/`, Python functions (including
nested helpers) in `tests/`, and exported assembly routines in `lib/`.
Generated files, `tests/fixtures/`, and `include/test/` header fixtures are
excluded; fixture line numbers and intentional errors are part of diagnostic
tests.

C contracts use `@brief`, one `@param` per named parameter in signature order,
and `@return` (including output/state changes for void functions). No-argument
tests may describe their return/output in `@note`. Python docstrings describe
the operation, each argument under `Args:`, and the result under `Returns:`.
Assembly comments describe register inputs and outputs with `Input:`/`Inputs:`
and `Output:` labels.

The C scanner handles the project's multiline signatures, pointer returns,
and array parameters without requiring extra packages. The check detects
missing contracts and mismatched parameter names, not whether descriptions
accurately explain the implementation; that still requires review.

## Adding a test

1. Add a fixture under the most relevant `tests/fixtures/units/<unit>/` folder.
2. Register it in `tests/run-tests.py` under the corresponding unit. State whether
   semantic analysis should accept it or reject it; for executable tests, also
   state the expected process exit status.
3. Run `make check` and verify the per-test report.

Negative tests should verify that the appropriate stage rejects the input rather
than merely checking that the whole compiler exits unsuccessfully. End-to-end
fixtures should verify behavior by executing the linked program.

## Regression tests

When a bug is found, add a fixture that reproduces it and register it in the
appropriate unit in `tests/run-tests.py`. Prefer checking behavior at the
lowest relevant layer and, when the behavior affects generated programs, add
an end-to-end source-to-executable test as well. For example, `.5` is checked
as a single `REAL_LITERAL` by the lexer, expression precedence is checked by
execution, calls are tested with distinct arguments in both registers and on
the stack, nested calls exercise temporary stack state, and pointer aliases
verify that writes through one pointer are visible through another.

Fixtures that require behavior not yet implemented by the backend should be
semantic/parser tests only; do not present them as end-to-end supported
features until the generated program can be assembled and executed.


### Fixed-array and legacy IR regressions

Source fixtures exercise fixed arrays in local and global storage, structure
fields, initialization, indexing, and whole-structure copies. Programs needing
runtime-sized storage use explicit allocation and release.
Static local arrays are exercised across repeated and recursive calls, nested
scopes, constant initialization, and separately compiled objects.
Static local scalar coverage includes zero and constant initialization, numeric
and pointer access, address relocations, shadowing, and separate modules.
Positional structure initializer coverage exercises nested structures, arrays,
strings, scalar conversions, omitted fields, pointer relocations, invalid lists,
nonconstant static values, and execution compared with the host C compiler.
Structure-array cases cover inferred bounds, mixed explicit and elided braces,
automatic structure copies, static local storage, and static relocations.

The `legacy_*.bir` fixtures were captured from the former frontend to retain
backend coverage of historical dynamic-array IR. They exercise resizing,
typed and string copying, structure fields, and initialization. They are IR
compatibility tests, not examples of supported source syntax.

The global static-string regression checks writable initial data, embedded NULs,
resizing, clearing, copying, aliases, and release of subsequent allocations. It
also rejects malformed `globalbytes` directives and checks that initial string
data requires no runtime allocation.
The cross-object regression checks a pointer into another object's static
data, multiple registry entries, and both direct and relocatable links.

The resize-edge legacy IR covers prefix preservation, zero and negative
requests, repeated release, and overflow for multiple element types.

The initializer allocation-failure fixtures use a controllable allocator to
exercise failed and successful local, global, and structure-field
initialization, including skipped element side effects and repeated
initialization in loops. The local and field cases run in both IR v2 and v3.
The single-source startup regression checks once-only global initialization
across recursive `main` calls, startup arguments, initializer-name collisions,
mixed executable and module objects, allocation failure without retry, and IR v5
directive validation.

The global integer-constant fixture checks compile-time arithmetic, widths,
short-circuiting, static groups, and `sizeof` against runtime behavior in both
executable and module modes. Rejection cases cover invalid evaluated operations,
nonconstant branches, and warnings for out-of-range casts.
The global real-initializer fixture compares integer-to-real static data with
runtime conversions, including signed limits and binary64 rounding, and checks
that real literals and powers retain their behavior.
Real constant-arithmetic tests compare static results with runtime operations,
including mixed integer widths, signed zero, overflow, underflow, and division
by zero in both compilation modes; invalid integer operations and nonconstant
expressions are rejected.
Real constant-comparison tests cover all six operators, NaNs, signed zero,
infinities, mixed-width rounding, nested expressions, and short-circuiting,
and compare folded results with runtime behavior.
Constant-conditional tests cover real and integer conditions and branches,
mixed-type conversions, signed zero, NaN, skipped invalid operations, and
rejection of nonconstant branches in both compilation modes.

The hexadecimal and bitwise fixture allocates memory with `malloc(0xffff)` and
checks bitwise operators, compound assignments, precedence, signed shifts, and
integer-width boundaries in both compilation modes. Negative cases check
malformed literals and out-of-range values at the appropriate frontend stage.

`function_calls/array_arguments.bc` checks passing numeric arrays to
functions and reading their elements there. `function_calls/argv_runtime.bc`
executes the conventional `argc`/`argv`/`envp` entry point, including nested
vector indexing, null termination, and passing an argument string to another
function.


### String comparison regressions

The character-pointer tests execute address comparisons on distinct arrays,
aliases, null pointers, non-terminated data, and side-effecting expressions in
executable and module modes. String content comparison is tested through
`strcmp`.
