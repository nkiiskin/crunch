# Crunch IR

Crunch defines its own textual, stack-based intermediate representation. It is independent of LLVM and of the x86-64 instruction set. Each function begins with `func NAME PARAMETER_COUNT RETURN_TYPE` and ends with `end`. Version 2 adds explicit numeric types and conversions; it is intentionally incompatible with the initial integer-only v1 format. The backend accepts versions 2 through 37, 39, 41 through 51, and 53 through 61.

Functions may use local slots numbered 0 through 1023. Parameters and
compiler-generated temporaries also consume slots.

The frontend selects the first applicable executable/module version pair:

| Requirement | Executable | Module |
|---|---:|---:|
| Thread-local scalar storage | — | 53 |
| Automatic variable-length array, pointer or parameter | 50 | 51 |
| Alignment specifier | 48 | 49 |
| Flexible or zero-length array member | 46 | 47 |
| Dynamic format or literal fixed binary80 precision | 42 | 43 |
| Variadic function syntax | — | 41 |
| Overridable formatted output or `vfork` call, or runtime fallback guard | — | 39 |
| Pointer subtraction or literal `%.*s` format | 36 | 37 |
| `long double` syntax or type | 34 | 35 |
| Aggregate argument-list retrieval | 32 | 33 |
| Argument-list operations | 30 | 31 |
| Floating formatted argument | 28 | 29 |
| Integer formatted argument | 26 | 27 |
| Function pointer, function address, or variadic signature | 24 | 25 |
| Aggregate return or memory-class aggregate parameter | 22 | 23 |
| SSE-class aggregate parameter | 20 | 21 |
| Integer-class structure parameter | 18 | 19 |
| Explicit signed/unsigned character type | 16 | 17 |
| `short` or `unsigned short` | 14 | 15 |
| Fixed global array, initialized global structure, static local, fixed-array structure field, or non-null global pointer initializer | 12 | 13 |
| `float` type or float-suffixed literal | 8 | 9 |
| Unsigned syntax, an unsigned-compatible enum, `sizeof`, `__builtin_offsetof`, or a bit-field | 6 | 7 |
| None of the above | 2 | 3 |

An unused ordinary runtime function prototype does not by itself select
unsigned IR; calling one with an unsigned signature does. Explicit `extern`
declarations and function definitions are not subject to this exception.
Some unsigned source constructs remain unsupported and are rejected during IR
generation. Local fixed arrays alone do not require a newer IR version.

Source `_Bool` uses the existing one-byte `byte` representation. Conversions
normalize scalar values with `bool` before storing them, without changing the
meaning of existing IR instructions.
Enum values use the existing 32-bit `int` or `uint` representation according to
their compatible type; enum type identity and enumerator scope are enforced
before IR generation.

## Small memory-class aggregate ABI (v54 and v55)

Executable v54 extends v50 and module v55 extends v53. They additionally
accept `memN` for 1 <= N <= 16, for aggregates classified as MEMORY even
though they fit in two eightbytes (for example, packed structures with
unaligned fields). They use the existing `memN` parameter, return, indirect
call and `vaargagg` conventions, including the hidden return pointer and
eightbyte-rounded stack slots. The stack copy reads exactly N bytes, and
`retagg` and `vaargagg` copy exactly N bytes for these types; padding in the
stack slot is unspecified. A source snapshot, return buffer or `vaargagg`
destination for these types need only provide N accessible bytes. The
`vaargagg` destination remains eightbyte-aligned.

Versions through v53 still reject these spellings. The frontend does not yet
emit v54/v55 or classify small packed MEMORY aggregates; it must do both and
provide by-value snapshots to use this ABI.

Bit-field operations lower to existing byte loads and stores, integer shifts,
masks, and conversions. Aggregate classifications use the occupied bit-field
bytes; static initializers merge field bits into block data. No new IR
instructions or version are required.

## Thread-local storage (v53)

Module v53 extends v51 with `thread_local NAME`, marking an internally linked
scalar `global` definition as thread-local. The directive must appear exactly
once per marked global and cannot name an external symbol or block. Existing
`gload`, `gstore`, and `gaddr` operate on the calling thread's instance.
Static initializer relocations cannot address a thread-local object.

The x86-64 backend uses ELF local-exec TLS (`R_X86_64_TPOFF32`) and initialized
`.tdata`. Assembly generation requires `--no-entry` and linking with a runtime
that initializes TLS, such as glibc. The Crunch executable linker does not
support TLS relocations. Executable IR does not support TLS.

## Aligned string literals (v60 and v61)

Executable v60 and module v61 extend v58 and v59. `stringlit LABEL HEXBYTES 4`
requests four-byte alignment; the existing three-field form retains byte alignment.
The frontend uses the aligned form for wide strings.

## Function-lifetime stack allocation (v58 and v59)

Executable v58 and module v59 extend v56 and v57 with `stackalloc_live`.
It consumes an integer byte count and pushes a 16-byte-aligned stack address,
preserving other evaluation-stack values. A later `stackrestore` can release
the storage; the frontend retains builtin allocations until the function
returns. Allocation traps on arithmetic overflow; available stack space limits
the allocation.

## Dynamic stack storage (v50 and v51)

Executable v50 and module v51 extend v48 and v49. `stackmark` pushes the
current dynamic stack position. `stackalloc` consumes a byte count and pushes
the address of newly allocated storage, aligned to 16 bytes. `stackrestore`
consumes a previously saved stack position, releasing subsequent allocations.
Allocation traps if rounding or stack-pointer subtraction overflows.
The evaluation stack must otherwise be empty at each of these instructions.
The caller must restore only a still-live mark from the same function and
must not use pointers into released storage. Available process stack space
limits allocations. The frontend emits these instructions for automatic local
variable-length arrays and saves dimension sizes in local slots for runtime
row strides. Automatic pointers to VLAs also save their dimension sizes but do
not allocate dynamic stack storage. As in v48/v49, scalar globals are aligned
to 16 bytes. Functions with runtime-sized array parameters evaluate bounds
on entry and save row sizes in local slots; their ABI remains pointer-based.

## Aligned declarations (v48 and v49)

Executable v48 and module v49 extend v46 and v47. Scalar `global` definitions
are aligned to 16 bytes, including internally linked globals and globals with
relocations. The frontend emits explicit `globalblock` or `localblock`
alignment for other aligned objects and lowers aligned automatic scalars to
local block aliases. This version can therefore add padding between unrelated
scalar globals, without changing their size or linkage.

## Aligned SSE aggregate ABI (v46 and v47)

Executable v46 and module v47 extend v44 and v45. The signature types
`agg16asi`, `agg16ais`, and `agg16ass` describe 16-byte, 16-byte-aligned
aggregates with SSE/INTEGER, INTEGER/SSE, and SSE/SSE eightbyte classes.
They use two local slots and follow the existing aggregate register rules;
stack arguments and `vaargagg` overflow-area accesses are aligned to 16 bytes.
Older versions reject these spellings and retain their original semantics.

## Flexible aggregate ABI (v44 and v45)

Executable v44 and module v45 extend v42 and v43. The signature types
`agg16a1i` and `agg16a1s` describe a 16-byte, 16-byte-aligned aggregate whose
first eightbyte is INTEGER or SSE, respectively, and whose second eightbyte
is NO_CLASS. They use one register and one local parameter slot, but occupy
16 aligned bytes when passed on the stack. Calls, returns and `vaargagg` use
the same classification. Earlier IR versions reject these spellings.

## Fixed binary80 precision (v42 and v43)

Executable v42 and module v43 add `%.NLf` for decimal precisions from 0
through 100; `%.Lf` means zero. Higher precisions are rejected. The backend
selects `_v42` tagged formatting entry points, leaving the behavior of older
IR unchanged. Module v43 includes the v41 variadic annotations and v39
runtime fallback dispatch. A computed format also selects v42 or v43 because
its conversions are not known at compile time.

## Runtime fallback dispatch (v39)

