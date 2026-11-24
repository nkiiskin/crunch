# Build configuration (Autoconf and Automake)

Crunch uses GNU Autoconf and Automake as its canonical configuration/build
entry point. The maintained inputs are `configure.ac`, `Makefile.am`, and
`build/config.mk.in`; `configure` and `Makefile.in` are generated and included
in source archives so users can configure and build without installing the
Autotools themselves. When changing configuration logic, edit `configure.ac`
and regenerate the scripts with `autoreconf -fi` (Autoconf and Automake must be
installed for that step).

Typical build and test:

```sh
./configure
make
make check
```

## Self-hosting bootstrap

To configure and build a seed compiler, then keep two native generations, run:

```sh
./build_2gen_crunch.sh --native
```

The script runs `./configure` with default options and `make` for the seed
build, so that initial build requires the configured host C compiler and GNU
binutils. It refuses to overwrite existing `bootstrap-stage1` or
`bootstrap-stage2` paths and prints the second-generation tool paths.

Each generation contains the Crunch driver, preprocessor, IR generator,
assembly backend, assembler, linker and archiver. It also builds its own
static and shared runtimes with its newly built native tools, rather than
copying the seed runtime. The first generation uses the seed compiler and its
native tools; the second uses the first generation. The generation builds
still require GNU make, Python 3, a POSIX shell, the source tree and its
generated build configuration.

For example, compile with the second generation using its configuration and
runtime:

```sh
CRUNCH_CONF_FILE="$PWD/bootstrap-stage2/bootstrap.conf" \
  "$PWD/bootstrap-stage2/crunch" --dynamic -I "$PWD/include" program.bc -o program
```

To build native generations individually after a normal build, use new output
directories:

```sh
sh tests/bootstrap-compiler.sh --native "$PWD/bootstrap-stage1"
CRUNCH_BOOTSTRAP_COMPILER="$PWD/bootstrap-stage1/crunch" \
CRUNCH_BOOTSTRAP_RUNTIME="$PWD/bootstrap-stage1/libcrunch.a" \
  sh tests/bootstrap-compiler.sh --native "$PWD/bootstrap-stage2"
```

Without `--native`, `build_2gen_crunch.sh` retains the GNU-tool bootstrap.
It builds the core compiler stages, uses GNU `as` and `ld`, and copies the
previously built static runtime. The individual GNU-tool workflow is:

```sh
sh tests/bootstrap-compiler.sh "$PWD/bootstrap-stage1"
CRUNCH_CONF_FILE="$PWD/bootstrap-stage1/bootstrap.conf" \
  "$PWD/bootstrap-stage1/crunch" --static -I "$PWD/include" program.bc -o program
CRUNCH_BOOTSTRAP_COMPILER="$PWD/bootstrap-stage1/crunch" \
CRUNCH_BOOTSTRAP_RUNTIME="$PWD/bootstrap-stage1/libcrunch.a" \
  sh tests/bootstrap-compiler.sh "$PWD/bootstrap-stage2"
```

To build two generations of all the tools against GNU headers and glibc:

```sh
./build_2gen_crunch.sh --glibc
CRUNCH_CONF_FILE="$PWD/bootstrap-stage2/bootstrap.conf" \
  "$PWD/bootstrap-stage2/crunch" --glibc program.bc -o program
```

Alternatively, after building the seed compiler:

```sh
sh tests/bootstrap-compiler.sh --glibc "$PWD/bootstrap-stage1"
CRUNCH_BOOTSTRAP_COMPILER="$PWD/bootstrap-stage1/crunch" \
  sh tests/bootstrap-compiler.sh --glibc "$PWD/bootstrap-stage2"
```

This mode uses each previous generation's assembler and linker for object
production and the system C compiler for glibc executable linking. It copies
only project headers and the generated linker configuration into each stage;
it does not use Crunch's libc headers or build its libc. Each stage copies the
previous compiler's `libcrunch-support.a` for compiler helpers. The build
ignores `CRUNCH_INCLUDE_PATH`, `CPATH`, and `C_INCLUDE_PATH`, preserves real
thread-local storage, and defaults `SOURCE_DATE_EPOCH` to `0` unless set.
`CRUNCH_GLIBC_CC` may select the system C compiler. `CRUNCH_BOOTSTRAP_RUNTIME`
is not used in this mode. These stage directories are intended for `--glibc`
and do not contain the normal Crunch runtime.

The native and GNU-tool workflows link the compiler tools statically and leave
object files in their output directories. Linux `getrandom` and `memfd_create` must be
available and permitted. These single-threaded tools are built with
`-D_Thread_local=` because the Crunch runtime does not initialize TLS. The
bootstrap preprocessor's default include path points to this source tree, and
the native and GNU-tool bootstrap build date and time strings are placeholders.

