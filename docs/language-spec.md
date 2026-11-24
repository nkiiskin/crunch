# Crunch Language Specification — Initial Draft

## 1. Target

- Target platform: x86-64 Linux.
- Assembly syntax: GNU assembler, AT&T syntax.
- Calling convention: System V AMD64 ABI.
- C interoperability is supported when declared types match the platform ABI.

## 2. Types

| Type | Size | Meaning |
|---|---:|---|
| void | None | No value |
| _Bool | 1 byte | Boolean integer, 0 or 1 |
| short | 2 bytes | Signed 16-bit integer |
| unsigned short | 2 bytes | Unsigned 16-bit integer |
| int | 4 bytes | Signed 32-bit integer, matching Linux x86-64 C `int` |
| long | 8 bytes | Signed 64-bit integer |
| unsigned int | 4 bytes | Unsigned 32-bit integer |
| unsigned long | 8 bytes | Unsigned 64-bit integer |
| long long | 8 bytes | Signed 64-bit integer, distinct from `long` |
| unsigned long long | 8 bytes | Unsigned 64-bit integer, distinct from `unsigned long` |
| char | 1 byte | Unsigned 8-bit integer |
| signed char | 1 byte | Signed 8-bit integer |
| unsigned char | 1 byte | Unsigned 8-bit integer |
| float | 4 bytes | IEEE 754 binary32 |
| double | 8 bytes | IEEE 754 binary64 |
| long double | 16 bytes | x86-64 binary80 |
| T* | 8 bytes | Pointer to T |

Crunch `int` and `long` have fixed widths independent of the host compiler. On the target Linux x86-64 System V ABI, these widths match C `int` (32 bits) and C `long` (64 bits), respectively. Crunch `char` currently retains unsigned 8-bit semantics; this differs from the signed plain `char` used by the usual x86-64 GCC configuration. The former type keywords `byte` and `real` are now ordinary identifiers.

### Numeric rules

The compiler uses C-style numeric promotions and conversions by default.
`--c11-numeric` is accepted for compatibility and has no effect. Plain `char`
remains unsigned but promotes to `int`, character
constants have type `int`, and shifts use the promoted left operand's type.
Arithmetic and conditional expressions use the usual arithmetic conversions.
All numeric types may be converted implicitly in assignments, initializers,
arguments and returns, and explicitly by casts, including `char` and `double`.
Integer narrowing retains low bits. Constant range warnings remain enabled.
This is not full C11 conformance; the enum and other language limitations below
still apply.

Comma-separated variable and structure-field declarations share the base type,
but each declarator has its own pointer modifiers, pointer qualifiers, and array
suffix. For example, `int *p, n, *items[2];` declares a pointer, an integer,
and an array of pointers. A typedef name supplies its entire aliased type as
the base; `typedef int *P; P p, *pp;` declares an `int*` and an `int**`.
File-scope pointer initializers can use null or supported static address
constants, such as `int n = 1, *p = &n;`.

Function pointer typedefs and parenthesized declarators, such as
`typedef int (*Callback)(int);` and `int (*callback)(int);`, are supported
for variables, fields, and parameters. Functions decay to pointers; calls
through pointers check argument and return types and use the System V ABI.
Functions can return function pointers, either through a typedef or with a
declarator such as `int (*choose(int which))(int)`; the result can be called
directly as `choose(0)(42)`.
Function addresses can initialize static pointers. Function pointers are
distinct from object pointers and do not support arithmetic. Prototypes may
omit parameter names; definitions must provide them. Variadic function
pointer signatures are parsed and checked and can call external variadic functions.
Arrays of function pointers are supported, including multidimensional arrays,
typedefs, fields, and parameters, for example `int (*callbacks[2])(int);`.
Nested parenthesized declarators can combine pointers, arrays, and function
signatures, for example `int (*(*factory)(int))(int);`. Function-type aliases
such as `typedef int Unary(int);` can declare functions or pointers to them.
Function parameters declared with a function type are adjusted to pointers.
Taking the address of a compiler-provided runtime function
without an explicit external declaration is not supported.
For functions defined in other modules, prototypes with a basic signature
incompatible with a runtime function require a user definition. Matching
runtime-named declarations also use user-first resolution in module mode.

The frontend recognizes `float`, including decimal and hexadecimal literals
with an `f` or `F` suffix, and checks its conversions and four-byte layout.
Hexadecimal floating literals require a `p` or `P` binary exponent. `float` takes
precedence over integer types in mixed arithmetic; `double` takes precedence
over `float`. Numeric conversions involving `float` are implicit, subject to
constant range checks. Sources using `float` lower to binary32 IR and use
the System V floating-point calling convention.

The frontend supports 16-byte, 16-byte-aligned binary80 objects, arrays and
aggregate members, arithmetic, conversions, comparisons, updates, and
function arguments, returns and variadic retrieval using the x86-64 System V
ABI. Compound assignments to other numeric types accept a `long double`
operand. The runtime does not provide a complete math library.
`<math.h>` provides `isfinite`, `isnan`, `signbit`, `fabsl`, and `truncl`
for binary80 values; the classification functions also accept `float` and
`double` through conversion.

`<float.h>` provides `FLT_MAX` and `DBL_MAX`, the largest finite binary32 and
binary64 values, and the binary80 `LDBL_*` precision, exponent, range and
subnormal limits. Other standard floating-point limits are not yet provided.

`<limits.h>` defines `CHAR_BIT` as 8, `CHAR_MIN` as 0, `CHAR_MAX` as 255,
`SCHAR_MIN` as -128, `SCHAR_MAX` as 127, `UCHAR_MAX` as 255, the Crunch-specific `BYTE_MIN` and `BYTE_MAX` as 0 and
255, `SHRT_MIN`, `SHRT_MAX`, `USHRT_MAX`, and `INT_MIN`, `INT_MAX`, `UINT_MAX`, `LONG_MIN`,
`LONG_MAX`, `ULONG_MAX`, `LLONG_MIN`, `LLONG_MAX`, and `ULLONG_MAX` for the
integer types above. The short limits have type `int`; the larger limits use
their corresponding integer types.

### Character integers

`signed char` and `unsigned char` are distinct from each other and from plain
`char`, including in pointers and function signatures. Their ranges are
-128..127 and 0..255; both promote to `int` for arithmetic and accept numeric
conversions. String literals may initialize arrays of any of the three
character types. Plain `char` retains its existing unsigned semantics.

### Boolean integers

`_Bool` is a distinct one-byte integer type that promotes to `int`. Conversion
from a numeric or pointer scalar produces 0 for zero and 1 otherwise, including
for NaN and infinity. Its pointers and function signatures remain distinct from
character types. `<stdbool.h>` provides `bool`, `true`, `false`, and
`__bool_true_false_are_defined`.

### Enumerations

Tagged and anonymous enum definitions may appear as declarations, in typedefs,
structure fields, parameter declarations, casts, and `sizeof` type names.
Tags and enumerators obey lexical scope;
enumerators have type `int`, are not modifiable, and may initialize later
enumerators. Explicit values must be integer constant expressions in the
signed 32-bit `int` range; omitted values start at zero and increment by one.
Enum types are distinct and use four-byte storage. An enum with any negative
enumerator is compatible with `int`; otherwise it is compatible with
`unsigned int`, matching the default GCC choice on this target. This affects
integer promotions, arithmetic, conversions, and compatible pointer and function
types. Distinct enum types are not compatible with each other. Enumerators
themselves remain `int` constants and may be used in array bounds, switch
labels, and static initializers.

An enum tag reference must follow a visible definition; incomplete enums are
not supported. Enum definitions in `for` initialization declarations are
rejected. Structure support retains other limitations and this is not full C11
conformance.

### Short integers

`short` and `unsigned short` are distinct 16-bit types with two-byte alignment,
matching the Linux x86-64 C ABI. `short int` and `unsigned short int` are
equivalent spellings. Their ranges are -32768 through 32767 and 0 through
65535. Signedness, `short`, and optional `int` may appear in any order.
Both promote to `int` in unary arithmetic, bitwise operations, binary
arithmetic, comparisons, shifts, and numeric conditional expressions. For
shifts, the result type is the promoted left operand's type. Switch controlling
expressions are promoted to `int`. Assignments,
initializers, arguments, returns, and casts involving a short type allow
numeric conversions; integer narrowing retains the destination's low bits.
Out-of-range constant conversions produce warnings as described below, except
that integer conversions involving unsigned types follow modulo conversion.
Pointers, arrays, structure fields, and function signatures retain the
distinction between the two types. The keyword `short` is reserved.

### Integer literals and bitwise operations

Hexadecimal integer literals use the prefix `0x` or `0X`, for example `0xfe`
or `0XFF`. They can be used wherever integer expressions are accepted,
including as function arguments. Unsuffixed decimal literals use `int` when
they fit, otherwise `long`; unsuffixed hexadecimal and octal literals also
consider `unsigned int` and `unsigned long` in the C candidate order. A leading
minus is a unary operator; the signed 64-bit minimum can be written as
`(-9223372036854775807 - 1)`. A decimal literal without an unsigned suffix
outside the signed 64-bit range is rejected, even when preceded by unary minus.
Digit separators are not supported. Unsigned literals and scalar operations
are described below.

Integer types support bitwise `&`, `|`, `^`, and `~`, shifts `<<` and `>>`,
and compound assignments such as `x <<= 5` and `x ^= 0xf0`. Right shifts
of signed integers are arithmetic; `char` right shifts are logical. `^` is
bitwise XOR, while exponentiation uses `**`.

The comma operator evaluates its left operand before its right operand and
produces the right operand's value and type. It binds less tightly than
assignment; commas separating arguments and declarators are not operators.
Comma expressions are not integer constant expressions.

### Unsigned integers

The compiler supports unsigned scalar types, literals, arithmetic, conversions,
constant global initializers, pointers, fixed arrays, and function parameters
and returns.

The types are `unsigned int` (32 bits, range 0 through 4294967295)
and `unsigned long` (64 bits, range 0 through 18446744073709551615). Their
sizes and alignments match the corresponding signed types and the Linux
x86-64 C ABI. They are distinct types for declarations, pointer compatibility,
function signatures, arrays, structure fields, and external linkage.
`signed` and `unsigned` are keywords; existing programs using them as identifiers
must be updated. Either can stand alone as shorthand for `signed int` or
`unsigned int`. GNU spellings `__signed` and `__signed__` are also accepted as
`signed`. Signedness, `long`, and optional `int` may appear in any order;
`long long` and `unsigned long long` have the same 64-bit range and layout as
`long` and `unsigned long`, respectively, but are distinct types with higher
rank. The words in `signed char` and `unsigned char` may likewise occur in
either order.

Integer literals may have a case-insensitive `U`, `L`, or `UL` suffix; `LU`
is also accepted, in either case. A `U` literal has type `unsigned int` if its
value fits, otherwise `unsigned long` if it fits. A `UL` or `LU` literal has
type `unsigned long`. A value larger than the unsigned 64-bit maximum is
rejected. An `L` literal is `long` when representable, or `unsigned long` for
an octal or hexadecimal literal outside the signed range. `LL` selects
`long long`, or `unsigned long long` for an octal or hexadecimal literal
outside the signed range; `ULL` and `LLU` select `unsigned long long`. The two
`L` letters must have the same case. Octal literals with a suffix use base eight. For example:

```c
unsigned int count = 0xffffU;
unsigned long mask = 0xffffffffffffffffUL;
```

For `+`, `-`, `*`, `/`, `%`, binary bitwise operations, comparisons, and
numeric conditional branches involving either new unsigned type, use C-style
usual arithmetic conversions. In particular:

| Operands | Common type |
|---|---|
| `int` and `unsigned int` | `unsigned int` |
| `long` and `unsigned int` | `long` |
| `int` or `long` and `unsigned long` | `unsigned long` |
| `unsigned int` and `unsigned long` | `unsigned long` |
| `long long` and `unsigned long` | `unsigned long long` |
| `long long` and `unsigned int` | `long long` |
| any integer and `unsigned long long` | `unsigned long long` |
| `char` and `unsigned int` or `unsigned long` | that unsigned type |
| `signed char`, `unsigned char`, `short`, or `unsigned short` and an integer type | apply `int` promotion first |
| integer and `double` | `double` |