Module v39 extends v37. `weak_fnaddr NAME` pushes the address of an optional
weak function symbol, or zero when it is undefined. NAME must be one of
`__crunch_fallback_printf`, `__crunch_fallback_fprintf`,
`__crunch_fallback_scanf`, `__crunch_fallback_fscanf`,
`__crunch_fallback_snprintf`, `__crunch_fallback_sprintf`, or `__crunch_fallback_vfork`. `callptr` accepts
up to 130 arguments in v39; the limit in earlier versions is 128.

The top-level directive `require_user_runtime NAME` requires an ordinary user
definition rather than the runtime fallback for the named runtime function.
Each name may appear only once. The backend emits a COMDAT guard that causes a
duplicate-symbol link error if the corresponding fallback is extracted. Guards
survive relocatable links and are deduplicated across modules.
They also export `__crunch_dso_guard_NAME` metadata. When linking a dynamic
executable, `crunch-ld` uses that metadata from explicitly supplied shared
inputs to reject an incompatible executable fallback. Hidden metadata is
also inspected when the ordinary symbol table is retained. Newer objects
also record guards in the ABI section, allowing detection after symbol
stripping if that section remains. This does not cover shared libraries
loaded later or only as indirect dependencies, older shared objects without
the metadata, objects that remove it, or final links performed by other linkers.

The frontend uses these markers to choose between the private runtime ABI
and an ordinary user function. Version 39 enables all v37 formatting
capabilities. Direct dynamic links of newly generated modules require
`libcrunch-fallback.a` before `libcrunch.so`; the Crunch driver supplies it.

## Variadic ABI annotations (v41)

Module v41 extends v39. `abi_variadic NAME` is a top-level annotation declaring
that a defined or externally declared function is variadic, independently of
whether it needs a `variadic` argument frame. Each annotation must name a
function with at least one named parameter and occur at most once. Functions
with a `variadic` frame must have the annotation. It does not create a frame or
change calling behavior.

## Object ABI metadata

The x86-64 backend emits a read-only `__crunch_abi` ELF section for module
functions. Each newline-terminated ASCII record is
`CABI1 KIND NAME VARIADIC RETURN [PARAMETER ...]`. `KIND` is `D` for an
externally visible definition or `R` for an external declaration. `VARIADIC`
is `variadic` when v41 carries an `abi_variadic` annotation, or an older IR
contains a variadic frame directive. It is `unknown` otherwise: legacy IR does
not reliably record variadicity when no frame is needed or for external
declarations, and absence of an annotation does not certify a fixed signature.
Types use the IR scalar and aggregate ABI spellings. Static functions are
omitted. Multiple objects contribute concatenated records; records do not
identify their original object after a final link.

`CABI1 G NAME` records a `require_user_runtime` guard. For explicit shared
inputs, `crunch-ld` uses these guard records as well as legacy guard symbols.
It checks whether the name is dynamically referenced before rejecting an
executable fallback.

The metadata describes IR-level calling conventions, not complete source
types, and may include unused declarations. Function signature records are
not used to enforce strict linking or loading. With `-Wabi-mismatch`,
`crunch-ld` compares declaration records against selected object definitions
and visible candidate definitions in directly linked shared libraries, and
warns about differences. This is a best-effort diagnostic:
it can warn about unused declarations or IR type differences that share a
calling convention, and cannot diagnose missing or incomplete records,
unverified assembly, indirect dependencies, or later loads. It does not model
all runtime symbol interposition. Absence of a warning does not establish ABI
compatibility.
Newly generated sections use `SHF_GNU_RETAIN` to survive section garbage
collection with GNU ld and crunch-ld; older objects need to be rebuilt to gain
that protection.

## Pointer difference and string precision (v36 and v37)

Executable v36 and module v37 extend v34 and v35. `ptrdiff` consumes two
pointer values, left then right, and produces their signed byte difference as
`long`. The frontend divides by the pointed-to type's size for C pointer
subtraction. As in C, both pointers must designate elements of the same array
(or its one-past position) and the element difference must fit in `ptrdiff_t`.
Formatted output also accepts `%.*s` with a signed integer precision and a
string pointer. Negative precision leaves the string unbounded.

## Address-based binary80 operations (v34 and v35)

Executable v34 and module v35 extend v32 and v33. Binary80 values are
represented by pointers to their ten-byte x86 extended-precision payload.

`extcopy` and `extneg` consume destination and source pointers and push the
destination. They copy the payload, with `extneg` toggling its sign bit.
The payloads may overlap. `extadd`, `extsub`, `extmul`, and `extdiv` consume
destination, left-operand, and right-operand pointers, in that order, and
push the destination. They compute the indicated operation in binary80.
All inputs must provide ten readable bytes and all destinations ten writable
bytes; padding is not copied or initialized.

`exteq`, `extne`, `extlt`, `extle`, `extgt`, and `extge` consume left and
right operand pointers and push an integer zero or one. Unordered comparisons
are false except for `extne`. `extbool` consumes one pointer and pushes zero
only for either signed zero, including true for NaN.

`extfrom TYPE` consumes a destination pointer and a scalar value, in that
order, stores its binary80 conversion, and pushes the destination pointer.
`extto TYPE` consumes a binary80 pointer and pushes the converted scalar.
TYPE is `float`, `real`, `int`, `uint`, `long`, or `ulong`. Floating conversions
round to nearest, ties to even. Integer conversions into binary80 are exact;
conversions out truncate toward zero. For integer conversions, the result is
unspecified if the truncated value is not representable or the input is
non-finite.

Arithmetic and comparisons use x87 with 64-bit precision, round-to-nearest,
and masked exceptions; conversions use the rounding rules above. All restore
the caller's control word afterwards. The
x87 exception status is not preserved, and these instructions do not provide
floating-point environment or trap semantics. Earlier versions reject them.

Function signatures may use `ext` as a parameter or return type. An `ext`
argument is an IR pointer to a 16-byte readable snapshot; calls pass its
contents in a 16-byte-aligned stack slot. `param INDEX SLOT ext` stores a
pointer to the incoming argument copy in the indicated local slot. An `ext`
return uses `retagg ext` with a pointer to a ten-byte readable payload and
returns the value in x87 `%st(0)`. `callagg` and `callptr` with an `ext`
return consume a result-buffer pointer before the arguments (and function
pointer for `callptr`), store the returned ten bytes there, and leave the
result-buffer pointer. The buffer must provide ten writable bytes; its padding
is untouched. Earlier versions reject `ext` in signatures.

`vaarg ext` consumes a System V argument-list pointer and pushes the address
of the next binary80 argument. It rounds the overflow pointer up to a
16-byte boundary and advances it by 16 bytes, without changing the GP or SSE
register offsets. The list and the argument must be valid, and the argument
address is valid only for the lifetime of the argument frame. Earlier versions
reject this operand.

Binary80 aggregate ABI support adds `agg16a`, an INTEGER/INTEGER aggregate
with 16-byte stack alignment, and `memNa`, a 16-byte-aligned MEMORY-class
aggregate of size N. N must be a multiple of 16 between 16 and 999992.
These types may be used for aggregate parameters and returns, and with
`vaargagg`; stack arguments and variadic overflow arguments are aligned to
16 bytes. `agg16a` otherwise follows the rules for `agg16`, and `memNa`
follows those for `memN`. Earlier IR versions reject both spellings.
An x87-class aggregate uses `ext` in a signature.

The runtime `strtold` call uses `callagg` with a binary80 result buffer,
following the `ext` return convention above. An unused declaration alone does
not select v34/v35; a call does.

## Aggregate argument-list retrieval (v32 and v33)

Executable v32 and module v33 extend v30 and v31. `vaargagg TYPE`
consumes a destination pointer and a list pointer and pushes the destination
pointer. TYPE is an aggregate parameter type. The destination must provide
eightbyte-aligned storage of the aggregate size rounded up to eight bytes.
Register-passed aggregates are reconstructed from the register-save area;
otherwise their stack copy is used. Register allocation is atomic across GP
and SSE classes, and the argument list advances accordingly. Earlier versions
reject this instruction.

## Variadic argument frames (v30 and v31)

