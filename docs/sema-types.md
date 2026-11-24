# Semantic type checking (milestone 0.8)

The semantic analyzer now performs first-pass expression and statement type checking after symbol registration and name resolution.

## Primitive types and conversions

The default rules use C-style promotions and numeric conversions.
`--c11-numeric` is an accepted compatibility option with no effect.
Plain `char` promotes to `int`, character constants have
type `int`, shifts use the promoted left type, and all numeric assignments,
arguments, returns and casts are permitted. The remaining range and literal
limitations are documented in the language specification.

- `char` is unsigned 8-bit, `int` signed 32-bit, `long` signed 64-bit, and `double` IEEE-754 binary64.
- `_Bool` is a distinct one-byte integer type that promotes to `int`; conversion
  from a numeric or pointer scalar yields 0 or 1. It uses AST v22.
- Enum types retain scoped identity and are compatible with `int` if any
  enumerator is negative, otherwise `unsigned int`. Enumerators themselves are
  `int` constants, evaluated in declaration order and restricted to the signed
  32-bit range. See the language specification for supported definition
  contexts and remaining tag limitations.
- `long long` and `unsigned long long` are distinct 64-bit types with higher
  rank than `long` and `unsigned long`. A mixed `long long` / `unsigned long`
  expression uses `unsigned long long` on this target.
- `signed char` and `unsigned char` are distinct 8-bit types. They promote to
  `int`, allow numeric conversions, and preserve their identity in pointers and
  function signatures. They use AST v19 and IR v16/v17.
- `short` and `unsigned short` are distinct signed and unsigned 16-bit types,
  with size and alignment 2. They promote to `int` in arithmetic and accept
  numeric conversions; integer narrowing retains the destination's low bits.
  Their pointers and function signatures retain their distinct types. Sources
  using them select AST v18 and IR v14/v15.
- The frontend recognizes `float` as IEEE-754 binary32, with size and alignment 4. Decimal real literals with an `f` or `F` suffix are rounded directly to binary32. For arithmetic and conditional branches, `double` takes precedence over `float`, and `float` takes precedence over integer types. Assignments, arguments, returns, and explicit casts between `float` and other numeric types are permitted, subject to the existing constant range checks. Integer-only operators, indices, and counts reject `float`. Float-containing programs lower to binary32 IR v8/v9.
- Numeric assignments, initializers, arguments, returns, and explicit casts
  permit conversions among all supported numeric types.
- Unsuffixed decimal literals use `int` or `long`; hexadecimal and octal
  literals also use `unsigned int` and `unsigned long` in C candidate order.
  The `L` suffix selects `long`, or `unsigned long` for a non-decimal literal
  outside the signed range. `LL` and `ULL` literals use the corresponding
  long long types, with unsigned selection for non-decimal `LL` values outside
  the signed range.
- `unsigned int` and `unsigned long` are type-checked as 32-bit and 64-bit unsigned integers. `U` literals select the first type that fits; `UL` and `LU` select `unsigned long`. Values above the unsigned 64-bit maximum are rejected.
- Mixed signed/unsigned arithmetic uses the conversions in `language-spec.md`. Integer narrowing uses the documented modulo and low-bit conversions. Floating-to-integer conversions are allowed implicitly and explicitly.
- `sizeof` and `__builtin_offsetof` yield `unsigned long`, including in constant expressions and
  mixed signed/unsigned arithmetic.
- Constant evaluation handles unsigned wraparound, comparisons, division, shifts, and integer conversions. Known zero divisors and invalid shift counts involving the new types are rejected, including compound assignments. Switch case values involving unsigned types are converted to the control type before duplicate checking.
- Unsigned expressions, declarations, pointers, constant globals, and function calls lower to executable IR v6 or module IR v7; fixed arrays may require newer IR versions. Unsigned formatted-output arguments use distinct runtime tags.
- Constant initializers, simple assignments, function arguments, and return values are checked for destination-range violations. Explicit casts of constant values are also range-checked; for example, `(char)300` produces a warning but remains compilable. Literal range and constant-expression requirements remain errors.
- Runtime integer narrowing retains low-order bits. Floating-to-integer conversions check signed destinations after truncation and produce zero for out-of-range values, NaN and infinity under the usual masked floating-point environment. Supported static initializers use the same results. See [runtime numeric conversions](language-spec.md#runtime-numeric-conversions).
- Cast syntax is `(type) expression`, for example `(int)measurement` or `(char)count`.
- Arithmetic operators require numeric operands, except pointer addition/subtraction by an integer offset. `%`, bitwise operators, and shifts require integer operands.
- Exponentiation `**` is a non-C extension that accepts numeric operands and produces `double`.
- Constant range checks do not currently evaluate power expressions; a cast of a power result uses runtime conversion rules.
- Comparisons and logical operators produce `int` results. Conditions accept scalar numeric or pointer values.
- Object pointers convert to and from `void*` without discarding pointee `const`
  or `volatile`; explicit casts can remove qualifiers. Implicit conversions
  may add qualifiers at the immediately pointed-to type, but nested pointer
  and array qualifiers must match. Conditional pointers preserve qualifiers
  from both branches. `void*` cannot be dereferenced, indexed, or used in
  pointer arithmetic. Equality with another object pointer compares addresses.

## Checked constructs

The analyzer checks variable initializers and assignments, compound assignments, unary and binary operators, array indexing and initializer elements, structure member existence, function call arity and argument types, conditional expressions, and return expressions against the enclosing function's return type.

Strings have type `char*`. Fixed `char` arrays may be initialized from string literals.

Fixed arrays have distinct element and length types. Their bounds are positive
integer constant expressions or are inferred from list and char-string
initializers. They decay to element pointers in ordinary expressions, retain
their object size for `sizeof`, and cannot be assigned or resized. Array
storage and lowering are implemented for automatic locals, global arrays,
and structure fields. Static local fixed arrays, structures, and scalar variables have
persistent storage and require supported constant initializers. Positional
structure and structure-array lists are checked recursively for excess or
incompatible elements, including when nested braces are elided. Field and
array designators select nested subobjects; indices must be nonnegative integer
constants within the array bound. Array bounds may be inferred from these
lists.
`dim` is an ordinary identifier.

## Limitations

The semantic analyzer diagnoses supported constant ranges but does not diagnose
runtime values. The x86-64 code generator implements the runtime numeric
conversion behavior, including its floating-to-integer range checks.

This is an initial semantic type checker, not yet a complete language conformance checker. Pointer arithmetic and full C-compatible pointer conversions are not accepted as general implicit conversions. Definite initialization, runtime allocation state, all return-path analysis, complete `main` ABI validation, and a standalone JSON-AST semantic filter remain future work.