The normal `make check` also tests the GNU-tool core bootstrap against the
host-built tools. Both normal and native-only suites test two native
generations and compare their behavior and diagnostics; see
[`testing.md`](testing.md).

## Experimental glibc linking

`crunch --glibc` uses module compilation, suppresses the Crunch `_start`, and
links using the system C compiler's glibc startup and libraries. It does not
link Crunch's libc; the small `libcrunch-support.a` archive supplies compiler
helpers such as real exponentiation. The normal Crunch libc modes remain the
default. The system linker driver is `/usr/bin/cc`, or the absolute path in
`CRUNCH_GLIBC_CC`. This mode requires a host glibc C toolchain even in a
native-only Crunch build.

The option omits Crunch's standard header directory, adds its glibc
compatibility-header directory, and discovers system include directories
using the selected system compiler's `-E -x c - -v`
search list. Discovery runs with `LC_ALL=C` when preprocessing is needed; a
failed or unparseable query is an error. Explicit `-I`, `CRUNCH_INCLUDE_PATH`,
and configuration-file `include_path` directories are searched first. The
driver defines `__STDC_HOSTED__` as `1` in this mode so GCC wrapper headers
select their hosted paths. An explicit `-D__STDC_HOSTED__=...` takes precedence;
this mode indicator does not imply full C standard or GNU extension support.
The standalone preprocessor and non-glibc driver mode are unchanged. The
selected system compiler must therefore be available for `--glibc`
preprocessing as well as linking. Use `--glibc` on both compilation and linking when producing
objects separately; ordinary Crunch objects containing `_start` conflict with
the system startup. `--glibc` currently supports non-PIE dynamic executables
and cannot be combined with `--dynamic`, `--static`, `--pie`, `-z`,
`--rpath`, or `--archive`. A matching C toolchain and compatible GNU headers
are required; not all GNU extensions are supported. File-scope static scalar
`_Thread_local` objects are supported in this mode. The native Crunch
executable runtime does not support TLS. The native linker rejects TLS
relocations, but GNU `ld` can accept them without initializing TLS; do not
link these objects using Crunch startup even if a GNU link succeeds.

## Configure options

Run `./configure` before `make`; the paths below are fixed at this step and
compiled into `crunch-pp`. Re-run `./configure` and `make` after changing them
(for example after moving or renaming the source directory).

| Option | Default | Meaning |
|--------|---------|---------|
| `--enable-native-only` | disabled | Use native assembler, linker and archiver without requiring GNU binutils |
| `--with-crunch-includedir=DIR` | `<source dir>/include` | Built-in header directory |
| `--with-crunch-conffile=FILE` | `/etc/crunch/crunch.conf` | Configuration file (absolute path) |
| `--with-default-assembler=gnu\|crunch` | detected GNU `as`, otherwise `crunch` | Driver's default assembler |
| `--with-gnu-as=FILE` | first `as` in `PATH` | GNU assembler the driver runs (absolute path) |
| `--with-crunch-ld=FILE` | first `ld` in `PATH` | GNU linker the driver runs (absolute path) |
| `--with-dynamic-linker=FILE` | detected system interpreter | Default ELF interpreter embedded by crunch-ld |
| `--with-default-linker=gnu\|crunch` | detected GNU `ld`, otherwise `crunch` | Driver's default linker for static and relocatable links |
| `--with-runtime-assembler=gnu\|crunch` | `crunch` | Assembler used for the built and installed runtime archive |
| `--with-crunch-ar=FILE` | first `ar` in `PATH` | GNU archiver used when selected (absolute path) |
| `--with-default-archiver=gnu\|crunch` | detected GNU `ar`, otherwise `crunch` | Driver's archiver and the runtime archive builder |
| `--with-crunch-libdir=DIR` | `${libdir}/crunch` | where the static and shared Crunch runtime libraries are installed |