Executable v30 and module v31 extend v28 and v29. The `variadic` directive
marks a function with at least one named parameter; it occurs once before
executable instructions. At entry, the backend saves the incoming System V
AMD64 integer and XMM argument registers in the function's stack frame.
`vastart` consumes a pointer to a writable 24-byte System V argument-list
object and initializes its register offsets, stack overflow pointer, and
register-save pointer for the first unnamed argument. Named aggregate
parameters use the same all-or-nothing register allocation as calls, and a
memory-class return consumes the hidden integer register. The saved area is
valid until the function returns. `vaarg gp` and `vaarg fp` consume a list
pointer and push the address of the next scalar argument, advancing the list
through its register-save area or stack overflow area. `vacopy` consumes
destination and source list pointers and copies the 24-byte state. The caller
must provide valid lists and matching promoted argument types. Earlier IR
versions reject these instructions.

## Seventeen-digit real formatting (v28 and v29)

Executable v28 and module v29 extend v26 and v27 with the `%.17g`
formatted conversion. Earlier versions continue to reject precision.
The backend selects the corresponding `_v28` runtime entry points.

## Integer formatted arguments (v26 and v27)

Executable v26 and module v27 extend v24 and v25. Formatted runtime calls
accept corresponding signed and unsigned argument tags when the value is
representable in both types. This permits promoted narrow unsigned integers
and other nonnegative `int` values for `%u` and `%x`, and corresponding
unsigned values for `%d` and `%c`; the same rule applies to `long` conversions.
Earlier versions retain their existing exact tag checks. The backend selects
the corresponding `_v26` runtime entry point.

## Function pointers (v24 and v25)

Executable v24 and module v25 extend v22 and v23. `fnaddr NAME` pushes the
address of a declared function. Static pointer relocations may also target
declared functions. `callptr COUNT RETURN_TYPE PARAM_TYPE...` consumes a
function pointer followed by COUNT arguments, and returns a scalar result.
For aggregate returns it additionally consumes a result-buffer pointer before
the function pointer and leaves that pointer, as in `callagg`. Aggregate
arguments use the same snapshot and ABI rules as direct calls. The signature
is explicit and checked for valid types and stack operands; runtime pointer
provenance and signature matching cannot be verified from IR alone. Indirect calls set the
System V vector-register count in `%al`, enabling source-level calls through
variadic function pointers with promoted argument types. Older
versions reject these instructions and function relocation targets.

## Integer-class structure parameters (v18 and v19)

Executable v18 and module v19 extend v16 and v17. Parameter types `agg1`
through `agg16` describe structures of the indicated byte size whose ABI
classification consists entirely of INTEGER eightbytes. They are permitted
only in `param` and `extern_func` parameter positions, not as return, storage,
or instruction types. A call supplies one `ptr` stack value per aggregate,
pointing to a snapshot with at least 8 readable bytes for sizes 1–8 or 16
readable bytes for sizes 9–16. The frontend copies the exact object bytes into
such a padded snapshot when the argument is evaluated.

`param INDEX SLOT aggN` reserves one local eightbyte slot for N <= 8 and
two for N > 8. The aggregate's first byte is at `addr SLOT` for one slot, or
`addr SLOT+1` for two slots; subsequent bytes are contiguous. Reserved slots
must not overlap other parameters or local declarations. All `param` directives
must precede executable instructions in v18/v19. Calls use the System V AMD64
INTEGER register convention: an aggregate is assigned wholly to registers
when enough are available, otherwise wholly to the stack without consuming
the remaining registers. Other arguments continue to use available registers.
Earlier versions reject the aggregate parameter names.

Executable v20 and module v21 additionally allow structure and union parameter names
with one class letter per eightbyte: `s` for SSE and `i` for INTEGER, such as
`agg8s`, `agg16is`, `agg16si`, and `agg16ss`. The suffix must contain at
least one `s`; all-INTEGER aggregates use the existing unsuffixed names.
Register allocation is atomic across both register classes: if either class
is exhausted, the whole argument is passed on the stack, without consuming
registers from the other class. Earlier versions reject these suffixes.

Executable v22 and module v23 additionally support aggregate returns and
memory-class parameters. `memN`, for 17 <= N <= 999992, describes an N-byte
MEMORY-class aggregate. It may appear as a parameter or return type.
`param INDEX SLOT memN` reserves one local pointer slot pointing to the
incoming stack copy. A `memN` call argument is a pointer to a snapshot with
at least N bytes rounded up to an eightbyte.

`callagg NAME COUNT` consumes a result-buffer pointer followed by COUNT
arguments, calls an aggregate-returning function, and leaves the result-buffer
pointer. The buffer must have writable storage of at least the return size
rounded up to an eightbyte. `retagg TYPE` returns an aggregate from a pointer
to a snapshot with that same padded size; TYPE must match the function's
aggregate return type. Calls and returns follow the System V AMD64 INTEGER,
SSE, and MEMORY conventions, including the hidden destination pointer for a
MEMORY return. `call` is only valid for scalar-returning functions. Older
versions reject these types and instructions.

## Explicit character types (v16 and v17)

Executable IR v16 and module IR v17 extend v14 and v15 with `sbyte` and
`ubyte` scalar types for signed and unsigned 8-bit characters. They have
one-byte storage and alignment; stack and local cells remain eight bytes.
Values are sign-extended or zero-extended, including across function calls.
Global initializers must be in -128..127 for `sbyte` and 0..255 for `ubyte`.
Integer casts retain the low eight bits. Floating conversions accept
`[-128, 128)` for `sbyte` and `(-1, 256)` for `ubyte` before truncation;
out-of-range and non-finite values produce zero. Earlier IR versions reject
the new names. The existing `byte` type and its behavior remain unchanged.
Wide character literals lower to signed `int` constants using the existing
`push` and integer global initializer forms; they do not introduce an IR
instruction or version.
Source-level C numeric conversions use existing typed instructions and do not
change the IR contract. In particular, truncate-first floating-to-signed conversions use explicit
range checks and an intermediate `long` conversion rather than changing the
older `cast` instruction's bounds. The `--c11-numeric` option remains accepted
but does not alter source semantics or IR version selection. Linking existing
objects cannot change the semantics with which they were compiled.

## 16-bit integers (v14 and v15)

`CRUNCH-IR 14` and `CRUNCH-IR 15` extend v12 and v13. They add signed
`short` and unsigned `ushort` scalar types, with two-byte size and alignment.
Local slots and stack cells remain eight bytes; signed values are sign-extended
and unsigned values zero-extended. Indirect storage, aliases, pointer
arithmetic, function parameters and results use the corresponding 16-bit
representation and System V AMD64 integer ABI. Integer conversions retain the
low 16 bits. Floating-to-short conversions check `[-32768, 32768)` for
`short` and `(-1, 65536)` for `ushort` before truncation, producing zero
outside the interval or for NaN and infinity. Global `ushort` initializers
are decimal values from 0 to 65535. Versions 2 through 13 do not accept these
type names.

## Fixed global arrays and static relocations (v12 and v13)

`CRUNCH-IR 12` and `CRUNCH-IR 13` extend v10 and v11 respectively.
Programs with fixed global arrays, initialized global structures, static local
arrays or scalars, fixed-array structure fields, or global pointer address
initializers use these versions.
Programs also using a short type use v14 or v15 instead.
Array and initialized structure storage use
`globalblock` and, for constant byte prefixes, `blockdata`; omitted bytes
are zero. Arrays declared only with `extern` use `extern_global NAME byte`
as an addressable symbol.

`blockreloc BLOCK OFFSET TARGET ADDEND` places an eight-byte address at
the byte offset in a global block. `globalreloc NAME TARGET ADDEND` sets
the initial value of a `global NAME ptr 0`. The target must name exactly one
declared global, global block, global alias, or string literal; the addend is
a signed decimal 64-bit byte offset. Relocations must fit their block, must
not overlap, and any overlapping `blockdata` bytes must be zero. A scalar
global relocation must be unique and cannot be combined with `globalbytes`.
The backend emits writable data with symbolic relocations, allowing static,
dynamic, and position-independent linking. Versions 2–11 reject these new
directives.

## Initialized blocks (v10 and v11)