Same-type `unsigned int` and `unsigned long` operands retain their type. Signed operands converted to an unsigned
common type are reduced modulo 2 to the power of that type's width. Thus
`-1 < 1U` is false, while `(long)-1 < 1U` is true. Operations whose operands do
not include a new unsigned type retain existing Crunch rules by default; in particular
this extension must not change existing `char`, `int`, or `long` expressions.
Exponentiation still converts both operands to `double` and produces `double`.
Comparisons still produce `int`. Logical operators and conditions test zero
versus nonzero and retain short-circuit evaluation.

For `unsigned int` and `unsigned long`, addition, subtraction, multiplication,
unary minus, and left shifts
wrap modulo 2 to the power of the result width. Unsigned division, remainder,
comparisons, and right shifts use unsigned interpretation; right shifts fill
with zero. Division or remainder by zero is invalid. For a shift involving a
new unsigned type, C integer promotions apply separately to the operands and
the result type is the promoted left operand's type. A negative shift count or
one at least as large as that type's width is invalid; known constant cases are
rejected and no runtime result is guaranteed for invalid counts. Existing
shifts without the new types are unchanged.

Assignments, initializers, arguments, and returns also allow C-style implicit
integer conversions when the source or destination is a new unsigned type;
explicit casts use the same integer conversion. Conversion to an unsigned type
reduces the value modulo 2 to the power of the destination width. Conversion to
a signed type preserves a representable value; otherwise Crunch defines the
result by retaining the destination-width low bits and interpreting them as a
two's-complement signed value. This defines the implementation-dependent C
case for Crunch's target. These rules apply equally to constants and runtime
values. For example, `unsigned int x = 1;` is valid, `unsigned int y = -1;`
produces 4294967295, and converting `0xffffffffU` to `int` produces -1.
Conversions not involving a new unsigned type retain existing Crunch rules.