Normally, `configure` searches `PATH` for GNU `as`, `ld`, and `ar` and selects
each available tool independently. If one is missing or is not GNU, it selects
the corresponding Crunch tool. It prints the detected paths and selected
defaults; a `crunch.conf` tool setting can override the driver defaults.
Explicit GNU tool paths must be absolute executables identifying themselves
as GNU tools, and an explicit GNU selection fails if that tool is unavailable.
The paths are written to `build/config.mk` as `AS`, `LD`, and `CRUNCH_AR`.
The former `--with-crunch-as=FILE` remains a deprecated alias for
`--with-gnu-as=FILE`. `--with-default-assembler=crunch` makes the driver use
Crunch's assembler by default; `crunch.conf` and the driver option can
override this choice. `--enable-native-only` defaults both the assembler and
linker to `crunch` and rejects GNU selections.
Configure detects the x86-64 ELF interpreter in `/lib64`, `/lib`, or
`/lib/x86_64-linux-gnu` and writes it to `build/crunch-linker-config.h`.
Use `--with-dynamic-linker=FILE` to select an absolute target path explicitly,
including for cross builds; `crunch-ld --dynamic-linker=FILE` overrides it
for an individual link. The configured path is not checked for existence
when explicitly supplied.
The selected archiver builds the runtime archives and is used by the driver's
`--archive` mode. `archiver_path` in `crunch.conf` overrides the driver's
selection. Some interoperability tests require GNU binutils.
The normal `make check` suite requires GNU binutils even if configure fell
back for a missing tool; use `--enable-native-only` for its native-only suite.
The runtime is assembled with `crunch-as` by default; configure with
`--with-runtime-assembler=gnu` to use GNU `as` instead. The selected archiver
still builds the archive. The resulting
`libcrunch.a` is used for static links and installed normally. This
does not select the driver's assembler or linker.

The build also produces `build/out/libcrunch.so.1` with SONAME
`libcrunch.so.1` and the `libcrunch.so` linker-name symlink. The selected
runtime assembler builds its objects; `crunch-ld` links the shared library
with undefined strong symbols rejected. The driver links executables against
the shared runtime by default; `crunch --static` uses the static archive.
Dynamic links also use `libcrunch-fallback.a` to resolve runtime names after
user objects and libraries. For direct linker invocations, place that archive
after the user inputs and before `libcrunch.so`; the driver does this
automatically. It is only needed at link time.

The shared runtime exports the Crunch-prefixed runtime entry points and
standard stream pointers. It is not a drop-in replacement for libc: in
particular, the formatting functions use Crunch's tagged-slot calling
convention, not C varargs. Its process-wide `errno` is exported with protected
visibility, so runtime operations use its own storage even when loaded into
a libc-based process. Use `__crunch___errno_location` to obtain its address
from an external program. It is not thread-local or synchronized; do not
assume it is interchangeable with libc's thread-local `errno`.

An external program can load the library, for example:

```sh
python3 -c 'import ctypes; l=ctypes.CDLL("./build/out/libcrunch.so"); f=l.__crunch_strlen; f.argtypes=[ctypes.c_char_p]; f.restype=ctypes.c_long; print(f(b"Crunch"))'
```

The installed directory is not necessarily in the system loader's search
path. For driver-produced dynamic executables, the driver embeds its absolute
runtime directory as `DT_RUNPATH`. Standalone `crunch-ld` also embeds the
selected directory when it automatically adds the shared runtime; explicit
runtime inputs require an explicit RUNPATH or separate loader configuration. The
shared `__crunch_run_initializers` helper resolves the weak Crunch registry
boundary symbols from the loader scope when the library loads; both must be
exported and already in scope to use a custom registry. With neither boundary
it does nothing; with only one it reports an error and sets Crunch `errno` to
`EINVAL`. Normal C runtime `.init_array` initialization is independent of
this helper. The shared `exit()` helper also resolves a weak
`__crunch_run_finalizers` hook when the library loads; if none is present, it
exits without managed executable finalization.

For a Crunch compilation pipeline that does not require GNU binutils:

```sh
./configure --enable-native-only
make
make check
```

This mode makes `crunch-as` and `crunch-ld` the driver defaults, builds the
runtime with `crunch-as` and `crunch-ar`, and runs the native-only test suite.
It does not search for GNU `as`, `ld`, or `ar`. GNU-specific tool paths and
GNU runtime/linker/archiver selections cannot be combined with this option.
The installed tools remain experimental; unsupported inputs fail rather than
falling back to GNU tools. A configuration file can still select GNU tools
when their absolute paths are explicitly supplied.

The host C compiler, GNU make, Python and shell utilities are still needed to
build Crunch; the host compiler may itself require a system assembler and
linker. This option does not claim a binutils-free host bootstrap.

```sh
./configure --with-crunch-includedir=/usr/local/include/crunch \
            --with-crunch-conffile=/usr/local/etc/crunch/crunch.conf
make
make check
```

## Header search path

`#include "file"` (user headers) first looks relative to the directory of the
file containing the directive (so `#include "inc/some_header.h"` works), and
then falls back to the search order below. `#include <name>` skips the first
step. Absolute names and `..` components are rejected.

Headers are looked up in this order; the first match
wins:

0. Each directory given with `-I DIR` to `crunch-pp` (or `crunch`), in order.