`CRUNCH-IR 10` and `CRUNCH-IR 11` extend v8 and v9 respectively. A
`blockdata NAME HEXBYTES` directive supplies the initial byte prefix of a
matching `globalblock`; any remaining bytes are zero. The hexadecimal data
must be nonempty, consist of complete bytes, fit the block, and appear at
most once per block. Initialized blocks are emitted as writable static data.
Versions 2–9 retain their existing meaning and reject this directive.

## Binary32 IR (v8 and v9)

`CRUNCH-IR 8` is the executable form and `CRUNCH-IR 9` the module form.
They extend v6 and v7 respectively, including their unsigned types and
optional initializer; existing versions are unchanged. `crunch-asm --validate`
accepts these versions without writing assembly.

The scalar type `float` denotes IEEE-754 binary32. `pushf 0xXXXXXXXX`
pushes exactly eight hexadecimal digits of raw binary32 bits (including
signed zero, infinities and NaNs). `global NAME float 0xXXXXXXXX` uses the
same representation. Float values occupy the low 32 bits of an IR stack or
local-slot cell, with the upper 32 bits zero; addressable floats occupy four
bytes and have four-byte alignment. Float pointers, indirect loads and stores,
aliases, dynamic arrays, and pointer arithmetic use a four-byte element size.
Function parameters and results preserve the float type, including in external
signatures.