Implicit conversion from `unsigned int` or `unsigned long` to `double` is allowed and
may round to binary64. Conversion from `double` to an unsigned type requires an
explicit cast. Out-of-range or non-finite constant conversions produce warnings;
runtime conversion behavior is specified under
[Numeric casts and constant ranges](#numeric-casts-and-constant-ranges).
Literal range checks and destination-specific checks such as array bounds still
apply after integer conversion.

Unsigned values are preserved in global initializers and across the System V
AMD64 ABI, including separately compiled Crunch and C objects.
`printf`, `fprintf`, `sprintf`, and `snprintf` support `%u` for `unsigned int` and
`%lu` and `%zu` for `unsigned long` / `size_t`, and lowercase hexadecimal `%x` and `%lx` for the
same respective types, as well as `%lld`, `%llu`, and `%llx` for long long
arguments, with corresponding runtime type tags.
Existing signed formats and tags remain unchanged. APIs with signed count or
offset parameters retain those parameter types and use the conversions above.
AST v15 represents the new types, suffixes, and full-width literal values.
IR v6 and v7 represent unsigned types and operations without reinterpreting
existing IR v2 through v5.

### Size of an object

`sizeof(type)` and `sizeof expression` produce a compile-time `size_t` byte
count. On the supported target, `size_t` is an alias for `unsigned long`,
as declared in `<stddef.h>`. The operand is type-checked but not evaluated, so side effects do not
occur. `char` is 1 byte, `int` and `float` are 4, and `long`, `double`, and pointers are 8.
Structure sizes include field padding and alignment. Fixed arrays have their complete element storage
size, including in structure fields. For a string literal, `sizeof` counts its
decoded bytes including the terminating null; elsewhere string literals currently
have `char*` type. `sizeof` rejects void and incomplete or unsupported object
types. It is an integer
constant expression and may be used in switch labels and supported global
integer initializers. The identifier `sizeof` is reserved.

### Member offsets

`__builtin_offsetof(type, member)` yields an `unsigned long` byte offset in
a complete structure or union, including inline aggregate definitions.
Designators may continue through `.member` and `[index]` and may name members
promoted by anonymous aggregates. Array indices must be integers; nonconstant
indices are evaluated at runtime. The result is an integer constant expression
when all indices are integer constant expressions. Bit-fields, missing members,
non-array indexing, and incomplete aggregate types are rejected.
`<stddef.h>` defines `offsetof(type, member)` using this builtin.

### Alignment of an object type

`_Alignof(type)` is an integer constant expression of type `size_t` giving
the type's alignment in bytes. Its operand must be a complete object type;
expressions, function types, `void`, and incomplete types are rejected.
Array types have their element type's alignment. Layout follows the supported
Linux x86-64 ABI, including structures, unions, and `long double`.
`<stdalign.h>` defines `alignof` as `_Alignof`, `alignas` as `_Alignas`,
and supplies the C11 feature macros.

### Alignment specifiers

`_Alignas(integer_constant_expression)` and `_Alignas(type)` specify the
alignment of an object or structure or union member. Supported alignments
are 1, 2, 4, 8, and 16 bytes; zero has no effect. Multiple specifiers use
the strictest nonzero alignment and cannot weaken the type's natural
alignment. Requests above 16 bytes are rejected. Alignment specifiers cannot
apply to typedefs, functions, parameters, register objects, or bit-fields.
Aligned members affect aggregate layout, size, and calling conventions.

### Thread-local storage

`_Thread_local` is supported for file-scope `static` scalar objects, including
pointers. Each thread receives its own object and constant initial value;
taking its address yields the calling thread's instance. All declarations of
an object must agree on thread-local storage. Block-scope, external-linkage,
array, aggregate, and `long double` thread-local objects are not supported.
Compile and link with `--glibc`; Crunch's own executable runtime does not
initialize TLS. This is a restricted subset of C11 thread-local storage.

### Exponentiation

`base**exponent` raises one numeric value to the power of another. Both operands
are converted to `double`, and the result is always `double`; integer destinations
therefore require an explicit cast. Operands are evaluated left to right.
Exponentiation is right-associative and binds more tightly than multiplication
and unary plus or minus: `2**3**2` is `2**(3**2)`, `-2**2` is `-(2**2)`,
and `2**-2` is `0.25`. Parentheses change the grouping, as in `(-2)**2`.

The standalone runtime computes powers in floating point. A negative finite
base with a non-integer exponent produces NaN; zero to a negative exponent and
overflow produce infinity, while underflow can produce a signed zero.
An odd integer exponent preserves a negative base's sign, including for zero
and infinity. As usual, any value to zero and one to any value produce one.
Results are approximations, not guaranteed correctly rounded. The operator does
not set `errno`.

Adjacent stars in an infix expression form `**`. Write multiplication by a
dereferenced pointer with separation, such as `a * *p`; `a**p` instead means
power and requires a numeric right operand. Pointer declarations such as
`int** p` and prefix dereferences such as `**p` retain their existing syntax
and backend restrictions. The preprocessor must leave the two power stars
adjacent.

At file scope, `char`, `int`, and `long` initializers may use integer constant
expressions, including arithmetic, bitwise operations, comparisons, logical
operators, conditionals, integer casts, and `sizeof`. They are evaluated into
static data in both executable and module mode; the usual type and constant
range checks still apply. Comparisons may also use supported real constant
expressions; they yield `int` values and follow runtime binary64 comparison
rules, including for NaNs and signed zero. Integer operations use their Crunch result widths.
Division by zero, signed division or remainder overflow, and shift counts
outside the operand width are rejected when evaluated. Logical and conditional
operators short-circuit, but every branch must itself be a constant expression.
Variables and function calls are not permitted, except inside an unevaluated
`sizeof` operand.
Semantic constant evaluation also short-circuits
supported logical and conditional expressions used as array bounds and case
labels. Statically known invalid shifts and integer divisors in an unchosen
branch or an unevaluated `sizeof` operand are not diagnosed.

File-scope `double` initializers also accept integer constant expressions, which
are evaluated at their integer width and converted to binary64, implicitly or
with an explicit `double` cast. Real literals, unary signs, powers, and `+`, `-`,
`*`, and `/` can be combined in constant expressions. Each real operation
uses binary64, including its rounding, signed zeros, infinities, and NaNs;
integer subexpressions retain their integer rules. Real comparisons may be used
in integer subexpressions and widened to `double`. Numeric conditional expressions
may select real or integer branches using integer or real conditions; the chosen
branch is converted to the conditional's result type. Only the chosen branch is
evaluated, but both must be constant expressions. General runtime expressions
and other real-valued operators in global initializers remain unsupported.

Uninitialized pointer variables and pointer fields are initialized to null by the compiler. This applies to supported pointer types, including pointers to pointers, in local and global variables and in local or global structures, including nested structures. An explicit initializer is preserved. Function parameters are initialized from their arguments, not reset to null.

### Type aliases

The reserved word `typedef` introduces a type alias:

```c
typedef int Count, *CountPtr;
struct Point { Count x; Count y; };
typedef struct Point Point;
typedef struct Pair { double x, y; } Pair, *PairPtr;
typedef struct { double x, y; } Coordinate;
Count twice(Count value) { return value * 2; }
```

Aliases may name scalar, pointer, named-structure, or unnamed-structure types,
including other aliases and `void`. They introduce no storage and may be used wherever
the underlying type is accepted, including declarations, parameters, return
types, fields, casts, and array element types. Existing backend
restrictions on those types still apply. In a comma-separated `typedef`, pointer
modifiers apply to their individual declarator.

An alias may name an incomplete structure or union, such as
`typedef struct Model Model;`. Pointers to that type may be declared and
passed without exposing its definition; objects and pointer arithmetic require
a complete type.

A standalone `struct Model;` or `union Model;` declares an incomplete tag.
At block scope it declares a tag in that scope even if an outer tag has the
same name; a later definition in the same scope completes it. External objects
may be declared with incomplete structure or union type without defining the
object. A later definition does not make earlier uses complete. A file-scope
tentative definition with external linkage may be completed later in the same
translation unit.

An alias is visible after its declaration within its file or block scope.
Aliases share the ordinary identifier namespace with variables and functions;
an inner declaration may shadow an outer alias. Repeating an alias in the same
scope is allowed only for an identical qualified type. `const` applied to an
alias qualifies the aliased type itself, so `const CountPtr` is a const pointer
to `int`. Use shared headers for aliases needed by multiple source files.
At file scope, a named structure may be defined inline in a `typedef`, as in the
`Pair` example; both the structure tag and aliases remain available afterward.
An unnamed structure definition has a distinct type for each definition, even
when its fields match another definition. Unnamed structures may be used in
`typedef` declarations or variable declarations at file or block scope; aliases
follow ordinary lexical scoping. Structure and union definitions can also
appear directly in type-name operands of `sizeof` and `_Alignof`. For example:

```c
typedef struct { int x, y; } Point;
int main() {
    Point p;
    p.x = 20;
    p.y = 22;
    return p.x + p.y;
}
```

Separately defining another unnamed structure with the same fields does not
make it assignment-compatible with `Point`. Array aliases such as
`typedef int Values[2][3];` support positive integer constant bounds. The
outermost bound may be omitted, as in `typedef int Values[];`.
Array alias parameters are adjusted to pointers, including in function-pointer
signatures. Parenthesized pointer-to-array declarators such as `int (*p)[3]`
are supported for variables, fields, aliases, and parameters, including in
function-pointer signatures; abstract forms such as `int (*)[3]` are supported
in casts and `sizeof`. Array bounds must be positive integer constants, and
pointer arithmetic and indexing require a complete pointed-to array type.
Functions may return a pointer to an array, for example `int (*get(void))[3]`.
Functions cannot return arrays or functions, and arrays cannot contain functions.
Initializers on `typedef` declarations are not supported. The former keyword
`def` is an ordinary identifier.

### File-scope static declarations

`static` before the type of a file-scope function or variable gives it internal
linkage: its symbol is private to the resulting object. A function first
declared `static` keeps internal linkage on later compatible declarations,
including its definition. A `static` declaration after an external declaration
of the same function is rejected. `main` cannot be `static`. Block-scope
static scalars, fixed arrays, and structures are supported as described below;
static structure fields are not supported.

Block-scope static numeric and pointer variables have persistent storage,
zero-initialized if no initializer is given. Supported initializers are numeric
constants or, for pointers, null and static address constants, including
addresses of previously declared static locals. Initialization occurs once,
not on each entry to the block. Each declaration has distinct storage, even
when names are shadowed or declarations occur in different translation units.
The name retains block scope and has no external linkage.

Block-scope static structures likewise have persistent, initially zeroed
storage. Their positional or designated list initializers must contain
supported numeric or static address constants, including addresses of their
own fields. Initialization does not repeat on later entries to the block.

File-scope variables can also be grouped:

```c
static {
    int a, other;
    char b[2];
    struct point points[20];
};
```

Each member has internal linkage, as if declared individually with `static`.
The braces do not introduce a scope, and the closing brace requires a
semicolon. Groups contain only variable declarations, not functions, type
definitions, or `extern` declarations. Comma-separated declarations and nested
`static` and `const` groups are supported. Block-scope static groups remain
unsupported.

The file-scope shorthand `static const { ... };` applies both qualifiers to
every member, just like `static { const { ... }; };`:

```c
static const {
    int a = 1, b = 2;
    char text[] = "hello";
};
```

The usual group rules apply. In particular, `int *p` becomes a pointer to
`const int`, not a const pointer, and array elements become const.

### File-scope extern functions

File-scope object and function names become visible at their declarations;
references before a declaration are rejected. A function definition is visible
inside its own body, so direct recursion is permitted. An array's type is
complete at a use only if a bound is visible there.

When no definition is present in the translation unit, `extern` before a
file-scope function declaration selects an external function, for example
`extern int calculate(int value);`.
It also disambiguates user functions whose names match Crunch library functions.
Compatible prototypes may be repeated; conflicting prototypes or definitions
are rejected.

An identifier-list function definition is supported when every parameter's
adjusted type is a pointer, `int`, `unsigned int`, `long`, `unsigned long`,
`long long`, `unsigned long long`, `double`, or `long double`. Parameter
declarations may appear in any order and omitted declarations default to
`int`. The definition is normalized to a prototype with those parameter
types. Types changed by default argument promotions, including `float`, `char`,
short integer types, `_Bool`, and enums, are rejected; old-style declarations
without a prototype are not modeled. Parenthesized parameter declarators and
inline enum definitions in the declaration list are unsupported.

A `static` function prototype must have a definition in the same translation
unit. An unresolved `extern` function requires module mode, used by
`crunch-ir --module` and by `crunch` for `-c` and multi-input builds. Module
assembly reserves `_start` and identifiers beginning with `__crunch_` for
compiler and runtime use.

### File-scope extern variables

`extern int count;` declares storage without allocating it, while
`extern int count = 42;` defines and initializes it.
Scalar, supported pointer, structure, union, and fixed-array types are allowed.
For example, `extern struct Point origin;` declares an object defined in
another translation unit; its type must be compatible with that definition.
`extern char message[];` declares an array defined elsewhere. An external array may
have a compatible constant bound or an incomplete type. Compatible file-scope
declarations without `extern` or an initializer are tentative definitions:
they may be repeated and produce one zero-initialized definition if no
initialized definition is present. An incomplete tentative array is completed
to one element if no other declaration supplies a bound.
Declarations must have compatible types and qualifiers, and at most one may
have an initializer. A later `extern` declaration inherits a visible `static`
declaration's internal linkage; conflicting linkage is rejected. Share declarations in headers:
the linker cannot verify type or structure-layout compatibility between
translation units.

### Block-scope external declarations

An `extern` object declaration inside a block declares linked storage without
allocating it. A block-scope function prototype also declares a linked function,
with or without `extern`. Their names are visible only from the declaration to
the end of the block, and can hide an outer automatic variable. Compatible
declarations may be repeated; conflicting declarations and object
initializers are rejected. A visible file-scope `static` declaration supplies
internal linkage. Otherwise, definitions can be supplied by another translation
unit, subject to the same module-mode and type-compatibility requirements as
file-scope external declarations.

### Const qualifiers

Declarations may be grouped as `const { ... };` at global scope or inside a
block. Each variable declaration is treated as if prefixed by `const`:

```c
const {
    double pi = 3.14;
    int num = 26;
    char ver[] = "0.77 beta";
};
```

The braces do not introduce a scope: names remain visible in the surrounding
scope and duplicate names are checked there. Comma-separated declarations and
nested const groups are supported. Only variable declarations are allowed, and
the closing brace must be followed by a semicolon. Qualifiers have the same
meaning as a leading `const`: `int *p` in a group becomes `const int *p`, and
array elements become const.

`const` qualifies a type without changing its size or ABI. It may precede or
follow a base type, or follow each `*`:

```c
const int limit = 10;         // Same type as int const.
int value = 3;
const int *view = &value;     // Pointer may change; *view cannot be written.
int *const fixed = &value;   // Pointer cannot change; *fixed may be written.
const int *const both = &value;
```

Const objects may be initialized, but assignments, compound assignments and
increment/decrement cannot modify them. Qualifiers apply to local and global
variables, parameters, and structure fields. Accessing a member of a const
structure makes that member const, including nested by-value members. It does
not make memory reached through a pointer field const. Whole-structure
assignment is rejected if the destination contains a const by-value member.
Initialization from another structure remains allowed.

Implicit pointer conversions may add const to the immediately pointed-to type,
but cannot discard it. This applies to initializers, assignments, arguments and
returns. Deeper qualifiers must match: converting `int**` to `const int**` is
rejected because it would permit modifying a const object through another alias.
Conditional pointer expressions preserve const from either branch. Top-level
parameter qualifiers constrain the function body but do not distinguish function
signatures; pointed-to qualifiers do.

`const int values[] = {1, 2}` initializes const array elements; later writes
are rejected. The `copy` and `fill` built-ins reject destinations pointing to
const memory. Read-only library inputs accept const pointers.

Like C, const does not require an initializer or make a variable an integer
constant expression. Existing uninitialized-value and global-initializer rules
still apply. Explicit casts between object-pointer types may change the pointed-to type or remove
qualifiers, but modifying an originally const object through such a cast is
unsupported. Const is enforced by semantic analysis, not by read-only storage
or runtime protection. Existing pointer/backend limitations remain unchanged.
String literals retain their existing byte-pointer type and read-only storage.

### Volatile, register, and auto

`restrict`, `__restrict`, and `__restrict__` are accepted as type qualifiers
but are not retained or used for alias optimization; their placement
constraints are not checked.

The outermost brackets of a directly declared array parameter may contain
`const`, `volatile`, and `restrict`, with or without a bound. After array
parameter adjustment, these qualify the pointer, not its element type.
Bracket qualifiers on parenthesized declarators, `static` array-parameter
bounds, and `[*]` prototype bounds are not supported.

`volatile` qualifies an object or pointed-to type. It propagates through
by-value structure members and array elements, but not through pointer members.
Implicit pointer conversions cannot discard pointed-to `volatile` and deeper
qualifiers must match, as with `const`. Conditional pointer expressions retain
the qualifiers of both branches. Explicit pointer casts can remove qualifiers.

Evaluated accesses through volatile lvalues are retained, including discarded
reads and read-modify-write operations. `sizeof` and taking an address do not
read the object. A discarded volatile structure or union is read byte by byte,
including padding; aggregate copies may also use byte accesses. Access widths
are implementation-dependent: automatic scalar slots use eight-byte loads and
stores, and `long double` copies use separate eight- and two-byte accesses.
Thus volatile does not guarantee a single naturally sized hardware access and
is not sufficient for devices requiring specific access widths. Volatile is
not atomic and provides no inter-thread synchronization or memory ordering.

`register` is accepted but ignored: it is not retained in the AST, requests no
particular register allocation, and does not restrict taking an address.
Other existing declaration syntax restrictions still apply.

`auto` explicitly specifies the default automatic storage for a block-scope
object. It cannot be used for functions, parameters, file-scope objects, or
alongside another storage-class specifier.

### Runtime assertions

`#include <assert.h>` defines `assert(expression)`. When `NDEBUG` is not
defined, the scalar expression is evaluated once; if false, the diagnostic on
standard error includes the source file, line, function, and unexpanded
expression, then the process terminates with `SIGABRT`. If `NDEBUG` is defined
when the header is included, the expression is not evaluated. Re-including
the header after defining or undefining `NDEBUG` updates the macro.

On Linux, failure unblocks `SIGABRT`, sends it to the process, and, if a handler
returns or the signal is ignored, restores the default disposition and sends
it again. If signaling fails, it exits with status 134. Process-directed
signaling does not guarantee delivery to the asserting thread in a multithreaded
program.

### Function names

Each function supports up to 1024 local IR slots and 1024 simultaneously
visible local bindings. Parameters and compiler-generated temporaries also
use slots; some declarations require more than one slot or binding.

Each function body has an implicit `__func__` object of type
`static const char[]`, initialized to the function's name. It has block
scope, can be shadowed in a nested block, and retains its array size for
`sizeof`. Outside a function, `__func__` is not implicitly declared.
`__FILE__`, `__LINE__`, `__DATE__`, and `__TIME__` are predefined by the preprocessor; see
[preprocessor configuration](configure.md).

### Static assertions

`_Static_assert(integer_constant_expression, "message");` is a declaration
accepted at file scope, in blocks, and among structure or union members. The
condition must be a supported integer constant expression and must evaluate
nonzero; otherwise compilation fails, with the message included for a false
condition. The message may consist of adjacent string literals, or may be
omitted as in C23's `_Static_assert(integer_constant_expression);`.
Assertions do not create storage or structure members. Their expressions use
the declarations visible at their location.

## 3. Arrays

### Fixed arrays

`int values[4];` declares a fixed array. The bound must be a positive
integer constant expression with a supported object size. An omitted bound with an initializer, as in
`int values[] = {1, 2};` or `char text[] = "abc";`, is inferred from the
initializer. Lists may not have more elements than the bound. A char array
may omit the string terminator when its bound equals the decoded string length.
Sized fixed-array fields are supported in structure layout. In expression
contexts fixed arrays convert to pointers to their elements, except for
`sizeof` and address-of; fixed arrays are not assignable or resizable with
`dim`. Multidimensional fixed arrays such as `int matrix[2][3]` use
row-major layout. Only the outermost bound may be omitted and inferred.
Array parameters such as `int matrix[][3]` adjust to pointers to their rows;
specified bounds must be positive integer constant expressions. Pointers to
array types can also be declared using an array typedef. `sizeof` accepts
fixed-array type names as well as array expressions. Element conversions and
global initializer checking still follow Crunch's existing rules rather than
full C conformance.
For compatibility, parameter declarators with only empty brackets, such as
`char argv[][]`, retain Crunch's historical pointer-to-pointer interpretation.

Automatic local fixed arrays have stack storage and support indexing, decay,
address-of, and list or string initialization. Omitted elements of an
initialized array are zeroed; an array without an initializer is not zeroed.
Global fixed arrays have static storage and support constant numeric, char
string, pointer-to-static-object, and structure-list initializers; unspecified
elements are zeroed.
Pointer initializers support null, string literals, static object addresses,
and constant offsets from supported addresses. Fixed-array structure fields
support indexing, decay, address-of, and `sizeof`; structure assignment copies
their contents. Static local fixed arrays have persistent, zero-initialized
storage and may use the same constant numeric, char string, and static-address
initializers as global fixed arrays. Each declaration has distinct storage,
including in different scopes and translation units; initialization does not
repeat on later calls. Automatic, static local, and file-scope structures and arrays
of structures support brace initializers, including brace elision
for nested structures and array fields. Array and structure initializer lists
also support chained field and constant array-index designators, mixed with
positional entries; later entries override earlier entries and omitted
subobjects are zero-initialized. An omitted array bound is inferred from the
largest initialized index. Character array fields may use a
string literal. Omitted fields and elements are zero-initialized, and omitted
structure-array bounds are inferred from the initialized elements. Automatic
initializers may copy supported structure values; file-scope and static-local
aggregate initializers must use supported numeric or static address constants.
An uninitialized file-scope array with no bound has an incomplete type until
completed by a compatible declaration; a tentative definition that remains
incomplete is completed with one element. `extern int values[]` declares an
incomplete external array. An automatic array requires a bound or initializer.
An incomplete array may be the last member of a structure with at least one
other named member, for example `struct Packet { int length; char data[]; };`.
The element type must be complete. Its offset respects element alignment, but
the member contributes no elements to `sizeof(struct Packet)` and may begin
within trailing padding. Access requires an object with sufficient extra
storage; `sizeof` cannot be applied to the flexible member itself. Initializers
and structure assignment affect only the fixed members. A union may contain such
a structure, but a structure member or an array element may not contain a
flexible member, including through a union. Existing aggregate-by-value backend
limitations still apply.

GNU zero-length array members are supported only as the final member of a
structure with another named member, with a complete element type and an
integer constant zero bound. Unlike a flexible member, the zero-length array
has a complete type and `sizeof(member)` is zero. Its element alignment affects
the member offset and structure alignment; indexing requires separately
allocated storage. Initializers cannot initialize the zero-length member.
Structures containing one may themselves be nested or used in arrays. Other
zero-length array declarations and non-final or union members are rejected.

### Automatic variable-length arrays and pointers

An automatic local array can have runtime bounds, such as `int a[n][m];`.
Each bound is evaluated once when execution reaches the declaration, and must
be positive. Its elements have stack storage until the array's scope is
exited; `break`, `continue`, `goto`, and return release exited arrays.
In a function using `__builtin_alloca`, VLA storage is instead retained
until the function returns.
Automatic local pointers directly to VLAs, such as `int (*p)[n] = a;`, evaluate and
save their bounds at the pointer declaration. Indexing and pointer arithmetic
use the saved runtime row strides. `sizeof` uses the saved size for a VLA
or VLA row, evaluating its operand when the operand has VLA type. Variably
modified typedefs, VLA type names and static pointers to VLAs are not yet
supported. A VLA cannot have an initializer,
static storage, or linkage. Jumps into the scope of a VLA or pointer to VLA,
including a switch dispatch, are rejected.
Stack space limits the maximum allocation.

Function parameters may use runtime array bounds referring to earlier
parameters, including multidimensional arrays and direct pointers to VLAs.
Array parameters are adjusted to pointers; their bounds are evaluated on
function entry and row sizes are saved for indexing, pointer arithmetic and
`sizeof` of VLA rows. `sizeof` of the parameter itself is the pointer size.

### Legacy IR compatibility

Resizable source-level arrays and `dim` are not supported; `dim` is an ordinary
identifier. Previously emitted dynamic-array IR remains accepted by the backend.
For explicit dynamic storage in source programs, use `malloc`, `realloc`, and
`free` from `<stdlib.h>`. See [IR compatibility](ir.md) for the backend contract.

## 6. String and character literals

Character literals represent one byte and have type `int`.

String literals are null-terminated byte sequences. As
expressions, they are immutable and stored in read-only
compiler-managed storage.
Adjacent string literals are concatenated into one string literal.

Supported escapes are:

    \a  \b  \e  \E  \f  \n  \r  \t  \v  \\  \'  \"  \?  \xH...  \ooo

The GNU extensions `\e` and `\E` represent the escape byte (27).
Octal escapes contain one to three octal digits, with a value at most `\377`.
Hexadecimal escapes consume one or more hexadecimal digits, up to the end of
the current literal or the first non-hexadecimal character, and must fit in
one byte (at most `0xff`). Leading zeroes are allowed.

Wide character literals `L'…'` have type `int`, matching Linux x86-64
`wchar_t`. They contain exactly one ASCII character, UTF-8 encoded Unicode
scalar value, or escape. Simple and octal escapes have the same meanings as
above, except octal values through `\777` are accepted. `\x` takes one or more
hex digits and must fit in 32 bits; its bit pattern is interpreted as signed
32-bit. Universal escapes `\u` and `\U` require exactly four and eight hex
digits respectively, and must denote a Unicode scalar; C's restriction on
values below U+00A0 applies (except U+0024, U+0040, and U+0060). Invalid
UTF-8, surrogate values, out-of-range escapes, and multi-character wide
literals are rejected. Wide character literals in preprocessor `#if`
expressions are not supported.

Wide strings `L"..."` contain 32-bit `int` elements and a terminating zero.
They use the wide character encoding and escape rules above. Adjacent narrow
and wide literals are concatenated as wide strings, preserving escape
boundaries; narrow segments use the ordinary string escape rules. A wide
string can initialize an `int` array, including inferred bounds and nested
aggregate initializers. String literal storage is read-only and aligned for
its element type. Taking the address of the whole literal is not yet supported.

A fixed `char` array may be initialized from a string literal. It receives
independent writable storage, including a terminating zero unless the bound
exactly matches the string length.

Block-scope structure and union compound literals, such as
`(struct Pair){1, 2}`, create writable temporary objects. Their initializers
follow the same positional and designated rules as structure declarations.
Block-scope array compound literals, such as `(int[]){1, 2}`, create an array
object with automatic storage lasting until the enclosing block exits. Omitted
outer bounds are inferred from the initializer, and fixed bounds, multidimensional
arrays, character strings, and designated initializers follow array declaration
rules. Arrays decay to pointers except under `sizeof` and address-of. Each
evaluation initializes the same object for that literal during the current
execution of its enclosing block. Block-scope scalar compound literals such
as `(int){42}` and `(int *){p}`
also produce objects with automatic block lifetime. They are lvalues and may
be addressed or modified unless const-qualified. Their initializers are
evaluated each time the literal is reached; they are not integer constant
expressions. File-scope compound literals have static storage duration: each
literal denotes a distinct object that persists for the program's lifetime.
Their initializers must be constant, and their addresses and array-to-pointer
decays may be used in static pointer initializers.

## 6.0 Core language and standard library

Crunch separates the core language from the standard library.

**Core language** (always available, no header needed):

- keywords, operators, types, and control flow;
- the byte-range operations `copy`, `fill`, and `compare`.

**Standard library** (declared by the standard headers; calling one without its
declaration is an error, `call to undefined function`):

| Header | Functions |
|--------|-----------|
| `<string.h>` | `memcpy`, `memmove`, `memset`, `memcmp`, `memchr`, `strcmp`, `strncmp`, `strlen`, `strcpy`, `strcat`, `strchr`, `strrchr`, `strstr`, `strerror`, `strdup`, `strtok`, `strtok_r` |
| `<inttypes.h>` | `strtoimax`, `strtoumax`; `PRId32`, `PRIu32`, `PRIx32`, `PRId64`, `PRIu64`, `PRIx64` |
| `<elf.h>` | Linux x86-64 ELF64 layouts, encodings and constants used by the native tools; not a complete ELF header |
| `<math.h>` | `isfinite` for `float` and `double` |
| `<float.h>` | `FLT_MAX` |
| `<stdarg.h>` | `va_list`, `va_start`, `va_copy`, `va_arg`, `va_end` |
| `<ctype.h>` | Character classification and case conversion |
| `<stdio.h>` | `printf`, `fprintf`, `vfprintf`, `sprintf`, `snprintf`, `vsnprintf`, `scanf`, `fscanf`, `freopen`, `putc`, `perror`, `getline`, `rename`, `renameat`, `input`, and Crunch stream functions |
| `<stdlib.h>` | `exit`, `malloc`, `calloc`, `free`, `realloc`, `allocator_trim`, `atoi`, `atol`, `strtol`, `strtoul`, `strtoll`, `strtoull`, `strtod`, `strtof`, `strtold` |
| `<fcntl.h>` | `open`, `openat`, `fcntl`, `fcntl_ptr` and Linux file flags |
| `<unistd.h>` | Descriptor, process and path operations, and Crunch `file_*` extensions |
| `<sys/stat.h>` | `stat`, `fstat`, `lstat`, `fstatat`, `mkdir`, `mkdirat`, `chmod`, `fchmod`, `S_ISREG`, and Linux `struct stat` |
| `<sys/utsname.h>`, `<sys/wait.h>`, `<sys/resource.h>`, `<sys/random.h>` | `uname`, `waitpid`, `wait4`, `getrlimit`, `getrusage`, wait status macros, and `getrandom` |
| `<poll.h>`, `<sys/select.h>` | `poll` and `select` readiness interfaces |
| `<signal.h>` | `kill`, `sigaction`, `sigemptyset`, signal constants |
| `<sys/socket.h>`, `<sys/uio.h>`, `<sys/un.h>`, `<netinet/in.h>` | Linux socket calls, message vectors, Unix and IPv4 address layouts |
| `<time.h>` | `struct timespec`, `nanosleep` |
| `<sys/mman.h>` | `mmap`, `mprotect`, `munmap`, `mremap`, and Crunch `map_failed` |
| `<dirent.h>`, `<sys/getdents.h>` | `getdents64`, Crunch `dirent64_next`, and legacy `getdents` |
| `<environment.h>` | `env_get` |
| `<errno.h>` | Writable `errno` and Linux x86-64 error constants |
| `<limits.h>` | Integer width and range constants, Linux `PATH_MAX` |
| `<stddef.h>` | `size_t`, `ptrdiff_t`, and `NULL` |
| `<stdalign.h>` | `alignof`, `alignas`, and their feature macros |
| `<stdint.h>` | Fixed-width, least-width, fast-width, pointer-width and maximum-width integer types, limits and constant macros |
| `<sys/types.h>` | `size_t`, `ssize_t`, `pid_t`, `mode_t`, `off_t`, `time_t`, and `suseconds_t` |
| `<sys/ioctl.h>` | `ioctl`, `ioctl_value`, selected Linux request constants, `struct winsize` |
| `<sys/sendfile.h>` | `sendfile` |
| `<sched.h>`, `<sys/time.h>` | `sched_yield`, `gettimeofday`, interval timers |
| `<sys/ipc.h>`, `<sys/shm.h>` | System V shared memory types, flags and operations |
| `<sys/sem.h>`, `<sys/msg.h>` | System V semaphore and message queue types, flags and operations |
| `<sys/syscall_220_259.h>` | Linux x86-64 syscalls 220–259, including raw timer, message queue, epoll, keyring and NUMA interfaces; see their `3crunch` pages for ABI differences |
| `<sys/syscall_260_299.h>` | Linux x86-64 syscalls 260–299, including raw signal-mask and timeout interfaces; see their `3crunch` pages for ABI differences |
| `<sys/file.h>` | `flock` and Linux lock flags |
| `<sys/sysinfo.h>` | Linux `sysinfo` and its result structure |

These are Crunch's supported declarations, not complete ISO C or POSIX
headers. Crunch-specific extensions and Linux-specific signatures remain as
documented below.
`fcntl` and `semctl` use fixed argument lists rather than GNU libc's variadic
interfaces. Use `fcntl_ptr` for pointer arguments to `fcntl`, and
`semctl_value` for integer arguments to `semctl`; pass zero for unused arguments.
`<ctype.h>` provides the standard `isalnum`, `isalpha`, `isblank`, `iscntrl`,
`isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`,
`isxdigit`, `tolower` and `toupper` functions. They use the ASCII C locale;
classification returns zero or nonzero, and case conversion leaves unmatched
characters and EOF unchanged. Arguments must be EOF or representable as
`unsigned char`, as in C. Locale-dependent classification is not supported.

`NULL` is also defined by `<stdio.h>`, `<stdlib.h>`, `<string.h>`,
`<unistd.h>`, `<dirent.h>`, `<sys/types.h>`, `<sys/select.h>`, and `<sys/mman.h>`. It expands
to the integer constant `0`.
`size_t` is also available through `<stdio.h>`, `<stdlib.h>`, `<string.h>`,
`<unistd.h>`, `<dirent.h>`, `<sys/types.h>`, and `<sys/mman.h>`. `ptrdiff_t`
is a signed `long`. Other standard `<stddef.h>` types and macros
are not yet provided.

`<stdint.h>` follows the Linux x86-64 LP64 ABI: 8-, 16-, 32- and 64-bit
integer types use `char`, `short`, `int` and `long` respectively; pointer and
maximum-width types use `long`. The 16-, 32- and 64-bit fast types use
`long`, while the 8-bit fast types use `char`. Constant macros take integer
literal tokens; macro arguments adjacent to `##` are not expanded before
being pasted.

`ssize_t` is a signed `long` alias on this target, declared in `<sys/types.h>`
and also exposed by `<stdio.h>`, `<unistd.h>`, and `<dirent.h>`. These headers do not yet
provide all POSIX types. Use `%ld` to format `ssize_t`; `%zd` is not supported.
On Linux x86-64, `pid_t` is `int`, `mode_t` is `unsigned int`, and `off_t`,
`time_t`, and `suseconds_t` are `long`. `<poll.h>` defines `nfds_t` as
`unsigned long`. `<sys/select.h>` supplies an `fd_set` and `FD_ZERO`,
`FD_SET`, `FD_CLR`, and `FD_ISSET`; descriptors must be between zero and
`FD_SETSIZE - 1`. Passing raw `long*` bitmaps to `select` now requires an
explicit conversion to `fd_set*` and storage large enough for the syscall.

`<stdlib.h>` declares `void* malloc(size_t byte_count)`, `void free(void* address)`,
and `void* realloc(void* address, size_t byte_count)`. Allocations are independent
blocks, aligned to 16 bytes, with unspecified contents. Blocks of up to 256 bytes
share mapped arenas; larger blocks use individual mappings. Empty arenas are
released. The allocator's shared pool state is not thread-safe. Requests too
large for the allocator fail with null and `errno = ENOMEM`. `malloc(0)` attempts
an allocation and returns a freeable pointer on success. Allocation failures
return null and set `errno`; successful operations leave it unchanged.

`free(0)` does nothing. `realloc(0, size)` behaves like `malloc(size)`;
`realloc(pointer, 0)` releases the allocation and returns null. Otherwise,
`realloc` preserves the old contents up to the smaller requested size, possibly
moving the allocation. Failure leaves the original allocation valid. Shrinking
may retain the existing mapping. Only pointers returned by these allocation
functions may be passed to `free` or `realloc`; do not release them with
`munmap` or another allocator. Callers must release allocated blocks.
If an underlying `munmap` fails while releasing a block or empty arena, `free`
and `realloc(pointer, 0)` leave the error in `errno`. A pool arena that cannot
be unmapped remains available for reuse. If releasing the old arena during a
moving `realloc` fails, `realloc` returns null, sets `errno`, and preserves the
original allocation.

`int allocator_trim()` explicitly retries releasing unused mappings retained
after allocator cleanup failures, including failed empty-arena releases.
It never releases live allocations. It attempts all eligible mappings; it
returns zero and leaves `errno` unchanged if all succeed, or returns -1 with
the first unmap error in `errno` if any fail. Successfully unmapped entries
are removed, while failed entries remain available for reuse or a later retry.
Continued unmap failures can still retain memory without a fixed bound.
See [allocator_trim(3crunch)](../man/allocator_trim.3crunch),
[malloc(3crunch)](../man/malloc.3crunch),
[realloc(3crunch)](../man/realloc.3crunch), and
[free(3crunch)](../man/free.3crunch) for the public contracts.

`memcpy`, `memmove`, and `memset` take and return `void*` destinations; `memcpy` and
`memcmp` accept `const void*` source operands. This allows object pointers
without casts while preserving the destination's writable requirement.
`memmove` permits overlapping source and destination ranges. `strlen` returns
`size_t`; `memcpy`, `memmove`, `memset`, `memcmp`, and `strncmp` take
`size_t` byte counts. `strncmp` compares at most `byte_count` bytes. `strcpy` and `strcat` copy through
the terminating NUL and return the destination; the destination must be large
enough and, like `memcpy`, no bounds are checked. `strchr` returns a pointer to
the first occurrence of a byte (the terminator is searchable) or a null pointer;
`strrchr` finds the last occurrence, including the terminator.
`strstr` finds the first occurrence of a substring, returning the original
string for an empty substring, or a null pointer when there is no match.
`strerror` returns a static description for Linux error numbers 0 through 40;
other numbers return a generic message.
`strdup` allocates an independent copy. `strtok` splits a writable string
into nonempty tokens, using shared internal state for subsequent calls; it is
not reentrant or thread-safe. `strtok_r` uses a caller-owned continuation
pointer instead. Both allow the delimiter set to change between calls.
test the result with `if (p)` or `p == 0` / `p != 0`. `atoi` and `atol` skip leading whitespace, accept
an optional sign, and parse decimal digits until the first other character,
returning 0 when there are none. Overflow wraps to the result width: signed
32-bit for `atoi` and signed 64-bit for `atol`.

`unsigned long strtoul(const char* text, char** end, int base)` parses a
non-null NUL-terminated string after skipping ASCII whitespace and an optional
sign. The base is 0 or 2 through 36; digits above 9 use case-insensitive ASCII
letters. Base 0 selects hexadecimal for `0x` or `0X` followed by a hexadecimal
digit, octal for another leading `0`, and decimal otherwise. Base 16 also
accepts that hexadecimal prefix. The result is a 64-bit unsigned value; a
leading minus negates a non-overflowing magnitude modulo 2^64.

If `end` is non-null, it must point to a writable `char*`; the stored pointer
points into the input at the first unconsumed byte, or to the original input if
no digits are converted. Do not write through this pointer when the input is
not writable. No conversion returns zero and leaves `errno` unchanged.
Overflow consumes all valid digits, returns 18446744073709551615, and sets
`errno` to `ERANGE`. An invalid base returns zero, sets `errno` to `EINVAL`,
and stores the original input in `*end` if present. Successful conversions
leave `errno` unchanged.

`long strtol(const char* text, char** end, int base)` accepts the same input,
bases, prefixes, and end-pointer rules as `strtoul`, but returns a signed
64-bit `long`. Positive overflow returns 9223372036854775807 and negative
overflow returns -9223372036854775808; both set `errno` to `ERANGE` and consume
all valid digits. Invalid bases return zero, set `errno` to `EINVAL`, and
store the original input in `*end` if present. Successful conversions and
inputs with no digits leave `errno` unchanged; no conversion returns zero.
An unused `strtoul` declaration in `<stdlib.h>` does not cause a signed-only
program using `strtol` to select unsigned IR.

`strtod(text, end)`, `strtof(text, end)` and `strtold(text, end)` parse ASCII whitespace, an optional
sign, decimal or hexadecimal floating-point numbers, and case-insensitive
`inf`, `infinity`, and `nan` (optionally followed by a parenthesized
alphanumeric/underscore payload). The decimal point is always `.`; the NaN
payload is ignored. A malformed exponent is left unconsumed. If `end` is
non-null, it receives the first unconsumed byte, or the original input when
there is no conversion. Do not write through it if the input is not writable.
No conversion returns positive zero and leaves `errno` unchanged.

The result is rounded directly to binary64, binary32 or binary80, respectively, using
round-to-nearest, ties-to-even regardless of the floating-point environment.
Overflow returns signed infinity and sets `ERANGE`; an inexact result that
rounds to a subnormal or zero also sets `ERANGE`. Exact subnormals, successful
normal results, infinities and NaNs leave `errno` unchanged.

An integer constant expression whose value is zero may initialize or be
assigned to any supported pointer type, passed to pointer parameters, returned
from pointer-returning functions, or used as the null arm of a pointer
conditional expression. An explicit cast of such an expression to a supported
pointer type, such as `(char*)0`, is also supported. These null pointer
constants may be compared with pointers using `==` or `!=`; a byte pointer
compared this way is tested for null rather than compared as a string. Explicit
integer-to-pointer casts preserve the integer's sign- or zero-extended x86-64
bit pattern. Explicit casts between pointer types preserve the address, including
between function-pointer types. Calling a function through an incompatible
function type remains undefined. Comparisons between pointers and nonzero
integers or real values are errors. Pointer parameters to the core
pointer-taking built-ins also accept null; passing a null pointer to an
operation that dereferences it remains invalid at runtime.

`void*` holds an untyped object address and may be used for variables, fields,
parameters, and return values. Object pointers convert implicitly to and from
`void*` in initializers, assignments, arguments, and returns, without discarding
pointee `const`. Explicit casts may remove `const`. Equality between a `void*`
and another object pointer compares addresses; a conditional expression with a
`void*` and an object pointer produces a `void*` with combined pointee
qualification. Dereferencing, indexing, or performing pointer arithmetic on
`void*` is invalid: first convert it to an appropriate typed pointer.
Conversions do not make an invalid address or incompatible memory access safe.
An explicit cast to `void` evaluates its operand and discards its value.

Pointers to pointers can hold the address of a pointer variable, pointer field,
or indexed pointer element. They can be stored, passed, returned, compared,
dereferenced, and indexed. A simple assignment through a dereference or index
updates the pointed-to pointer; the usual const qualification rules apply.
For example:

```c
void insert(struct Node** link, struct Node* node) {
    node->next = *link;
    *link = node;
}
```

The address of an indexed numeric or pointer element can be taken as `&values[index]`,
whether the base is a fixed array or a typed pointer. The base and index
are each evaluated once; the resulting pointer retains the element's `const`
qualification. As with pointer arithmetic, the program must ensure that the
address is within the allocation or one past its end, and must not dereference
the one-past address.

Indexed numeric elements also support compound assignment, such as
`values[index] += amount` and `buffer[index] ^= mask`. The base, index, and
right-hand expression are each evaluated once, in that order. The result is
the updated value converted to the element type; normal numeric and const
rules apply.

Numeric lvalues reached by dereferencing a pointer support the same compound
assignments, for example `*p += 1`. The pointer expression and right-hand
expression are each evaluated once, in that order; the result is the updated
value converted to the pointed-to type. The pointer must be valid and writable.

Global numeric variables support the same compound assignments and conversions
as local numeric variables. The right-hand expression is evaluated once, and
the assignment expression yields the updated value.

Numeric structure fields support the same operations, whether accessed directly
or through a structure pointer or indexed structure. Any pointer or index
expression and the right-hand expression are evaluated once, and the result is
the updated value converted to the field type.

Modifiable numeric globals, pointer dereferences, indexed elements, and
structure fields also support prefix and postfix `++` and `--`. The lvalue
is evaluated once; prefix yields the updated value, while postfix yields the
original value. Const objects cannot be updated.

Pointers to numeric, structure, and pointer elements support addition and subtraction of integer
offsets, scaled by the pointed-to element's size, including structure padding.
Integer plus pointer is also supported. Modifiable pointer variables, pointer
fields, and pointer lvalues reached by dereferencing or indexing support `+=`,
`-=`, and prefix and postfix `++` and `--`. The lvalue address and any offset
expression are each evaluated once. As in C,
postfix yields the old pointer and prefix yields the updated pointer; for
example, `*p++` reads the current element before advancing `p`. The pointer
must remain within its allocation or one past its end. Array objects themselves
are not modifiable pointer lvalues.
Subtracting compatible object pointers yields a `ptrdiff_t` element count;
both pointers must refer to the same array or its one-past position.

`<sys/mman.h>` provides Linux x86-64 memory mapping:

```c
void* mmap(void* address, size_t byte_count, int protection, int flags,
           int descriptor, off_t offset);
int munmap(void* address, size_t byte_count);
void* mremap(void* address, size_t old_size, size_t new_size,
             int flags, void* new_address);
int map_failed(const void* address);
```

`mmap` and `mremap` return a mapping address or the Linux `MAP_FAILED`
sentinel on failure, setting `errno` to the positive error number. Use
`map_failed(result)` to test for that sentinel: byte-pointer equality compares
strings and must not be used on invalid pointers. `map_failed(0)` is false;
a null address is not the failure sentinel. `munmap` returns zero on success or
-1 and sets `errno` on failure. Successful calls leave `errno` unchanged.

The header defines `PROT_NONE`, `PROT_READ`, `PROT_WRITE`, `PROT_EXEC`,
`MAP_SHARED`, `MAP_PRIVATE`, `MAP_FIXED`, `MAP_ANONYMOUS`,
`MREMAP_MAYMOVE`, `MREMAP_FIXED`, and `MREMAP_DONTUNMAP`. Pass a zero
address to let the kernel choose the mapping location; anonymous mappings
normally use descriptor -1 and offset zero. Offsets and fixed addresses must
meet Linux page-alignment requirements. These calls pass their arguments to
Linux; callers are responsible for valid sizes, flags, access permissions and
mapping lifetimes. `MAP_FIXED` and `MREMAP_FIXED` can replace existing
mappings. Release mappings with `munmap`, not `free`.

Unlike the C interface, Crunch `mremap` always takes five arguments.
Pass zero for `new_address` unless using
`MREMAP_FIXED`, which also requires `MREMAP_MAYMOVE`.

`<fcntl.h>` declares `int open(const char* path, int flags, mode_t mode)`;
`<unistd.h>` declares
`ssize_t read(int fd, void* buffer, size_t count)`,
`ssize_t write(int fd, const void* buffer, size_t count)`,
`off_t lseek(int fd, off_t offset, int whence)`, `int close(int fd)`,
and `<sys/stat.h>` declares `int stat(const char* path, struct stat* buffer)` and
`int fstat(int fd, struct stat* buffer)`. These translate raw
Linux syscall errors to -1 and set `errno` to the positive error number.
On success, `open` returns a file descriptor, `read` and `write` return byte
counts, and `close`, `stat`, and `fstat` return 0. `read` returns 0 at EOF; reads and
writes can be short and are not retried. Successful calls leave `errno`
unchanged. The caller must pass a byte count and sufficiently
large buffers; calls pass the count directly to Linux.

The socket interfaces `socket`, `bind`, `connect`, `listen`, `accept`,
`recvfrom`, `sendmsg`, and `recvmsg` use Linux x86-64 kernel socket and
message layouts. They return -1 and set `errno` on failure, and preserve
`errno` on success. Receive and send calls may return short byte counts.
`recvfrom` accepts optional source-address and address-length pointers;
`sendmsg` and `recvmsg` use the `struct msghdr` and `struct iovec` layouts.
`sendto`, `shutdown`, `getsockname`, `getpeername`, `socketpair`,
`setsockopt` and `getsockopt` use Linux socket semantics. `sendfile` can use
an explicit offset without changing the input descriptor's position.
Crunch `clone` takes four fixed arguments, runs the child function on the
supplied stack, and does not support optional TID, pidfd or TLS parameters.
The runtime is not thread-safe. A `vfork` child shares its parent's address
space and stack; it must immediately call `execve` or `_exit`, without
returning from its calling function or modifying other data.
`<sys/un.h>` provides Unix-domain addresses; `<netinet/in.h>` provides IPv4
addresses. IPv4 ports and addresses must be supplied in network byte order;
byte-order conversion functions are not yet provided.

`nanosleep` uses a relative Linux `struct timespec`, returning -1 with `EINTR`
and optionally writing the remaining interval when interrupted. `sleep`
returns zero on completion or the remaining seconds rounded up on interruption.
`sigaction` uses Crunch's Linux x86-64 kernel-layout `struct sigaction` and
an eight-byte `sigset_t`, not libc's layouts. `sigemptyset` clears that set.
Crunch supplies the signal-return trampoline when installing an action;
do not unload the runtime while its handlers are installed. `SIG_DFL` and
`SIG_IGN` select the default and ignored dispositions. `SIG_IGN` calls a
runtime helper and is not an integer constant expression. The kernel may
expose its internal restorer flag and pointer when an action is queried.
`sigprocmask` uses the same eight-byte kernel signal set; `SIG_BLOCK`,
`SIG_UNBLOCK` and `SIG_SETMASK` select the operation. Signal n is bit n-1.
The kernel-only `rt_sigreturn` is provided by the signal trampoline and is
not callable as an ordinary library function.

`pread` and `pwrite` perform positioned I/O without changing the file offset;
Linux still appends `pwrite` calls on an `O_APPEND` descriptor. `readv` reads
into `<sys/uio.h>` vectors. `mprotect` changes mapping permissions.
`brk` uses the glibc success/error convention but changing the process break
can conflict with other allocators. Crunch `ioctl` always takes three
arguments, with its third argument a pointer; use `ioctl_value` for integer
arguments and pass zero for requests with no argument. These wrappers translate
Linux errors to `errno`, leaving it unchanged on success.

`writev` writes scatter/gather vectors. `msync`, `mincore`, and `madvise`
operate on page-aligned mappings using Linux flags. `shmget`, `shmat`,
`shmctl`, and `shmdt` provide Linux x86-64 System V shared memory;
check `shmat` with `map_failed`, detach with `shmdt`, and mark segments for
removal with `shmctl(IPC_RMID)` to avoid persistent resources. IPC and
segment metadata structures follow the Linux x86-64 kernel ABI.
`pause` waits for a signal; `alarm` shares the real timer with `setitimer`.

`open` always takes three arguments. Its flags can combine the Linux constants
`O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_EXCL`, `O_TRUNC`, and
`O_APPEND`. The mode is a numeric Linux permission mask, used when creating
a file and modified by the process umask; pass 0 when it is not needed.
`lseek` sets the file offset relative to the beginning (`SEEK_SET`), current
offset (`SEEK_CUR`), or end (`SEEK_END`), returning the resulting byte offset.
It can fail with `ESPIPE` for a pipe or other nonseekable descriptor.
`STDIN_FILENO`, `STDOUT_FILENO`, and `STDERR_FILENO` name descriptors 0, 1,
and 2 respectively.
`stat` follows symbolic links and writes the Linux x86-64 144-byte kernel
`struct stat` layout declared in `<sys/stat.h>`. Fields include `st_dev`, `st_ino`,
`st_nlink`, `st_mode`, `st_uid`, `st_gid`, `st_rdev`, `st_size`,
`st_blksize`, `st_blocks`, and seconds/nanoseconds as `st_atime`,
`st_atime_nsec`, `st_mtime`, `st_mtime_nsec`, `st_ctime`, and `st_ctime_nsec`.
Crunch has signed `int` and `long` fields, so interpret unsigned kernel
values as their bit patterns. The layout is target-specific, not a portable C
library `struct stat`. The destination must be writable and large enough.
`fstat` uses the same layout for an open descriptor, even after the file has
been renamed or unlinked.

`openat(int directory, const char* path, int flags, mode_t mode)` opens a path
relative to a directory descriptor, or to the current directory when it is
`AT_FDCWD`. An absolute path ignores the descriptor. Like `open`, it always
takes a mode argument. `O_DIRECTORY`, `O_NOFOLLOW`, and `O_CLOEXEC` are also
available for `open` and `openat`.

The directory-relative interfaces `fstatat`, `readlinkat`, `unlinkat`,
`mkdirat`, `renameat`, and `symlinkat` interpret each relative path against
its directory descriptor, or the current directory for `AT_FDCWD`; absolute
paths ignore their corresponding descriptor. `fstatat` uses the existing
`struct stat` layout and follows the final symbolic link unless
`AT_SYMLINK_NOFOLLOW` is set. Linux `AT_EMPTY_PATH` allows an empty path to
refer to the descriptor itself. `readlinkat` returns the number of bytes
copied without appending a NUL; a count equal to buffer capacity can mean
truncation. The caller must provide sufficient storage for the count.
`unlinkat` removes a non-directory entry with flags zero, or an empty directory
with `AT_REMOVEDIR`. `mkdirat` applies the process umask to its mode.
`renameat` has independent source and destination descriptors and Linux
replacement and cross-filesystem restrictions. `symlinkat` stores the target
text as given, even if it does not exist; its target argument is not resolved
against the directory descriptor. These calls return -1 and set positive
`errno` on failure; successful calls leave `errno` unchanged.

`lstat(const char* path, struct stat* buffer)` reports the final symbolic link
itself rather than following it. `ssize_t readlink(const char* path, char* buffer,
size_t count)` copies at most `count` bytes of the link target and returns the
number copied, without adding a NUL; a return equal to the buffer size may
indicate truncation.

`ssize_t getdents64(int fd, void* buffer, size_t count)` reads Linux x86-64 directory
records, returning bytes read, zero at end of directory, or -1 with `errno`.
The count must be between 0 and 4294967295 and the buffer must hold that many
bytes. Records are variable-length and target-specific.
`dirent64_next(const char* buffer, long count, long* position, struct dirent64*
entry)` decodes one record from a returned buffer. Start `position` at zero
for each new buffer. It returns 1 for an entry, 0 at the buffer end, or -1
with `EINVAL` for an invalid position, count, or malformed record. On failure
the position and entry are unchanged. `entry.name` points into the input
buffer and is valid only while that buffer remains unchanged; `name_length`
excludes its terminator. `inode` and `offset` retain the kernel bit patterns
in signed longs; `offset` is an opaque directory-position cookie, not a byte
offset into the buffer. `kind` is a `DT_*` value and may be `DT_UNKNOWN`.
The iterator validates record boundaries and termination but cannot validate
whether caller-supplied pointers actually reference accessible storage.
Successful calls leave `errno` unchanged.

`unlink(const char* path)` removes a non-directory entry;
`rename(const char* old_path, const char* new_path)` renames or moves an
entry under Linux replacement and filesystem restrictions.
`mkdir(const char* path, mode_t mode)` creates a directory with permissions
modified by the umask (and possibly a default ACL); `rmdir(const char* path)`
removes an empty directory. Each returns zero on success or -1 with `errno`
set on failure, and leaves `errno` unchanged on success.
The low-level `_crunch_*` entry points are compiler-internal and are not
declared by the public headers.

The existing `file_open(path, mode)`
takes `FILE_READ`, `FILE_WRITE` (create/truncate, requested permissions 0644,
subject to the process umask) or
`FILE_APPEND` and returns a descriptor. `file_read` and `file_write` take a
descriptor, a byte buffer and a byte count and return the number of bytes
transferred; `file_close` returns 0 on success. All return exactly -1 on failure
and set `errno` to the positive Linux error number. `file_read` returns 0 at
end of file; short transfers are possible. Successful calls, including
end-of-file reads, do not clear or otherwise change `errno`.

Include `<errno.h>` to read or assign `errno`, for example
`if (file_close(fd) == -1 && errno == EBADF) { ... }`. It is initially zero
and uses process-wide writable `int` storage, not thread-local storage.
The header exposes Linux x86-64 error constants such as `ENOENT`,
`EINTR`, `EBADF`, `ENOMEM`, `EACCES`, `EFAULT`, `EINVAL`, and `ENOSPC`.
`EWOULDBLOCK` aliases `EAGAIN`, and `EOPNOTSUPP` aliases `ENOTSUP`.
Check a function's failure return before inspecting `errno`; an older error
may remain after success. Both sets of file I/O functions and `input` set
`errno` on failure; other existing runtime routines retain their previous error behavior.
The header declares `extern int errno;`; a direct declaration without the header
refers to the same runtime storage. The compatibility accessor
`__errno_location()` returns its address. Programs should not define their own
`errno`, which conflicts with the runtime definition when that object is linked.

`int input(char buf[], int max_length)` reads a line from standard input,
stores at most `max_length` bytes, and appends NUL. The caller must provide
at least `max_length + 1` writable bytes: `input(buf, 5)` needs a six-byte
buffer. The newline is not stored; excess bytes are discarded through the
newline or EOF on success, so the next call starts with the next line. Counts are bytes,
not decoded Unicode characters. Only the LF newline is stripped.

The return value is the stored count, including a partial final line at EOF;
zero means an empty line or EOF before any bytes. A zero maximum consumes the
line and writes only NUL. A null buffer or negative maximum returns -1 with
`errno = EINVAL` without consuming input. Read errors return -1 with positive
Linux `errno`, leaving any stored prefix NUL-terminated. Interrupted reads
are retried; success and EOF leave `errno` unchanged. After a read error, bytes
already consumed cannot be recovered, and a subsequent call starts at the
remaining part of the same line if the newline was not yet consumed.

`<environment.h>` declares `env_get(environment, name)`. Pass the `envp`
parameter from `main`; the function returns a pointer to the value of the
matching environment variable, or a null pointer when the name is absent.
The returned value points into the original environment string and remains
valid for the lifetime of the process. An empty name or null argument returns
null. The function does not modify the environment.

The headers contain only prototypes; implementations are supplied by
`libcrunch.a` and linked when used. Library functions are ordinary
functions, so a program may define its own function with a library name (for
example `int strlen(int x) { ... }`). The user's definition then takes
precedence and the runtime helper is not used. In module mode, calls to
runtime-named declarations use ordinary linkage where possible, with weak
fallbacks selecting the runtime when no user definition is linked. Formatted
output and `vfork` dispatch between ordinary user functions and their special
runtime implementations. Explicit `extern` declarations use ordinary linkage.
When a referenced declaration disagrees with a runtime function's arity, return
class, or pointer/integer argument classes, module output requires a user
definition and rejects the runtime fallback when the guarded object and
fallback are linked together. For dynamic executables, `crunch-ld` also checks
guard metadata in explicitly supplied shared libraries. This check does not
cover libraries loaded later or only as indirect dependencies, older libraries
without guard metadata, libraries that remove it, or final links using other
linkers. As with ordinary C linkage, linkers cannot verify complete
function type compatibility across modules; declarations and user definitions
must still agree.

## 6.1 String and memory built-ins

`strlen(text)` from `<string.h>` returns the number of bytes before the first
NUL byte as `size_t`. `strcmp(a, b)` compares NUL-terminated strings by
unsigned-byte value. String literal storage is read-only.

The standard header `<stdlib.h>` declares the non-returning exit function:

    #include <stdlib.h>
    void noreturn exit(int status);

`exit(status)` terminates the process with the supplied status by issuing the Linux x86-64 `exit` syscall. If a managed executable-finalization hook is available, it runs before the syscall. Unlike standard C `exit`, it does not run `atexit` handlers, flush C streams, or run shared-library finalizers. Its single argument must be an integer (`char`, `int`, or `long`). As with Linux process exit status, only the low eight bits are observable by the parent process. A `noreturn` function never returns to its caller; statements after the call are unreachable.

The C `_Noreturn` function specifier is supported. `noreturn` is an ordinary
identifier, except in the legacy `void noreturn function(...)` declaration
syntax, which remains accepted.

The core language also provides these byte-range memory operations:

- `copy(destination, source, byte_count)` copies exactly the requested number of bytes and returns `destination`. The source and destination ranges must not overlap. The destination must point to writable memory; string literal storage must not be used as a destination.
- `fill(destination, value, byte_count)` writes the low eight bits of `value` to each destination byte and returns `destination`.
- `compare(left, right, byte_count)` compares exactly the requested number of bytes and returns a negative value, zero, or a positive value using unsigned-byte ordering. It does not stop at NUL bytes.

The pointer arguments must refer to valid memory for the entire requested range. The byte count must be nonnegative. A zero byte count performs no memory access; defensively, a negative signed count is treated as zero. These routines do not check pointer validity or allocation bounds.

## 7. Control flow

Supported statements:

- if / else
- switch / case / default
- for
- while
- do / while
- break
- continue
- goto and named labels
- return
- compound blocks

`while (expression)` behaves like a C while loop: its
condition is evaluated before each iteration.
`do statement while (expression);` evaluates the condition after each
iteration, including after a `continue`.

`switch (integer_expression) { ... }` dispatches to matching `case` labels or
to an optional `default` label. Case labels must be distinct integer constant
expressions representable by the switch expression's type. A switch expression
must have type `char`, `int`, or `long`. Execution continues from the matching
label through subsequent labels until a `break` or the end of the switch, so
cases fall through as in C. `break` exits the innermost enclosing loop or
switch; using it outside either is a semantic error. For now, `case` and
`default` labels must appear directly in the switch body; a case body may
contain a braced block.

`goto name;` transfers control to `name: statement` in the same function.
Labels have function scope and a namespace separate from variables and types;
duplicate or undefined labels are errors. Forward and backward jumps and
jumps into nested blocks are supported. As in C, a jump that bypasses an
automatic initializer does not run that initializer.

File-scope `static inline` functions retain internal linkage; `inline` is
an optimization hint and does not guarantee inlining. For an external-linkage
function, a definition whose file-scope declarations all say `inline` and none
say `extern` is an inline-only definition; it does not supply an external
definition. Crunch emits calls and addresses using the external function and
requires module mode for such a translation unit. Another translation unit
must provide the external definition when it is used. An `extern inline`
definition, or an inline definition with a file-scope declaration that is
`extern` or lacks `inline`, supplies the external definition. An inline-only
definition cannot define a modifiable static object or reference an identifier
with internal linkage.

`__builtin_alloca(size)` allocates `size` bytes aligned to 16 bytes on the
current function's stack. Its lifetime ends when that function returns.
In functions using this builtin, VLA storage is also retained until return.
The `--glibc` mode supplies an `alloca.h` wrapper mapping `alloca` to the
builtin. Stack space is limited; excessively large allocations can trap or
exhaust the stack.

Functions may accept arrays as pointer parameters. The array
element expressions are available in the callee; pass the length as a separate
integer argument when the callee needs a bound. The entry point may be declared
as `int main()`, `int main(int argc)`, or with the conventional argument
vectors:

    int main(int argc, char argv[][], char envp[][])

At process startup, `argv` and `envp` point to the null-terminated vectors
provided by Linux. The compiler supports indexing these vectors and passing an
individual byte string (such as `argv[1]`) to a function taking `char[]`.
Arrays of structures may likewise be passed as `struct Name values[]`;
structure element bounds should be passed separately when needed.

## 8. Compilation pipeline

Source file -> lexer -> parser -> semantic analysis
            -> intermediate representation -> code generation

Each stage has an independent C interface and receives
its inputs explicitly. Lexing and parsing operate in memory.


## 9. Structures

The `struct` keyword introduces a named structure type. Structures can contain numeric and pointer fields, fixed-array fields, dynamic numeric-, structure-, or pointer-array fields, and fields of another structure type. Nested member expressions work with dot notation, for example `person.address.postcode`.

Structures and unions support bit-fields declared with `_Bool`, `int`,
`signed int`, or `unsigned int`, including typedefs of these types. The width
must be an integer constant expression between zero and the type's width
(`_Bool` has a maximum width of one); zero-width fields must be unnamed.
Other implementation-defined bit-field base types are rejected.

Bit-fields use the Linux x86-64 System V layout: unnamed fields occupy bits
without raising aggregate alignment, and an unnamed zero-width field advances
the next field to the declared type's allocation boundary. Plain `int`
bit-fields are signed. Reads sign-extend signed fields; assignments truncate
to the field width and yield the stored value. Integer promotions apply,
including promotion of unsigned fields narrower than 32 bits to `int`.
Address-of and `sizeof` cannot be applied directly to a bit-field.
Positional and designated initializers skip unnamed fields, and structure and
union copies and by-value calls preserve the object representation.
Volatile bit-fields are accessed one occupied byte at a time; they are not
suitable for devices requiring a particular hardware access width.

`struct` and `while` are reserved. The former keywords `type` and `loop` are ordinary identifiers;
existing source code must use `struct` for structure declarations and `while` for loops.

    struct Point { int x; int y; };
    struct Shape { struct Point origin; int kind; };

    int main() {
        struct Shape shape;
        shape.origin.x = 10;
        shape.origin.y = 20;
        int *x = &shape.origin.x;
        *x = 42;
        return shape.origin.x;
    }

The address-of operator accepts local and global structure variables, structure-array elements, and scalar structure members, including nested members. The resulting pointer aliases the object or member. Both local and global structure objects use contiguous, byte-addressable storage with the same calculated field offsets and alignment rules. A local structure occupies an aligned block in its function stack frame; taking a member address yields the actual address at that field offset. Adjacent fields can therefore be accessed through pointers when their types and offsets make that valid. Structure pointers can be passed and returned by functions, stored in local or global variables and structure fields, compared, indexed, and used in pointer arithmetic. `p->field` accesses the same field as `p[0].field`; supported reads, assignments, addresses, nested members, and pointer-field updates evaluate the pointer expression once. Structure-pointer fields allow chains such as `p->next->value`. Fixed-array fields can be indexed and addressed through a structure pointer. Structure pointers use the standard pointer ABI in separately compiled modules and C interoperability. The pointed-to object must remain alive; returning a pointer to a local structure does not extend its lifetime.

```c
struct Node { int value; struct Node* next; };
struct Node first;
struct Node second;
first.next = &second;
first.next->value = 42;
```

Whole-structure assignment is supported for objects of the same structure type, including nested members:

    destination = source;

Assignment copies field values recursively, including fixed-array contents.
Pointer fields are shallow-copied; pointed-to allocations are not automatically
freed when a structure is overwritten or leaves scope.

Local and global structure storage supports numeric scalar fields, pointer fields
(including pointers to pointers), nested structure fields, and fixed-array
fields. Structure-pointer fields may refer to their containing structure type.
Pointer fields are initialized to null; other uninitialized fields retain
unspecified contents. A local structure can be initialized from another
instance of the same type. Fixed arrays of structures use the computed aligned
structure layout and support indexed access and whole-element copies. Recursive
by-value structure definitions are not supported. Functions may pass and return
ordinary structures by value, including nested integer, pointer, floating-point,
and fixed-array fields. Structures up to 16 bytes use the appropriate INTEGER
and SSE eightbyte classes, with binary80 aggregates following the x87 or memory
rules; larger supported structures use the memory-class ABI.
Calls use the Linux x86-64 System V ABI and copy each
structure argument at its left-to-right evaluation point; changing the
parameter cannot modify the caller's structure. Supported arguments include
structure lvalues, conditional expressions of supported structure arguments,
and assignments to named structures or direct members.

Unions use the same declaration, member access, initialization and assignment
forms as structures. All members begin at offset zero; the union's size is its
largest member size rounded up to its largest member alignment. Named and
anonymous unions can appear inside structures, including anonymous members
whose fields are promoted. A positional union initializer selects its first
member; designated initializers can select another member. Whole-union
assignment copies the object representation. Unions and structures containing
them can be passed and returned by value, including floating-point,
mixed-class, and larger memory-class layouts. By-value aggregates are limited
to 999992 bytes and alignments of at most 16 bytes by the current IR.
Arguments are copied at their evaluation point, and returned values have
temporary storage. Pointers to unions can also be passed and returned.

An unnamed structure embedded without a field name is an anonymous member:

```c
struct Rectangle {
    struct { double x, y; };
    double width, height;
};
```

Its fields are accessed directly as `rectangle.x` and `rectangle.y`. Nested
anonymous members promote their fields recursively. Promoted names must not
conflict with other visible fields of the containing structure. Anonymous
members retain their own layout and alignment, and const qualification applies
through them. A named field such as `struct { int x; } position;` is not
anonymous; its members are accessed through `position`.

## 11. Real literals

Real literals include decimal fractions and exponent notation. A leading or
trailing decimal point is allowed when the literal contains at least one
digit: `.5`, `12.`, `1.25`, `1e3`, and `.5E-2` are examples. A standalone
dot remains punctuation.


## Numeric casts and constant ranges

`char` and `unsigned char` are unsigned 8-bit, `signed char` is signed 8-bit, `short` is signed 16-bit, `unsigned short` is unsigned 16-bit, `int` is signed 32-bit, `long` is signed 64-bit, `unsigned int` is unsigned 32-bit, `unsigned long` is unsigned 64-bit, and `double` is IEEE 754 binary64.

Numeric assignments, initializers, arguments, returns, and explicit casts allow
conversions among all supported numeric types. Constant initializers, simple
assignments, explicit casts, function arguments, and return values are checked
for destination range. Compound assignments depend on the target's runtime
value and are not checked by treating the right-hand operand as the result.
Out-of-range constant conversions produce warnings, not errors; narrowing a
runtime value is allowed. Literal range and constant-expression requirements
remain errors.

### Runtime numeric conversions

These are Crunch's current Linux x86-64 rules, not a promise of C-compatible
behavior for conversions outside C's defined range:

- Integer narrowing retains the low-order destination-width bits, interpreted
  using the destination's signedness.
- Floating-to-signed-integer conversions truncate
  toward zero first and accept the result if it is representable. Thus
  `-128.5`, `-32768.5`, and `-2147483648.5` convert to the respective
  signed minima. Binary32 and binary64 have no representable value between
  `-2^63-1` and `-2^63`, so the `long` boundary is unchanged for these sources.
- A `float` or `double` converted to `unsigned char` or `unsigned short` is
  accepted in `(-1, 256)` or `(-1, 65536)` respectively and truncated toward zero.
- A `float` or `double` converted to `unsigned int` or `unsigned long` is
  accepted in the open interval `(-1, 2^N)`, where N is the destination width.
  An accepted value is truncated toward zero; a negative fraction greater than
  `-1` therefore converts to zero.
- A `float` or `double` converted to `char` is accepted in `(-1, 256)` and
  truncated toward zero.
- A floating-to-integer conversion outside the applicable interval, or of NaN
  or either infinity, produces zero without a runtime diagnostic under the
  usual masked floating-point exception environment. With invalid-operation
  exceptions unmasked, a `double` signaling NaN can instead raise a hardware
  exception during the check.
- Conversion to `float` rounds to binary32 using round-to-nearest,
  ties-to-even. Overflow rounds to signed infinity; a value just beyond the
  largest finite `float` can instead round back to that finite value.
  Converting a finite `float` to `double` preserves its value exactly.

Supported static floating-to-integer initializers use the same conversion
results. A compile-time range warning is separate from these runtime rules:
the compiler cannot generally diagnose the value of a runtime expression.
An out-of-range floating-to-integer cast is not an integer constant expression
for contexts such as array bounds and switch labels, even though it can be
compiled with a warning in an ordinary initializer or assignment.

Unsigned integer arithmetic wraps at its result width. Programs must not rely
on results for signed overflow, division or remainder by zero, signed
minimum divided by `-1`, or invalid shift counts; these are not defined by
C11. Signed right shifts use arithmetic shift on this target. Known invalid
operations are diagnosed in the contexts described above. Floating arithmetic
follows the documented binary32 and binary64 rules; comparisons with NaN are
unordered. Matching static and runtime rounding assumes the usual
round-to-nearest floating-point environment.


### Structure backend support

Structure declarations may contain numeric scalar fields, pointer fields,
nested structures, and fixed-array fields. Both local and global structure
objects use contiguous byte-addressable storage with the same calculated field
offsets and alignment. Nested member access, scalar member addresses, and
recursive whole-structure assignment are supported. Local structure declarations
may be initialized from another instance of the same type; array-element
copies are supported. Structure assignment copies pointer values shallowly.
Positional local and global structure initializers are supported as described
above. Supported structure-valued parameters and returns follow the aggregate
ABI described above.


## 8. Output

Use `printf` from `<stdio.h>` to write to standard output. For example:

    #include <stdio.h>
    int main() {
        printf("Hello, world!\n");
        return 0;
    }

String literals are stored as NUL-terminated bytes in read-only static storage.
The former `print` built-in is removed; `print` is an ordinary identifier.

The functions `strlen` and `strcmp`, declared in `<string.h>`, inspect
NUL-terminated strings. Pointer operators `==` and `!=` compare addresses,
including for `char*`; use `strcmp` to compare string contents. Whether
separate string literals share storage is unspecified. `length` is an ordinary
identifier and no longer names a built-in function.

The compiler's numeric formatting helpers are internal to formatted output; source
programs use the format conversions below instead of calling those helpers.
`printf(format, values...)` writes to standard output, while
`fprintf(stream, format, values...)` writes to a Crunch `FILE` stream,
`sprintf(buffer, format, values...)` writes to an unbounded byte buffer, and
`snprintf(buffer, capacity, format, values...)` writes to a bounded byte buffer.
`vfprintf(stream, format, arguments)` writes to a stream using a `va_list`.
All are declared in `<stdio.h>` and write without adding a newline. Their initial
C-style conversion subset is:

| Conversion | Argument |
|---|---|
| `%d` | Signed `int`, or a `char` promoted to `int` |
| `%ld` | Signed `long` |
| `%lld` | Signed `long long` |
| `%u` | `unsigned int` |
| `%lu` | `unsigned long` |
| `%zu` | `size_t` |
| `%llu` | `unsigned long long` |
| `%x` | `unsigned int` in lowercase hexadecimal |
| `%lx` | `unsigned long` in lowercase hexadecimal |
| `%llx` | `unsigned long long` in lowercase hexadecimal |
| `%f` | `double`, with six fractional digits and the range/rounding of the internal real formatter |
| `%.17g` | `double`, rounded to 17 significant digits, ties to even |
| `%Lf` | `long double`, with six fractional digits |
| `%.NLf` | `long double`, with `N` fractional digits, for literal `N` from 0 to 100 |
| `%.17Lg` | `long double`, rounded to 17 significant digits, ties to even |
| `%s` | Non-null `char*` pointing to a NUL-terminated string |
| `%c` | `char` or `int`; writes the low byte, including NUL |
| `%%` | Writes a literal percent sign; consumes no argument |

Conversions other than `%%` accept an optional literal decimal field width up
to 2147483647 and the `-` and `0` flags. A field wider than its value is
right-aligned with spaces by default; `-` left-aligns it, and `0` pads numeric
conversions with zeros after any minus sign. `-` overrides `0`; `0` is ignored
for `%s` and `%c`. A width never truncates the value. For example, `%08d`
formats `-42` as `-0000042`, and `%-8s` pads a string on the right.
For `%.17g`, trailing fractional zeros are removed, scientific notation is used
when the rounded decimal exponent is less than -4 or at least 17, and infinity
and NaN are written as `inf` and `nan`. Zero padding is ignored for non-finite
values. This conversion uses round-to-nearest, ties-to-even regardless of the
floating-point environment.
`%Lf`, `%.NLf` and `%.17Lg` format the full binary80 range without narrowing
and use round-to-nearest, ties-to-even regardless of the floating-point
environment. `%.Lf` is equivalent to `%.0Lf`; leading zeros in the precision
are accepted.

For example:

```c
#include <stdio.h>
int main() {
    printf("name=%s count=%d value=%f\n", "test", 3, 1.25);
    return 0;
}
```

For `printf` and `fprintf`, the return value is the number of bytes written, or `-1` for an output error,
unsupported conversion, missing argument, conversion/type mismatch, null string,
unrepresentable output count, or unsupported real value. Errors can leave a
prefix already written. Extra arguments are evaluated but otherwise ignored.
Formats may be computed at runtime. `%.*s` accepts an integer precision before
the string; a negative precision is treated as if no precision were supplied.
An `%Lf` precision greater than 100 is unsupported. Other precisions, including
dynamic `%Lf` precision, dynamic widths, other flags, and
other conversions are not supported yet. `%%` does not accept a width or flags.
Callers must supply valid NUL-terminated strings. A null format returns `-1`.
For `fprintf`, a null stream returns `-1` and sets `errno` to `EINVAL`, even
when the format is empty. A stream write failure sets `errno` and the stream
error indicator. Format errors have no specified effect on either. Crunch streams
are unbuffered and are not compatible with C library `FILE` objects.
`fflush` therefore succeeds without performing I/O, including for a null stream.
`tmpfile` creates an anonymous seekable read/write stream using Linux
`memfd_create`; its descriptor is close-on-exec and its storage is memory-backed.
`fileno` returns a stream's underlying file descriptor.
`<stdio.h>` defines `FILE` as an alias for `struct FILE`; both spellings
refer to the same Crunch stream type.

`snprintf` returns the number of bytes that would have been written, excluding
the terminating NUL, even when truncated. With a positive capacity it writes at
most capacity minus one bytes and a NUL terminator, including on format error;
the destination must be writable and large enough. A zero capacity writes
nothing and permits a null buffer. A null buffer with positive capacity fails
with `-1` and `errno = EINVAL`. Other formatting errors return `-1` without a
specified errno. The capacity has type `size_t`; the destination and input
strings must not overlap.

`putchar` writes its low byte to `stdout` and returns that byte or `EOF`.
`puts` writes a string followed by a newline to `stdout`, returning a
nonnegative value or `EOF`. Failures set the stream error indicator.
Formatted output accepts corresponding signed and unsigned integer argument
types when the value is representable in both, including promoted narrow
unsigned values for `%u` and `%x`.

When a literal format contains only supported conversions, the compiler warns
if its number of conversions differs from the number of supplied values.
This warning is non-fatal and does not check argument types. Computed and
unsupported formats are not checked.

External variadic functions and calls through variadic function pointers use the
System V ABI, including default argument promotions. External calls require
module compilation; aggregate arguments retain the existing ABI limitations.
`<stdarg.h>` provides System V AMD64 `va_list`, `va_start`, `va_copy`, `va_end`
and `va_arg` for `int`, `unsigned int`, `long`, `unsigned long`,
`long long`, `unsigned long long`, `double`, pointers, enums and aggregates
with supported by-value ABI layouts. `va_start`
must name the final named parameter of a variadic definition; lists are valid
only while that invocation is active. Reading arguments advances the list, and
`va_copy` creates independent state. Types must match the actual promoted
argument types; `float` and narrow integer `va_arg` types are rejected.
Aggregate types unsupported as by-value parameters are also rejected.
Argument bounds are not checked.
The GCC-compatible `__builtin_va_list` type has the System V AMD64 array
layout and supports declarations from GCC and glibc headers. GCC's
`__builtin_va_start`, `__builtin_va_arg`, `__builtin_va_copy` and
`__builtin_va_end` use the same ABI and supported argument types and layouts
as Crunch's `<stdarg.h>`.
`vsnprintf` consumes a list from Crunch or interoperating C. It supports the
same conversions as `snprintf`.
Library `printf`, `fprintf`, `sprintf`, and `snprintf`
retain their private runtime calling convention (at most 127 values after the
format).
`scanf` and `fscanf` similarly accept up to 127 destination pointers. They
support integer, floating-point, byte character, string, scanset, pointer and
`%n` conversions, assignment suppression, and field widths. See
`fscanf(3crunch)` for the supported modifiers and limitations; wide-character
input and locale-dependent conversions are not supported.
The library header declarations omit the ellipsis; the compiler
recognizes calls to these library prototypes. User definitions and explicit
`extern` functions of these names remain ordinary fixed-argument functions.

## 8.0 User headers

`#include "path/file.h"` includes a user header. The name is resolved first
relative to the directory of the file that contains the directive, so
`#include "inc/some_header.h"` and nested includes work from any working
directory. If it is not found there, the standard search order is used
(`-I` directories, `CRUNCH_INCLUDE_PATH`, the built-in directory, then
`crunch.conf`), so `crunch -I inc prog.bc` also finds `inc/file.h` for
`#include "file.h"`. `#include <name>` skips the relative lookup. Absolute names
and `..` components are rejected. Include guards (`#ifndef`/`#define`/`#endif`)
and cycle detection apply to both forms.

## 8.1 Standard runtime headers

The configured include directory provides guarded headers for the assembly-backed
runtime routines. Include `<stdlib.h>` for `void noreturn exit(int status);`,
`<string.h>` for `memcpy`, `memset`, `memcmp`, `strcmp`, and `strlen`, and
`<stdio.h>` for `printf`. These headers provide
declarations only. The implementations are in `libcrunch.a`, and the linker
extracts the members needed by the program.

The existing string and memory declarations use `char*` pointers. Memory
operations take byte counts, `memcpy` requires non-overlapping
ranges, and the string routines require valid NUL-terminated byte strings.
The core built-ins `copy`, `fill`, and
`compare` are available without including these headers; everything declared
here requires its header (see section 6.0).