Macros can also be defined on the command line with `-D NAME` (equivalent to
`#define NAME 1`) or `-D NAME=VALUE` (like `#define NAME VALUE`), for both
`crunch` and `crunch-pp`. `-D NAME=` defines an empty replacement.
1. Each directory in the colon-separated `CRUNCH_INCLUDE_PATH` environment
   variable (same format as `PATH`; use `$HOME`, not `~`, since the shell does
   not expand a tilde inside a quoted or exported value).
2. The built-in directory chosen with `--with-crunch-includedir`.
3. Each directory from `include_path` in the configuration file.
4. In `--glibc` mode, Crunch's glibc compatibility headers, followed by
   system directories discovered in compiler search order.

`crunch-pp --no-crunch-includes` omits step 2. `crunch --glibc` instead
selects `crunch-pp --glibc-includes`, which omits step 2 and adds the
compatibility headers before the system directories. Standalone `crunch-pp` does not discover system
directories; `--system-include DIR` appends one after the configured paths.

The configuration file contains `key = value` lines. Whitespace around `=` is
allowed, repeated `include_path` and `library_path` lines are appended in order,
and `#` starts a
comment line. The `assembler` and `linker` keys accept `gnu` or `crunch`; the
last occurrence wins and overrides the configured default. The assembler
defaults to GNU as when detected and `crunch-as` otherwise;
`--with-crunch-as` overrides the file setting.
This driver option takes no argument; configure's `--with-gnu-as=FILE`
instead sets the GNU assembler path.
The optional `assembler_path`, `linker_path`, and `archiver_path` keys override
the corresponding executable paths. They must be absolute, are used literally
(including spaces), and the last occurrence wins. The selected `assembler` and
`linker` still determine the expected tool interface: set `linker = crunch`
when using a Crunch linker or a wrapper for it. `--with-crunch-as`
overrides both `assembler` and `assembler_path`. The archiver defaults to the
configure-selected GNU `ar` or `crunch-ar` beside the driver and is used by
`crunch --archive`; it must accept the `ar rcs` interface.

```
# /etc/crunch/crunch.conf
include_path = /usr/local/include/crunch:/opt/crunch/include
library_path = /usr/local/lib:/opt/crunch/lib
assembler = crunch
linker = crunch
assembler_path = /opt/crunch/bin/crunch-as
linker_path = /opt/crunch/bin/crunch-ld
archiver_path = /usr/bin/ar
```

A missing file is ignored. A line without `=`, an unknown key, or an empty
directory entry, invalid tool choice, or non-absolute tool path is an error
reported as `file:line:`. The
experimental native linker is not a full replacement for GNU ld. Its
relocatable links (`-c` and assembly output to `.o`) preserve ELF relocations
and section groups for a later link, but reject unsupported section types or
options; see `src/linker/README.md` for COMDAT limitations. Static executable
links support the GOTPCREL relocation family but not other GOT or TLS relocations.
The standalone `crunch-ld -dynamic` supports a restricted fixed-address dynamic
executable mode. The driver selects that mode and `libcrunch.so` by default;
`crunch --dynamic` selects it explicitly, and `crunch --static` selects static
linking. Both options
require an executable link and cannot be combined. Dynamic mode invokes
`crunch-ld` beside the driver, or the configured `linker_path` when
`linker = crunch`; a GNU linker selection is overridden for this mode, without
a GNU fallback. The configured assembler remains in use.

`-Wabi-mismatch` enables best-effort cross-object signature warnings at
link time. It compares available Crunch IR ABI records in selected objects
and extracted archive members, and visible candidates in directly linked
shared libraries. Missing metadata, indirect dependencies, and later-loaded
libraries are not checked. Differences are warnings, not link errors, and
may include unused declarations or IR types with the same calling convention.
Runtime symbol interposition is not fully modeled. Absence of warnings does
not guarantee ABI compatibility. Dynamic executable
links already use `crunch-ld`; static and relocatable links need
`linker = crunch` in the configuration (or `--enable-native-only` when
configuring). When GNU ld is selected, the driver warns and discards ABI
checking, but continues the link. The option does not check intermediate
stages or archive creation. `-Wunused-variable` enables unused local-variable
warnings, and `-Wcase-fall-through` enables switch case fall-through warnings;
both are disabled by default. `-Wall` enables all optional warnings.

The driver embeds the absolute directory containing its shared runtime as
RUNPATH. Build-tree programs therefore depend on that build directory;
installed programs depend on the configured Crunch library directory.
Keep `libcrunch.so.1` available there, or configure the loader separately.
Runtime directories containing `:` or `$` cannot be safely embedded and are
rejected.