`add`, `sub`, `mul`, `div`, `neg`, comparisons, `bool`, and `not` accept
`float`. Arithmetic and conversions to `float` round to binary32 using
round-to-nearest, ties-to-even. Comparisons use IEEE-754 unordered behavior;
both signed zeros are false and NaNs are true for boolean conversion.
The backend temporarily masks floating-point exceptions and selects the required
rounding and gradual-underflow mode for float operations. It restores the
caller's floating-point control, retaining accrued exception flags.
Integer-only instructions do not accept `float`; `pow` continues to use
`real`. `cast` explicitly converts between `float` and numeric types.
Conversion from `float` to an integer uses the checked intervals described
under [`cast`](#instructions). A `float` value cannot be
implicitly substituted for an integer or `real` stack value, or vice versa:
emit an explicit `cast`, including for integer literals. Control-flow merges
must preserve that distinction. Before a formatted-output runtime call, float
arguments must be explicitly converted to `real` and use the existing real tag.

The verifier checks literal syntax, typed stack flow, function signatures,
scalar storage, and the float-specific instruction restrictions, including
unreachable instruction operands. As with older IR, it is not a proof of
pointer provenance or runtime memory safety.

## Separate-compilation modules

`crunch-ir --module` emits v3, v7, v9, v13, v15, v17, v19, v21, or v23 modules, and `crunch-asm`
accepts v2 through v23.
The `crunch` driver uses module mode for `-c` and multi-input builds; a
single-source executable and intermediate-output modes retain executable-oriented
call and validation behavior. Scalar and pointer globals are supported in all
versions; historical dynamic-array global IR is also accepted.

### Modules and declarations

A module represents one source translation unit and starts with
`CRUNCH-IR 3`, `CRUNCH-IR 4`, `CRUNCH-IR 7`, `CRUNCH-IR 9`, `CRUNCH-IR 11`,
`CRUNCH-IR 13`, `CRUNCH-IR 15`, `CRUNCH-IR 17`, `CRUNCH-IR 19`, `CRUNCH-IR 21`, or
`CRUNCH-IR 23`. Unless changed below, its instructions, value representation,
and definition directives have the IR v2 semantics described in this document.
IR v5 retains v2's executable-oriented call and validation rules but adds the
initializer directive described below. IR v6 and v7 add unsigned types and
make the initializer optional. Existing v2 through v5 input is not reinterpreted.

The following module-scope directive declares a function without defining it:

```
extern_func NAME COUNT RETURN_TYPE [PARAMETER_TYPE ...]
```

`extern_func` has exactly `COUNT` parameter types in source order; `COUNT` is
zero for a function with no parameters. Parameter and result types use the
existing IR ABI types: `byte`, `int`, `long`, `real`, and `ptr`; `void` is
allowed only as a result type. Versions 6 and later additionally allow `uint` and
`ulong`; versions 8 and later also allow `float`, versions 14 and later allow
`short` and `ushort`, and versions 16 and later allow `sbyte` and `ubyte`.
Versions 18 and later also allow aggregate parameters; aggregate returns
require version 22 or later. External variadic calls use `fnaddr` and `callptr`
in v24/v25 or later. Variadic definitions have the same IR signature as their
fixed parameters and may not yet access unnamed arguments.
Source-level `void*`, like other supported object pointers, uses the `ptr`
ABI type for parameters, results, and storage; no IR format change is needed.
Generated runtime calls align the stack to the System V AMD64 ABI even when
other expression values remain on the evaluation stack.

All IR versions also accept `extern_global NAME TYPE`, where `TYPE` is `byte`,
`int`, `long`, `real`, or `ptr`. It declares a linker symbol without allocating
storage; v6 and later also accept `uint` and `ulong`, and v8 and later accept `float`.
Repeated declarations and a definition must agree on type; an external
declaration cannot be marked `internal`. Undefined symbols are link errors.
In legacy IR, an external dynamic array uses two declarations: `extern_global NAME ptr` for
its data pointer and `extern_global NAME$length int` for its length. The
corresponding definition supplies both symbols. The `$length` suffix is the
only `$` form allowed in external global names and requires type `int`. These
declarations allocate no storage, and resizing updates both shared symbols.

For example, a module can call a function defined elsewhere:

```
CRUNCH-IR 3
extern_func scale 1 real real
func compute 0 real
pushr 0x4000000000000000
call scale 1
ret real
end
```

Function definitions retain the `func` and `param` directives. Repeated
declarations of a symbol in a module must agree exactly on its IR types and
parameter count. A declaration and definition of the same symbol must also
agree; a module may contain at most one definition of a symbol. Functions and
globals share a symbol namespace. A name cannot be both a function and a
global. Declarations do not weaken existing validation: an ordinary `call`
must refer to a declared or defined function, its arguments and result must
match that function's signature, and global accesses must match a declared or defined
global's type. Invalid IR is rejected even if the invalid instruction
is unreachable, as in v2.

`ptr` describes an ABI class, not the complete source type. The frontend must
still check compatible source-level declarations, including pointee types and
qualifiers and array element types, before lowering them to IR. A structure-array
element layout must agree across translation units; the linker cannot check it.
External structure objects or field aliases require a later linkage contract.

### Calls, linkage, and startup

Ordinary calls to defined and external functions use the existing Linux
x86-64 System V calling convention, including the independent integer and
floating-point register classes and stack-passed arguments. The signature
controls both validation and code generation. IR v3, v4, v7, and v9 distinguish runtime
calls using `runtime_call NAME COUNT`, with the same stack behavior and
runtime-specific argument rules as the corresponding v2 runtime call.
An ordinary `call` always names a module-defined or externally declared
function; it is never resolved by guessing from a runtime routine's name.
This keeps user functions and runtime operations unambiguous, including
`printf`, `fprintf`, `sprintf`, `snprintf`, and their private runtime ABI.

For a referenced source function with no definition in the translation unit,
module output uses ordinary linkage for fixed-ABI library routines. It emits
v39 dispatch for the formatted routines and `vfork`, whose runtime ABIs differ.
Linked user definitions take precedence; otherwise, the fallback archive
supplies the runtime implementation. An incompatible declaration emits a
`require_user_runtime` guard. Explicit `extern` declarations use ordinary
linkage. An unresolved ordinary external function is rejected in executable
mode.

Definitions have external linkage unless designated internal with the
`internal` directive described below. External declarations generate references to the same
linker symbols as the corresponding definitions. Generated string literals,
branch labels, allocation helpers, and other implementation-only symbols must
be local to their object, so names reused by different modules cannot collide.
References to `libcrunch.a` runtime symbols retain their existing private
linkage conventions. Module IR reserves `_start` and names beginning with
`__crunch_` for the entry point and runtime, including for internal symbols and
external declarations; the backend rejects conflicting names. Generated string
labels use the reserved `__crunch_string_` prefix.

A module need not define `main`. If it does, `main` must satisfy the existing
entry-point signature rules, and its object supplies the single Linux
`_start` that runs Crunch module initializers and calls `main`. A module without
a definition of `main` emits no `_start`. The driver requires a globally
defined `_start`; an assembly entry point may be used without a Crunch `main`.
GNU `ld` invoked directly
can warn rather than fail when the entry point is missing, so callers invoking
it directly must check that condition. Relocatable
objects, including a combined object produced by a relocatable link, need not
have either symbol. Custom entry points and other startup runtimes must follow
the initialization contract below.

`crunch-asm --no-entry` suppresses the generated `_start`, allowing the object
to use a C runtime's startup. Initializers remain registered in `.init_array`.
This option does not change IR call conventions or supply any libraries.

The linker resolves names, but ordinary ELF symbols do not encode Crunch
signatures. Thus the compiler validates each module against its own
declarations, while compatible declarations across separately compiled units
are the programmer's responsibility, normally through shared headers. A
successful link does **not** prove cross-module type compatibility. Undefined
references and duplicate externally linked definitions are link errors;
detecting incompatible signatures across objects requires additional metadata
and is not promised by this contract.

### Global initialization

Supported integer and real constant-expression scalar initializers and
zero-initialized storage use static data and retain their v2 representation.
String-literal global byte arrays use static data; other
global-array initializers and sized declarations require runtime execution.
Older frontend output placed these instructions at the beginning of `main` in
IR v2; the backend continues to execute that IR as written.

IR v4 and v5 require a top-level `initializer NAME` directive exactly once.
IR v6 and later allow it at most once. When present, it must name an `internal`
function defined in the same module with zero
parameters and an `int` result. The backend rejects missing, duplicate,
external, or incompatible initializers in versions requiring one, and rejects
duplicate, external, or incompatible initializers in v6 and later. Historical
frontend output used that function to initialize global arrays. The backend registers an
object-local once-guarded wrapper in both `.init_array` and the Crunch startup
registry. V2 and v3 retain their existing semantics and cannot use this directive.

The generated Crunch `_start` runs Crunch object initializers before `main`;
a standard C runtime runs them via `.init_array`. A custom assembly `_start`
must call `__crunch_run_initializers` from `libcrunch.a` before using initialized
globals unless the executable's startup has already run them. It takes no arguments and follows the System V AMD64 calling
convention. It may be called repeatedly, including after C startup, without
reinitializing an object. Startup and explicit calls must be serialized: the
guard does not provide thread synchronization.

The shared runtime's helper requires both Crunch registry boundary symbols to
be exported in the loader scope before `libcrunch.so` is loaded. It reports an
error and sets Crunch `errno` to `EINVAL` if only one boundary is available;
if neither is available, there is no registry to traverse. Its references are
resolved when the shared library loads, not when the helper is called.

Initializers run in linker input order, including through `ld -r`; within each
object, declarations run in source order. Cross-module dependencies must
respect that order. C runtime `.init_array` ordering relative to non-Crunch
constructors follows the host linker/runtime rules. Crunch `_start` and the
explicit helper run only Crunch initializers, not unrelated C constructors.
Static archives follow normal linker extraction rules: an archive member is
initialized only if the linker includes it. In historical array initializers,
allocation failure leaves the array empty and is not retried. The backend's
static-array registry protects pointers into static byte-array data across
linked objects, including objects combined by `ld -r`.

The generated `_start` also calls the weak `__crunch_run_finalizers` hook
when `main` returns, if it is defined. The `exit()` runtime helper calls the
same hook before exiting. `crunch-ld -dynamic` supplies this hook for
executable `.fini_array` entries; it does not run shared-library finalizers.
Custom entry points must call the hook explicitly. See `src/linker/README.md`.

## Instructions

- `internal NAME`: mark a defined function, scalar global, or global block as
  object-local. The directive appears at module scope and must name exactly one
  definition other than `main`; repeated or unresolved directives are invalid.
  Dynamic global-array pointers and their `$length` symbols are marked
  separately. This is an IR v2 extension; v2 without the directive is unchanged.
- `globalbytes NAME HEXBYTES`: initialize a global byte-array descriptor with
  writable static data. `NAME` must name a `ptr` global initialized to zero;
  `NAME$length` must name an `int` global initialized to the decoded byte
  count. The even-length hexadecimal data must end in `00`, including for an
  empty string. A name may have only one `globalbytes` directive. These
  directives are validated before output and do not emit executable
  initialization instructions. This is an IR v2 extension.

The x86-64 backend records each object's initial static-array address range in
the `__crunch_static_arrays` ELF section. Resize code scans the linker-collected
ranges before releasing storage, including when a pointer into static data is
passed from another object. GNU `ld` supplies the weak section-boundary symbols
`__start___crunch_static_arrays` and `__stop___crunch_static_arrays`, which are
reserved for this purpose. All objects participating in this mechanism must be
generated by a backend that emits the registry-aware resize code.
- `func NAME COUNT TYPE`, `param INDEX SLOT TYPE`, `local SLOT TYPE`, `end`: function and storage declarations. Types are `byte`, `int`, `long`, `real`, or `void` where applicable.
- `push INTEGER`, `pushr 0xBITS`: push a signed integer literal or a raw IEEE-754 binary64 literal bit pattern.
- `load SLOT`, `store SLOT TYPE`, `dup`, `drop`: access a local slot and manipulate the evaluation stack. `store` pops and normalizes the value to its declared type.
- `cast SOURCE_TYPE DESTINATION_TYPE`: convert the top stack value. Integer
  narrowing keeps low-order bits. For `real` or `float` to `int` or `long`,
  the original value must be in `[-2^(N-1), 2^(N-1))`, where N is the
  destination width. For conversion to `uint` or `ulong`, it must be in
  `(-1, 2^N)`; for `float` to `byte`, in `(-1, 256)`. Accepted values are
  truncated toward zero. The IR also permits `real` to `byte`: it checks the
  `int` interval and leaves the truncated integer in the stack cell; byte
  storage and other byte-normalizing operations use its low eight bits.
  With floating-point exceptions masked, NaN, infinity, and values outside the
  applicable interval yield zero. An unmasked invalid-operation exception can
  interrupt a `real` signaling-NaN check. These checks apply before truncation.
  Unsigned and `float` types require the IR versions that introduce them.
- `neg TYPE`, `not TYPE`, `bitnot TYPE`, `bool TYPE`: unary arithmetic, logical-not, bitwise-not, and scalar-to-boolean normalization.
- `add TYPE`, `sub TYPE`, `mul TYPE`, `div TYPE`, `rem TYPE`, `band TYPE`, `bor TYPE`, `bxor TYPE`, `shl TYPE`, `shr TYPE`: typed binary operations. Integer operations honor `byte` unsigned 8-bit, `int` signed 32-bit, and `long` signed 64-bit widths. `real` arithmetic uses IEEE-754 binary64.
- `pow real`: consumes two `real` values (base first, exponent on top) and
  pushes a `real` power. Both stack values must be `real`, not integer
  literals. This extension is accepted in v2, v3, and v4; generated assembly calls
  the selectively linked `__crunch_pow` member of `libcrunch.a`. See the
  language specification for numerical behavior. The symbol `__crunch_pow` is
  reserved in both IR versions.
- `eq TYPE`, `ne TYPE`, `lt TYPE`, `le TYPE`, `gt TYPE`, `ge TYPE`: typed comparisons producing integer 0 or 1. Floating-point comparisons follow IEEE-754 unordered/NaN behavior.
- `label L<N>`, `jmp L<N>`, `jz L<N>`, `jnz L<N>`: control flow. Conditional branches pop an integer boolean and branch on zero/nonzero.
  Source `goto` and named labels lower to these existing function-local branches.

The x86-64 backend validates IR before emitting assembly. It rejects duplicate function names and labels, malformed label spellings, undefined or cross-function branch targets, unknown instructions (including in unreachable blocks), unknown function calls, incorrect call argument counts, evaluation-stack underflow, inconsistent stack depth at control-flow merges, and reachable fall-through past `end`. Diagnostics for instruction-level failures include the IR line number and function name. Every evaluation-stack value occupies one 64-bit cell. The verifier now tracks abstract stack types through reachable control flow, checks operation and cast compatibility, validates typed local/global storage and call arguments, and rejects incompatible stack types at control-flow merges. Integer literals are treated as context-polymorphic until consumed; stores and integer operations may normalize between integer widths as defined by IR v2. The verifier remains conservative and is not a proof of memory safety: pointer provenance, full runtime-call prototypes, all declaration/operand grammar constraints, and exact return-stack cleanliness remain follow-up work.
- `call NAME COUNT`: direct call. Arguments are evaluated left-to-right and the result is pushed as a raw 64-bit cell. The callee's signature determines argument types and ABI register classes.
- `ret TYPE`: pop a return value, restore the caller frame, and return it in `%rax` for integer types or `%xmm0` for `real` (raw bits are transferred from the stack cell). `void` functions discard the internal placeholder value.

Every evaluation-stack value occupies one 64-bit cell; a `real` value is represented by its raw binary64 bits while on the IR stack. Local slots also occupy eight bytes, with normalization on stores and typed operations. The `_start` entry point calls `main` and exits with its integer result. Function calls follow the Linux x86-64 System V ABI, using the independent general-purpose and XMM register sequences and stack-passing overflow arguments. The caller dynamically aligns `%rsp` to 16 bytes before calls, including nested calls inside expressions.

Short-circuit `&&` and `||`, `if`/`else`, `for`, `while(condition)`, `break`, and `continue` are lowered to labels and branches. Unsupported AST constructs fail with a diagnostic instead of silently generating incorrect output.

`crunch-ir [--module] [--c11-numeric] [-Wunused-variable] [-Wcase-fall-through] [-Wall] [FILE.bc|-]` runs lexing, parsing, semantic analysis, and emits IR on standard output. `crunch-opt [FILE.bir|-]` is an IR optimization stage that currently copies its input byte-for-byte, without validation or optimization. `crunch-asm [--no-entry] [--source-file NAME] [FILE.bir|-]` validates and translates IR to GNU x86-64 assembly on standard output; `--source-file` adds a `.file` directive for the given source name. Without this option, standalone IR translation omits `.file`, since IR does not carry a source filename. `crunch-asm --validate [FILE.bir|-]` validates without emitting assembly. The textual format is versioned so incompatible IR changes can be introduced explicitly.

The `crunch` driver runs `crunch-opt` between `crunch-ir` and `crunch-asm` for
source compilation. `--emit-ir` stops before the optimizer.

The `crunch` driver supplies each source's basename (or `<stdin>` for standard
input) to the backend. NUL-terminated `stringlit` bytes are emitted with
`.string`; legacy IR literals lacking a final NUL use `.ascii` so their byte
length remains unchanged. Embedded NULs and non-printable bytes are escaped.
Wide source strings use aligned little-endian 32-bit elements and a four-byte
terminator.
Initialized data blocks use the same byte-preserving directives, with
relocations and zero padding emitted separately.