For dynamic links, use `--rpath PATH` or `-rpath PATH` to add explicit
runtime search paths; `--rpath=PATH` and `-rpath=PATH` also work. Repeated
options are embedded in command-line order, **before** the driver's runtime
directory, which remains a fallback. For a relocatable bundle, for example:

```sh
crunch --dynamic --rpath '$ORIGIN/../lib' program.bc -L lib -lexample -o bin/program
```

Quote `$ORIGIN` so the shell leaves it for the loader, and bundle both
`libexample.so` (or its SONAME) and `libcrunch.so.1` in `lib`. The explicit
path can select a different shared runtime ahead of the built-in fallback, so
ensure it contains the intended compatible libraries. The fallback still
points to the original build or installation if the bundle is incomplete.
RUNPATH applies only to direct dependencies on glibc; dependencies of shared
libraries need their own RUNPATH. Empty path arguments are rejected, but
empty colon-separated components can search the current working directory
and should be avoided. RUNPATH options are rejected outside dynamic executable
links and do not affect link-time library search.

For dynamic executable links, `-z relro` or `-zrelro` opts into GNU RELRO;
it is not enabled by default and is rejected for static, archive, and
intermediate or relocatable output. The loader makes `.dynamic`, the GOT,
initialization and finalization arrays, and supported `.data.rel.ro` sections
read-only after relocation. Ordinary data and BSS remain writable; dynamic
bindings are already eager even without RELRO.

Direct `.so` and versioned `.so.N` inputs are accepted in dynamic
mode. Other shared dependencies need their own loader search paths. Dynamic
executables support initialization and managed finalization arrays. Use
`-pie` or `--pie` for an opt-in position-independent dynamic executable;
the default remains fixed-address. PIE requires position-independent inputs:
dynamic text relocations, narrow absolute image addresses, and direct
references to shared data are rejected. Static, archive, and intermediate
output cannot use the driver option. Shared-library finalization remains
unsupported. There is no
GNU ld fallback. Crunch external globals can be accessed through shared
libraries, including the standard streams and `errno`. A dynamic executable
cannot define its own global `errno` or directly link another shared library
that defines it, since that could diverge from the protected runtime storage.
Transitive dependencies are not checked at link time. Raw assembly references and older objects that use
direct PC-relative references to shared data need GOT relocations or `--static`.
For testing, the
`CRUNCH_CONF_FILE` environment variable overrides the configured file name.
Both linkers process archives in input order: put each archive after objects
and archives that reference it, or repeat it when needed. Unresolved symbols
are retained by relocatable links for later resolution.

## Library search path

By default, `crunch -lNAME` searches each directory for
`libNAME.so` before `libNAME.a`; `-l:FILE.so` and `-l:FILE.so.N` also select
exact shared-library filenames, and `-l:FILE.a` selects an exact archive.
Library inputs retain their position among
objects and archives. Static and relocatable (`-c`) links remain static-only.
All `-L DIR` directories are searched first, in command-line order, regardless
of where `-L` appears. Then the
colon-separated `library_path` entries are searched in configuration order.
The installed configuration defaults to `PREFIX/lib` (using the installation
prefix, without `DESTDIR`). If `library_path` is absent, the same path is
compiled into the driver. A configured `library_path` replaces that default.
Paths are literal, can be relative, and cannot contain empty entries. This
search does not change where the driver finds its own runtime. Standalone
`crunch-ld` also uses these directories for library searches and automatically
appends `libcrunch.a` for static executables or `libcrunch.so` for dynamic
executables and shared objects. It searches each `-L` and configured directory
and its `crunch` subdirectory, then its configured installation runtime
directory. If no runtime is readable it continues without one. Explicit runtime
inputs suppress automatic selection; `-nostdlib` and `-nodefaultlibs` disable
it, and relocatable links never add the runtime. Dynamic executables also add
the adjacent `libcrunch-fallback.a` when available. For automatic shared
runtime selection, its resolved absolute directory is appended to RUNPATH;
directories containing `:` or `$` are rejected. The remaining search is
link-time only: configure the loader separately for additional shared
dependencies, for example with `LD_LIBRARY_PATH`. A searched shared object
without a SONAME is recorded by filename rather than by its search path.

## Installation

`make install` installs, under `--prefix` (default `/usr/local`; `DESTDIR` is
supported for staged installs):