## Assembly cleanup

The x86-64 backend applies a small, semantics-preserving peephole pass to generated assembly. In particular, an immediately adjacent `push %rax` / `pop %rax` pair is removed because it restores the stack pointer and leaves `%rax` unchanged. The optimization is deliberately local and does not alter the Crunch IR contract.

The IR does not encode `volatile` separately: frontend lowering emits the
required accesses as ordinary memory operations. The backend must preserve
those operations and their emitted order, even when their values are discarded.
Discarded volatile structure and union reads are lowered to byte reads of the
entire object representation, including padding. This contract does not
guarantee naturally sized hardware accesses or atomicity; see the volatile
limitations in the language specification.


### Pointer and indirect-memory instructions (IR v2)

- `addr SLOT` pushes the address of a local slot.
- `loadind TYPE` pops an address and pushes the numeric value stored there.
- `storeind TYPE` pops a value and an address, stores the value, then pushes the stored value (so an expression statement can `drop` it).
- `ptradd TYPE` and `ptrsub TYPE` consume an address and an integer element offset and push the adjusted address. `TYPE` is the pointed-to numeric element type; offsets are scaled by its size (`byte` 1 byte, `int` 4 bytes, `long` and `real` 8 bytes).
- `ptr` is the IR storage and ABI class for supported pointer types, including pointers to pointers. Pointers occupy one 64-bit word and use the general-purpose register argument class under System V AMD64.

For compound assignments to indexed numeric elements or dereferenced numeric
pointers, the frontend evaluates the address once, duplicates it, loads the
value, performs the operation using the same conversions as a local compound
assignment, and stores the converted result with `storeind`.
Numeric increment and decrement on indirect lvalues likewise evaluate the
address once; postfix operations preserve the original value in a typed
temporary before storing the update.

The backend supports address-of local variables and scalar structure members, dereferencing numeric and pointer values, indexing through numeric, structure, and pointer elements, pointer comparisons, pointer-plus/minus-integer expressions, and increment/decrement and `+=`/`-=` on pointer variables, fields, and indirect pointer lvalues. Pointer arithmetic follows C-style element scaling, and each indirect lvalue address and offset is evaluated once. Structure pointers can be passed, returned, stored in local and global variables and structure fields, indexed or advanced with the structure's padded byte stride, and used for indirect member access. Pointer-to-pointer values use the same `ptr` ABI; supported small integer structures may also be passed by value, but structure-valued returns remain unsupported. Pointers can be compared with null, but dereferencing them is not automatically null-checked.

## Unsigned integer IR (v6 and v7)

`uint` is an unsigned 32-bit type and `ulong` an unsigned 64-bit type.
They are valid in numeric type positions, including function signatures,
locals, globals, aliases, indirect memory operations, and dynamic-array
element types. Their memory sizes and pointer strides are 4 and 8 bytes,
respectively; local slots and evaluation-stack cells remain 8 bytes. `uint`
values are zero-extended when normalized. Function arguments and results use
the System V AMD64 integer ABI and can interoperate with corresponding C
`unsigned int` and `unsigned long` values.

`pushu VALUE TYPE` pushes a concrete unsigned value. `TYPE` must be `uint` or
`ulong`; `VALUE` is decimal digits only, in the range 0 through 4294967295 or
18446744073709551615, respectively. Unsigned global initializers use the same
decimal syntax and range. Invalid `pushu` instructions are rejected even in
unreachable blocks. The existing signed `push` is unchanged. Unsigned types
and `pushu` are rejected in v2 through v5.

Typed unsigned arithmetic and bitwise operations retain low-order bits at the
type width. Division, remainder, comparisons, and right shifts interpret their
operands as unsigned, and right shifts fill with zeros. The producer must supply
valid nonzero divisors and shift counts within the operand width. Integer casts
retain destination-width low bits, interpreted according to destination
signedness. Unsigned-to-`real` converts the full value, rounding to binary64;
`real`-to-unsigned follows the checked interval described under `cast`. The IR
producer is responsible for emitting casts for
source-language conversions. Legacy dynamic-array counts retain the signed
count contract. Historical frontend output checked unsigned source counts
against 2147483647 and supplied zero on overflow, causing resize to release
the old allocation and return an empty descriptor.

The frontend lowers unsigned scalar expressions, declarations, pointers, arrays,
function calls, returns, constant globals, and formatted-output arguments to
these versions.
Source `long long` and `unsigned long long` retain distinct semantic types
and ranks, but lower to the existing `long` and `ulong` IR types, respectively;
their layout and ABI representations are identical on this target.


## Global scalar data (IR v2 extension)

`global NAME TYPE VALUE` defines a scalar data symbol; `extern_global NAME TYPE` declares one without storage. Values are serialized as integer decimal values or raw hexadecimal IEEE-754 bits for `real`; omitted source initializers become zero. `gload NAME TYPE` pushes the value of a global symbol and `gstore NAME TYPE` pops and stores a value. Legacy dynamic global arrays use a pointer symbol named `NAME` and a companion `NAME$length` integer symbol. Normally both start at zero; `globalbytes` gives a string-initialized byte array a static pointer and initial length. `globalblock NAME SIZE ALIGN` declares a zero-initialized contiguous global structure or fixed-array object; `globalalias ALIAS BLOCK OFFSET TYPE` names a typed field at a byte offset within that object. External structures and unions use `extern_global NAME byte` and address their members relative to `gaddr NAME`; no field aliases are needed. The x86-64 backend emits structure blocks in `.bss` with the requested alignment and maps aliases to byte offsets, while scalar globals use `.data` and RIP-relative addressing.