| What | Where |
|------|-------|
| `crunch`, `crunch-pp`, `crunch-lexer`, `crunch-parser`, `crunch-ir`, `crunch-asm`, `crunch-as`, `crunch-ld`, `crunch-ar` | `$(bindir)` |
| Standard and Crunch runtime headers, including `sys/` headers | `$(includedir)/crunch` |
| `libcrunch.a`, `libcrunch-fallback.a`, `libcrunch-support.a`, `libcrunch.so.1`, `libcrunch.so` | `$(libdir)/crunch` (configurable with `--with-crunch-libdir`) |
| `crunch.1` man page | `$(mandir)/man1` |
| `crunch-ar.1`, `crunch-ld.1` man pages | `$(mandir)/man1` |
| `*.3crunch` library man pages | `$(mandir)/man3` |
| `crunch.conf` (default includes `library_path`) | `$(sysconfdir)/crunch/crunch.conf` |

The installed `crunch` and `crunch-pp` are rebuilt with the installation paths compiled in, so
it finds the installed headers and configuration file rather than the source
tree. `--with-crunch-includedir` and `--with-crunch-conffile`, if given, set the
installation locations as well as the build-tree defaults. Temporary
installation binaries are staged outside the build tree and removed
after installation. Run `make` as your normal user before a privileged
`make install` to avoid root-owned build artifacts. An existing
configuration file is never overwritten. `make uninstall` removes it (and its
directory, if empty) only when it is identical to the default template; a
modified file is kept. The `crunch` driver finds the other tools in its own directory, so keep
them together. For example:

```sh
./configure --prefix=$HOME/crunch
make
make install
$HOME/crunch/bin/crunch hello.bc -o hello
```

## Manual page

`make doc` generates `build/out/crunch.1` from `man/crunch.1.in`, filling in
the version, the `as`/`ld` paths, and the installed header directory and
configuration file chosen by `configure` (so the FILES section matches
`--prefix` and `--with-crunch-conffile`). `make install` installs it under
`$(mandir)/man1`. View it with `man -l build/out/crunch.1`.
`make doc` also prepares `input`, `open`, `read`, `write`, `close`, `stat`,
`fstat`, `lstat`, `readlink`, `openat`, `fstatat`, `readlinkat`, `unlinkat`,
`mkdirat`, `renameat`, `symlinkat`, `getdents64`, `dirent64_next`,
`unlink`, `rename`, `mkdir`, `rmdir`, `lseek`, `malloc`, `free`,
`realloc`, and `allocator_trim` library pages in
subsection `3crunch`, installed in
`$(mandir)/man3`. For example, use `man -s 3 -e crunch open` after installation
or `man -l build/out/open.3crunch` from the build tree.

## Build details

The root Makefile delegates the established compile rules to `build/Makefile`.
Configuration creates `build/config.mk` with the detected compiler, Python,
shell and, in normal mode, GNU assembler/linker tool paths, plus the default `crunch-pp` include
directory. By default, headers are searched in the source tree's `include/`
directory. Override it with `./configure --with-crunch-includedir=DIR`.
`crunch-pp` searches, in order: the colon-separated directories in
`CRUNCH_INCLUDE_PATH`, the configured include directory, and the directories
listed by `include_path` in the configuration file (default
`/etc/crunch/crunch.conf`, changed with `./configure --with-crunch-conffile=FILE`;
the `CRUNCH_CONF_FILE` environment variable overrides it at run time). The file
holds `include_path = dir[:dir...]` lines (whitespace around `=` is allowed;
repeated lines are appended), `#` comment lines, and blank lines. A missing file
is ignored; unknown keys, lines without `=`, and empty directories are errors.
For example:

```sh
CRUNCH_INCLUDE_PATH="$HOME/crunch-headers:/opt/crunch/include" \
  build/out/crunch-pp program.bc | build/out/crunch-lexer -
```

An empty path component searches the current directory. `crunch-pp` expands
`#include <filename>` and `#include "filename"` directives and writes the resulting source to standard
output. Backslash-newline pairs are spliced before directive parsing and macro expansion.
`#if`, `#elif`, `#ifdef` and `#ifndef` support nested `#else`/`#endif`; both `#define` and
`#macro` names count as defined. `#undef NAME` removes either kind of name;
undefining a name that is not currently defined is harmless. Replacement
macros use `#macro` or `#define`: object-like macros such as
`#define COLOR_RED 34` and function-like macros with zero or more
parameters are supported. Function-like invocations may nest and span ordinary
newlines, with whitespace or comments between their name and `(`; parameters are
substituted as identifiers outside strings and comments. A valueless
`#define NAME` expands to an empty replacement, while remaining defined for
include guards and `defined`.
Equivalent macro redefinitions are accepted; incompatible redefinitions are
an error unless the name is first undefined.

`#include` and GNU `#include_next` accept literal or macro-expanded quoted and
angle-bracket header names. `#include_next` resumes the include search after
the directory that supplied the current header; duplicate directories are
searched once. Relative header names may contain `.` and `..` components,
which the filesystem resolves against each searched directory, including
symbolic links. This permits reading outside an include directory; include
search paths are not a security sandbox. Absolute names and empty path
components remain unsupported. Active `#pragma once` directives prevent repeated inclusion of
the same file, including through symbolic or hard links.
Preprocessor conditions support `__has_include(...)` and `__has_include_next(...)`
using the same search order and header-name safety restrictions, without
including the checked file. Both operators can be tested with `defined`.
`__has_builtin(identifier)` and `__has_attribute(identifier)` are also
predefined. The former recognizes `__builtin_alloca` and `__builtin_offsetof`;
other builtin and
attribute queries expand to `0`. Their operands may be macro-expanded, and
these names can be overridden or undefined like other predefined macros.
Variadic macros support `...` / `__VA_ARGS__` and GNU named arguments, including
GNU `, ##` elision for omitted arguments and empty token-paste operands.
Macro expansion preserves token boundaries unless tokens are explicitly pasted;
synthetic boundaries do not introduce whitespace when an expanded argument is
later stringified.
Chains of object-like macros whose replacements are single identifiers can
invoke a function-like macro using following source tokens, including across
ordinary newlines. More complex replacements and function-macro results are
not generally rescanned together with following source tokens.
GNU libc headers are not yet generally supported: some GCC predefined macros
are not implemented. Successful
preprocessing alone does not establish that the parser accepts GCC-specific declarations.
Preprocessor directives within a multiline macro invocation are not supported.
Diagnostics for tokens produced by a multiline invocation use the invocation's
starting line; tokens following it retain their own source locations.

Crunch predefines `__CRUNCH__` as `1`, `__CRUNCH_MAJOR__`,
`__CRUNCH_MINOR__`, and `__CRUNCH_PATCHLEVEL__` as the numeric release
version components, and `__linux__`, `__x86_64__`, `__unix__`, and `__unix`
as `1` for the target. These are available in Crunch and preprocessed assembly inputs,
including standalone `crunch-pp`. A command-line `-D` takes precedence; source
can also `#undef` a predefined name. Crunch does not define `__GNUC__` or
`__STDC_VERSION__`. It defines `__NO_INLINE__` as `1` because it does not
currently inline function calls.

Crunch also predefines `_Static_assert` as a self-referential object-like macro.
It remains the compiler's `_Static_assert` keyword when expanded, while
`defined(_Static_assert)` lets headers detect that the keyword is supported.
It may be overridden with `-D` or removed with `#undef`; it does not advertise
general C11 conformance.

For the Linux x86-64 target, `__WCHAR_TYPE__`, `__WCHAR_WIDTH__`,
`__WCHAR_MAX__` and `__WCHAR_MIN__` describe a signed 32-bit `int` wide
character type. Integer type, limit, width and constant-construction macros
used by GCC's freestanding `stdint.h` are also predefined, along with GCC's
fundamental integer limits, integer widths, and `__SIZEOF_*__` macros for supported scalar and
standard typedef types.
`__CHAR_BIT__` is 8 and `__CHAR_UNSIGNED__` is defined because Crunch's plain
`char` is unsigned. These macros follow the same override and undefinition rules.
`__LP64__` and `_LP64` are defined as `1` for the target's 64-bit long and
pointer ABI; `__ILP32__` is not predefined.
GCC-compatible `__BYTE_ORDER__` and `__FLOAT_WORD_ORDER__` identify little-endian
x86-64, with `__ORDER_LITTLE_ENDIAN__`, `__ORDER_BIG_ENDIAN__`, and
`__ORDER_PDP_ENDIAN__` available for comparisons.
Selected GCC-compatible `__FLT_*__`, `__DBL_*__`, and `__LDBL_*__` format properties and
limits describe Crunch's binary32, binary64, and x86 extended-precision types.

`__FILE__` expands to the current source path as a string literal and
`__LINE__` to its one-based logical line number. They are defined for
`#ifdef` and expand at the macro invocation site, including within headers and
`#if` expressions. Removed backslash-newline pairs still count toward
`__LINE__`, including in direct macro arguments. Following GCC, an argument to
a call through a single-identifier alias uses the alias invocation's line.
Control bytes in `__FILE__` are emitted as fixed-width octal escapes so that
following digits cannot change their value. Standard input uses `"-"` as its file name. The existing
`#undef` and command-line override rules apply.