The x86-64 backend accepts global blocks and `ptradd_bytes` strides up to
64 MiB each.

Compound assignments to global numeric variables use `gload` and `gstore` with
the same numeric operations and conversions as local compound assignments.


## Structure members (Milestone 2.17)

Nested structure members are flattened to IR storage names: local field aliases have unique generated identifiers, while global aliases use double-underscore-separated paths. Global and local structures use contiguous aligned storage. The `gaddr NAME` instruction pushes the RIP-relative address of a declared global symbol or global field alias. The IR generator supports address-of scalar globals, pointer globals, local and global structure variables, structure-array elements, scalar members and nested members, and whole-structure assignment emits field copies recursively, including copies between structure variables and indexed structure-array elements. Local structure declarations may use another instance of the same type as an initializer. Positional structure lists initialize local storage at runtime and global storage with `blockdata` and `blockreloc`; omitted fields are zeroed. Structure-valued parameters and return values remain unsupported.

Static local structures use internally linked global blocks and private field
aliases. Constant initializers use the same data and relocation machinery as
file-scope structures; storage remains distinct across scopes and modules.

Compound assignments to numeric fields use the same conversions as local
numeric assignments. For fields reached indirectly, the containing address
is evaluated once and the update uses `loadind` and `storeind`.


## Fixed arrays

Automatic local fixed arrays use aligned `localblock` storage and `laddr` for
their object address. In v34 and v35, a `localblock` alignment must be a power
of two no greater than 16 bytes; earlier versions accept other alignments but
do not guarantee them. Local blocks total at most 16,777,216 bytes per function,
including alignment padding;
available process stack space can impose a smaller practical limit.
Initialized arrays are zero-filled before their explicit
elements are stored. String initializers copy the decoded bytes and, when it
fits, the terminator. Global fixed arrays use static blocks, with constant
numeric, string, null-pointer, or supported symbol-and-offset pointer
initializers, including positional structure arrays. Fixed-array
structure fields use addresses within the containing block, or computed
addresses for indirect access. Whole-structure copies copy fixed-array fields
bytewise. Static local fixed arrays use internally linked global blocks with
constant initializers and lexical-scope-specific names. Positional aggregate
initializers support nested braces, brace elision, and field or array
designators. Designated operations initialize local storage in source order;
static data and relocations reflect later overriding initializers. Semantic
models must remain alive through IR generation.

## Legacy dynamic-array IR

The backend continues to accept the legacy `array_alloc TYPE`,
`array_resize TYPE`, and `array_resize_bytes STRIDE` instructions, although
source programs cannot declare or resize legacy dynamic arrays. `array_alloc`
consumes an element count and returns the actual count followed by the pointer.
The resize instructions consume the old pointer, old count, and requested count,
and return the actual count followed by the pointer. The byte-stride form is
used for structure elements. A zero, negative, overflowing, or failed request
returns a null pointer and zero length. Successful resize preserves the
intersecting prefix; uninitialized new bytes have unspecified contents.
Static byte-array data identified by `globalbytes` is not unmapped by resize.
Pointer elements are shallow-copied; their pointees are not freed.

`ptradd_bytes STRIDE` scales an integer offset by a positive byte stride and
is also used by current fixed-array indexing and structure-pointer arithmetic.
For strides too large for a signed immediate multiplier, arithmetic multiplies
by the stride and uses `ptradd byte` instead.

## Standard-library memory and output routines

Library `printf` uses `call printf COUNT` in executable IR and
`runtime_call printf COUNT` in module IR, where COUNT is odd and
between 1 and 255. The format pointer is pushed first; each formatted argument
then contributes a value cell followed by an integer type-tag cell: 1 for
byte/int, 2 for long, 3 for real, and 4 for byte pointer. IR v6 and later also
use 5 for unsigned int and 6 for unsigned long. IR v34 and v35 add tag 7
for a pointer to a ten-byte binary80 snapshot. The result is an `int`
byte count or -1. Values are evaluated left-to-right, including unused trailing
arguments. The backend passes a pointer to the format cell and the number of
value/tag pairs to `__crunch_printf` for v2-v5, `__crunch_printf_v6` for
v6-v25, `__crunch_printf_v26` for v26-v27, `__crunch_printf_v28`
for v28-v33, `__crunch_printf_v34` for v34-v35, `__crunch_printf_v36`
for v36-v37 and v39/v41, or `__crunch_printf_v42` for v42-v43, with pairs at descending
addresses. The backend similarly selects
versioned entry points for `fprintf`, `sprintf`, and `snprintf`, so older IR still rejects
the new conversions and tags. This is a private Crunch runtime ABI, not the
C/System V variadic ABI. User-defined
functions named `printf` retain normal signature checking and calling conventions.
The library checks conversion/tag agreement and argument availability at runtime.
Supported conversions are `%d`, `%ld`, `%lld`, `%f`, `%s`, `%c`, and `%%`;
v6 and later also support `%u`, `%lu`, `%llu`, `%zu`, `%x`, `%lx`, and `%llx`.
`%zu` uses the unsigned-long
tag on this target. The hexadecimal conversions use lowercase
digits and require the corresponding unsigned type tags through v25; v26 and
v27 also accept representable signed values. Conversions other
than `%%` accept literal widths and the `-` and `0` flags. The only supported
precisions include `%.17g` in v28 and later, `%Lf` and `%.17Lg` in v34 and
later, `%.*s` in v36 and later, and `%.NLf` for N from 0 through 100 in v42
and later. Dynamic widths and dynamic `%Lf` precision remain unsupported.

Library `vsnprintf` uses `call vsnprintf 4` in executable IR or
`runtime_call vsnprintf 4` in module IR. Its arguments are destination pointer,
integer capacity, format pointer, and a pointer to a System V AMD64 `va_list`.
The backend validates these types and calls `__crunch_vsnprintf`. It accepts the
same current formats as v42, including `%.*s` and `%.NLf`, and consumes the
argument list. These unversioned `va_list` entry points accept the current
formats even when called from older IR.
Binary80 arguments are taken from the 16-byte-aligned overflow area.

Library `fprintf` uses `call fprintf COUNT` in executable IR and
`runtime_call fprintf COUNT` in module IR. Its stream pointer and format pointer
are pushed first, followed by the same value/tag pairs as `printf`. COUNT is
even and between 2 and 256. The backend validates both pointer cells and passes
the stream, format-cell address, and pair count through a private runtime ABI.
User-defined or explicitly external `fprintf` functions use normal calls.

Library `snprintf` uses `call snprintf COUNT` in executable IR and
`runtime_call snprintf COUNT` in module IR. Its fixed cells are a destination
pointer, an integer capacity, and a format pointer, followed by the same
value/tag pairs as `printf`. COUNT is odd and between 3 and 257. The backend
validates those cell types and passes the format-cell address and pair count
through a private runtime ABI; the capacity and destination are in the next
two higher-address cells. User-defined or explicitly external `snprintf`
functions use normal calls.

Library `sprintf` uses `call sprintf COUNT` or `runtime_call sprintf COUNT`.
Its fixed cells are a destination pointer and format pointer followed by the
same value/tag pairs as `printf`. COUNT is even and between 2 and 256.
The backend passes the format slot and count to the corresponding versioned
`__crunch_sprintf` entry point. User-defined or explicitly external `sprintf`
functions use ordinary calls.

Library `scanf` and `fscanf` use `call` in executable IR and `runtime_call`
in module IR. Their fixed cells are respectively a format pointer or a stream
and format pointer, followed by destination-pointer/tag pairs. The frontend
uses tag 4 for each pointer. COUNT is odd from 1 through 255 for `scanf` and
even from 2 through 256 for `fscanf`. The backend passes the format slot and
destination count to `__crunch_scanf` or `__crunch_fscanf`; the latter also
receives the stream. Module calls dispatch to a user definition when present.