`__DATE__` and `__TIME__` expand to translation-date and time string literals
in UTC, fixed at their first use during a preprocessing invocation. When
`SOURCE_DATE_EPOCH` is set, its nonnegative decimal Unix timestamp is used
instead; values above 253402300799 or malformed values are rejected when a
timestamp macro is expanded. They can be overridden with `-D` or undefined
like other predefined macros.

An active `#error` reports its following text and fails preprocessing; GNU
`#warning` reports its unexpanded text on stderr and continues. Inactive
directives are ignored. `#line number` sets the logical line of the next
source line, and `#line number "file"` also changes its logical file name.
The operands are macro-expanded; the line number must be decimal and between
1 and 2147483647. Logical locations affect `__FILE__`, `__LINE__`, compiler
diagnostics, and assembly line markers, but not relative include lookup. An
included file has its own logical location; the including file's location is
restored afterward.

`#if` and `#elif` expand replacement macros, recognize `defined NAME` and
`defined(NAME)`, and treat remaining identifiers as zero. Use `defined` to
test an empty macro. Expressions support decimal, octal and
hexadecimal integer constants (including `U`, `L` and `LL` suffixes),
single-byte character constants and escapes, parentheses, unary `+`, `-`, `!`
and `~`, arithmetic, shifts, comparisons, bitwise and logical operators, and
the `?:` operator with C precedence. Signed and unsigned operands use 64-bit
preprocessor arithmetic; unsigned arithmetic wraps, and signed right shifts
are arithmetic. Invalid syntax, integer overflow, division by zero, and invalid
shifts are errors in evaluated expressions. `&&`, `||`, and `?:` short-circuit;
expressions in inactive outer branches or after a selected branch are not
evaluated. The `defined` operator must appear directly in the directive, not be
produced by macro expansion. Floating-point, string and multicharacter
constants are not supported. Backslash-newline splicing applies before
directive parsing, including in inactive branches. A backslash followed
by spaces does not continue the line. Macros support `##` concatenation,
using unexpanded arguments adjacent to `##` and rescanning the result; full
preprocessing-token validation is not supported. Function-like macros support `#` stringification
of unexpanded arguments, including whitespace normalization and quoted-token
escaping. Include expansion emits private comment markers that let compiler
diagnostics identify the original file and line. For preprocessed assembly,
`crunch-pp --assembly` instead emits GNU assembler line markers and defines
`__ASSEMBLER__` as `1` unless supplied with `-D`. The `crunch` driver selects
this mode for `.S` files and passes through `-I` and `-D`. Recognized
preprocessor directives at the start of a line take precedence over assembly
comments; other `#` text is preserved as an assembly comment, without macro
expansion. In addition to `#pragma once`, Crunch accepts GCC diagnostic
`push`, `pop`, and `ignored` for `"-Wpedantic"` or `"-pedantic"` as no-ops:
Crunch has no corresponding pedantic warning category. `#pragma pack()`,
`pack(n)`, `pack(push)`, `pack(push,n)`, and `pack(pop)` are supported for
decimal n of 0, 1, 2, 4, 8, or 16; 0 restores default packing. The push
stack and current value persist across includes. Pop on an empty stack and
other packing forms are errors. Operands are macro-expanded. Preprocessed C
contains canonical `#pragma pack(n)` state changes consumable by GCC and the
Crunch lexer; assembly output omits these directives. In preprocessed C,
GCC target and optimization controls (`GCC target`, `push_options`,
`pop_options`, `reset_options`, and `optimize`) are preserved without applying
them or expanding their operands. Crunch compilation and preprocessed assembly
reject them. Other unsupported pragmas are rejected in active sections.
`_Pragma` is not implemented: it remains in preprocessor output and is not
applied as a directive.
Some intrinsic headers use these controls to enable instructions such as MMX,
SSE, POPCNT, or RDRAND, or to restrict register use. Crunch's scalar SSE
code generation and native assembler instruction support do not imply support
for GCC vector types, target-specific builtins, or per-function target options.
This is Crunch preprocessing, not full GCC preprocessor compatibility.
Macro names, macro
parameters, guard names, and Crunch source identifiers share the ASCII C
identifier form `[A-Za-z_][A-Za-z0-9_]*`; universal-character-name identifiers
are not supported. The supported target is currently Linux on x86-64. The C compiler must
accept strict C11 flags, GNU make is required, and Python 3 is required for
integration tests. GNU binutils are required for normal configuration and
interoperability tests, but not for native-only configuration and tests.
LLVM is not detected or required.

The experimental `crunch-as` object assembler is installed beside the other
stage tools. Pass `--with-crunch-as` to the driver to select it in
place of GNU `as`; it does not yet support all generated assembly. GNU `as`
remains the default assembler in the normal configuration.