The x86-64 backend recognizes runtime calls for the memory/string routines, `printf`, and `exit`, plus legacy IR calls to `end` and `print`. Runtime helper internals for numeric and string formatting are not callable as source-language functions or IR runtime calls. Their argument cells are pushed left-to-right, as for other calls. `memcpy` takes destination pointer, source pointer, and byte count, returns the destination pointer, and has the usual non-overlapping-range precondition. `memset` takes destination pointer, byte value (low 8 bits used), and byte count, and returns the destination pointer. `memcmp` compares the requested number of bytes and returns -1, 0, or 1 according to unsigned-byte lexicographic ordering. `strcmp` takes two NUL-terminated byte-string pointers and returns -1, 0, or 1 according to unsigned-byte lexicographic ordering. `strlen` returns the number of bytes before a NUL terminator. The legacy IR call `print` takes a NUL-terminated byte-string pointer, writes its bytes to standard output without adding a newline, and returns 0 on success or -1 on write failure; source-level `print` is no longer provided. `printf` handles formatted numeric and string output. `exit` and the legacy IR call `end` take an integer exit status and do not return. They run any managed executable-finalization hook, then terminate using the Linux x86-64 `exit` syscall; they do not implement standard C stream flushing or `atexit` handlers. `strlen` and `strcmp` are ordinary library functions and can be overridden. `memcpy`, `memset`, and `memcmp` are also exposed to source programs as `copy`, `fill`, and `compare`, respectively. The source-level byte count must be an integer type; zero performs no memory access and negative signed counts are clamped to zero before the call. `copy` requires non-overlapping ranges and a writable destination.

The `<ctype.h>` classification and case-conversion functions are runtime
calls with one integer argument and an integer result. They use ASCII C-locale
semantics; the valid input domain is EOF or an unsigned-char value.

For compatibility, IR runtime calls to `strlen`, `fread`, and `fwrite` push
`long` cells. Source declarations return `size_t` (`unsigned long`); the
frontend emits `cast long ulong` after those calls in unsigned-capable IR.

`call input 2` in v2 (`runtime_call input 2` in v3) consumes a writable byte pointer and an integer maximum character
count and returns an int count (or -1 with errno). It calls `__crunch_input`
from `libcrunch.a`; the buffer requires maximum plus one bytes for the NUL.

`call __errno_location 0` in v2 (`runtime_call __errno_location 0` in v3)
pushes a pointer to the runtime's process-wide
writable `int` error number. It links to `__crunch___errno_location` in
`libcrunch.a`; this is the same storage declared by `extern_global errno int`
and initialized to zero. File I/O runtime calls
return -1 on Linux syscall failure and store the positive error number there,
leaving it unchanged on success. No IR format version change is required.

Assembly routines may call the hidden `_crunch_errno_location` symbol with no
arguments; it returns the address of that same process-wide, writable `int`
in `%rax`. It is an internal alias of `__crunch___errno_location`, not a new
source-language or IR routine. The storage is not thread-local.

The runtime calls `mmap` (six arguments, pointer result), `munmap` (two,
integer result), `mremap` (five, pointer result), and `map_failed` (one,
integer result) use the System V integer argument registers. The public
mapping wrappers and the hidden `_crunch_mmap`, `_crunch_munmap`, and
`_crunch_mremap` assembly symbols convert Linux errors to -1 and set the
shared `errno`, leaving it unchanged on success.
`map_failed` checks the -1 pointer sentinel without dereferencing it.

`call strtoul 3` in v6/v8 and `runtime_call strtoul 3` in v7/v9 consume the input
pointer, optional end-pointer address, and integer base, and push an `ulong`.
They call `__crunch_strtoul` in `libcrunch.a` and use the shared `errno` on
overflow or an invalid base. See the language specification for conversion,
pointer, and error semantics. Earlier IR versions do not recognize this
runtime call; ordinary user-defined or explicitly external functions with
the same name retain normal call linkage.

`call strtol 3` in v2, v5, v6, and v8, and `runtime_call strtol 3` in v3, v4,
v7, and v9 consume the same pointer, pointer, and integer arguments and push a
signed `long`. They call `__crunch_strtol`; both conversion routines share a
private parser and the runtime's `errno`. A user-defined or explicitly
external `strtol` uses ordinary call linkage.

`call strtod 2` and `runtime_call strtod 2` consume two pointers and push a
`real`, using the binary64 System V return register. They are available in
all supported IR versions. `strtof` is analogous but pushes `float` and
requires float-capable IR (v8 and later). Both use the shared runtime `errno`;
source definitions and explicitly external declarations use ordinary linkage.

`malloc`, `free`, and `realloc` are runtime calls declared in `<stdlib.h>`;
they use `ptr` for allocation pointers and `long` for byte counts. The
assembly entry points `_crunch_malloc`, `_crunch_free`, and `_crunch_realloc`
share the public implementation and use the internal C-style mapping calls.
They are separate from the dynamic-array allocation helpers.
`allocator_trim` is a zero-argument runtime call returning `int`; it retries
release of retained unused allocator mappings without affecting live blocks.

`open`, `read`, `write`, `lseek`, `close`, and `stat` are runtime calls in both IR
versions, with argument counts 3, 3, 3, 3, 1, and 2 respectively. Their
results are `int`, `long`, `long`, `long`, `int`, and `int`. Each wrapper calls a
separate internal `_crunch_*` raw Linux syscall routine and converts errors
through the shared `errno` helper. Both wrapper and raw routine symbol names
are reserved in v2, v3, and v4. `stat` requires a writable Linux x86-64 kernel
stat buffer; the source-level contract is described in the language
specification. No new IR format version is required.
`fstat`, `unlink`, `rename`, `mkdir`, and `rmdir` use the same raw-wrapper
convention in both IR versions. Their argument counts are 2, 1, 2, 2, and 1;
all return `int`. `fstat` uses the same stat layout as `stat`.
`openat` (four arguments, `int`), `lstat` (two, `int`), `readlink`
(three, `long`), and `getdents64` (three, `long`) use the same convention.
`dirent64_next` is a four-argument `int` runtime call that validates and decodes
a directory record; its private wrapper symbol is reserved. The public
record and buffer contracts are specified in the language specification.
`fstatat`, `readlinkat`, `unlinkat`, `mkdirat`, `renameat`, and `symlinkat`
use the same reserved raw-wrapper convention. Their argument counts are 4, 4,
3, 3, 4, and 3; `readlinkat` returns `long` and the others return `int`.
`fstatat` invokes Linux x86-64 `newfstatat` and uses the existing stat layout.

Socket, signal, and time runtime calls use the Linux x86-64 ABI:
`socket`, `bind`, `connect`, `accept`, `sendmsg`, `recvmsg`, and `sigaction`
take three arguments; `listen` and `nanosleep` take two; `recvfrom` takes six;
`sigemptyset` and `sleep` take one. `sendmsg`, `recvmsg`, and `recvfrom`
return `long`, `sleep` returns `uint`, and the others return `int`.
`crunch_sig_ign` takes no arguments and returns a handler pointer.
Their source-level layouts and error behavior are in the language specification.

A `stringlit LABEL HEXBYTES` directive declares a NUL-terminated string literal as static read-only bytes (encoded as hexadecimal byte pairs), and `straddr LABEL` pushes its address. The frontend emits these directives for source-level string literal expressions; the backend gathers their storage into `.rodata`. The backend emits a local static-array ownership check for resize operations; allocation and deallocation routines are linked from `libcrunch.a`. Runtime and formatting helpers are provided by selectively linked library members; numeric-formatting entry points are hidden and are called only by library `printf`. These routines use x86-64 instructions and Linux syscalls and do not depend on libc.

`env_get` takes the `envp` vector and a NUL-terminated variable name, returning a pointer to the value after the matching `NAME=` entry or null if no exact name match exists. It does not allocate or modify the environment strings.
