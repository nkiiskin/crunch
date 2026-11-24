# Crunch AST interchange format (versions 15 through 49)

The standalone parser writes one JSON document followed by a newline to standard output. The document is the complete parsed syntax tree, not a summary. Its top-level shape is:

```json
{
  "format": "CRUNCH-AST",
  "version": 16,
  "declarations": []
}
```

`declarations` preserves source order for ordinary top-level declarations. Compiler-generated declarations for nested and block-scope unnamed structures are appended after them. Every node has a `kind` and source `location` (`offset`, one-based `line`, and one-based `column`). Types, expressions, declarations, and statements are represented recursively. Optional children are JSON `null`; lists are JSON arrays, including empty lists. Type alias uses are expanded by the parser to their underlying types; otherwise the format does not invent resolved symbol IDs, inferred expression types, or semantic results.

The JSON does not record semantic type information. Semantic analysis uses
C-style numeric rules by default; `--c11-numeric` remains an accepted
compatibility option and does not change the JSON format or its version.

## Node conventions

- Version 49 allows narrow hexadecimal escapes with one or more digits in
  literal spellings. Their values must fit in one byte.
- Version 48 marks wide string expressions with `"wide":true`; `value`
  contains their adjacent source token spellings separated by spaces and
  `length` is the byte length of that spelling.
- Version 47 adds `"inline":true` to non-static inline function declarations.
  Semantic linkage is not serialized.
- Version 46 marks wide character expressions with `"wide":true` and
  serializes their signed 32-bit decoded value in `value`. Ordinary character
  nodes omit `wide` and retain their existing representation.
- Version 45 permits a null `message` in a `static_assert` declaration.
- Version 44 adds `offsetof` expressions with `target_type`, optional
  `enum_definition`, and a `designators` array. The first designator is a
  `field`; subsequent entries are `field` or `index` objects with `location`
  and respectively `name` or `index`. Inline aggregate definitions appear
  among the generated declarations. Resolved offsets are not serialized.
- Version 43 represents packed aggregate definitions.
- Version 42 adds `"thread_local": true` to variable declarations with the
  `_Thread_local` specifier; the property is omitted otherwise.
- Version 41 adds an `alignment_specifiers` array to declarations that
  have `_Alignas` specifiers. Each entry is an expression; a type-name
  specifier is represented as an `alignof` expression. Resolved alignment
  is a semantic result and is not serialized.
- Version 40 permits scalar compound literals as `cast` nodes whose
  `target_type` is scalar and whose `operand` is an `array_initializer`.
- Version 39 permits array compound literals as `cast` nodes whose
  `target_type` is an array and whose `operand` is an `array_initializer`.
  Inferred bounds and initializer plans are semantic results.
- Version 38 adds `alignof` expressions with `target_type`, nullable
  `enum_definition`, and null `operand` fields. The semantic alignment is
  not serialized.
- Version 37 represents bit-field declarations with a `bit_field_width`
  expression. Unnamed bit-fields have a null `name`; they are distinct from
  anonymous aggregate members. Resolved widths and bit positions are semantic
  results and are not serialized.
- Version 36 represents volatile-qualified types with an optional
  `"volatile": true` property on the qualified type node. The property is
  omitted for unqualified nodes; sources using `volatile` select version 36.
- Version 35 adds `static_assert` declarations with `condition` and `message`
  expression nodes. Structures and unions with assertions include an
  `assertions` list; each entry contains the declaration and `preceding_fields`,
  its position relative to the ordinary `fields` list.
- Version 34 represents nested fixed-array bounds and explicit parameter-array
  bounds with an `array_bound` expression on the corresponding array or
  adjusted pointer type. A null `array_bound` represents an omitted inner bound,
  which semantic analysis rejects. An ordinary outermost variable bound is in
  the declaration's `array_size` field; a bound inside a parenthesized
  declarator such as `int (*callbacks[2])(int)` is on the array type.
  Constant-expression typedef bounds also
  use `array_bound`; literal typedef bounds retain the v32 `length` field.
  An incomplete outer array typedef omits both `array_bound` and `length`.
  Parenthesized pointer-to-array declarators and abstract type names use a
  pointer type containing an array type with its bound expression.
- Version 33 adds the `long_double` type kind and `long_double` real
  literals whose `value` is the exact source spelling, including the suffix.
  Semantic analysis supports these constructs; code generation does not yet.
- Version 32 adds a decimal-string `length` field to array types expanded from
  literal-bound array typedefs. Parameter types are adjusted to pointers.
- Version 31 adds an optional `"variadic":true` field to variadic function
  declarations and function types. Its absence means a fixed signature.
- Version 30 adds the `comma` binary operator.
- Version 29 adds `do_while` statements with `condition` and `body` fields.
- Version 28 adds `function` types with `return_type` and a `parameters` array
  of types. Function pointer declarators use a `pointer` whose `pointee` is
  this function type; a function returning a function pointer has that
  pointer as its `return_type`. Unnamed prototype parameters have a null `name`.
- Version 27 adds `union` type and declaration kinds, with the same fields as
  their `struct` counterparts. A block-scope standalone union definition uses
  `union_scope`. Anonymous union members use the existing `anonymous_member`
  representation.
- Version 26 adds `goto` statements with a `name`, and `label` statements
  with a `name` and nested `statement`. Switches with statements before the
  first case have a `prefix` array. The semantic label IDs are not serialized.
- Version 25 adds `designated_initializer` expressions with an ordered
  `designators` array and a `value`. Each designator has `kind` `field` and
  `name`, or `kind` `index` and an `index` expression; resolved indices are
  not serialized.
- Types use `kind`: `void`, `int`, `long`, `unsigned_int`, `unsigned_long`, `byte`, `real`, `float`, `pointer`, or `struct`. Every type has a `const` boolean qualifying that node. Pointer types contain `pointee`; structure types contain `name`. Thus `const int *p` has a non-const pointer node with a const `int` pointee, while `int *const p` has a const pointer node with a non-const `int` pointee.
- The currently ignored `register` and `static inline` function
  hint are not represented in the AST; the `static` linkage remains recorded.
- Version 24 adds an `enum_definition` field to casts, `sizeof` type names,
  and parameters when they contain an enum definition. A block-scope standalone
  named structure definition is represented by a `struct_scope` declaration
  containing its internal `name`; its corresponding structure declaration is
  appended to the document. Block-scope named structure types use generated
  internal names so shadowed tags remain distinct.
- Version 23 adds `enum` declarations with nullable `tag`, `id`, and ordered
  `enumerators` containing `name`, `location`, and nullable `value`. Enum types
  contain the nullable `tag` and matching parser-assigned `id`. Inline
  definitions are serialized before their associated declarations; the ID
  distinguishes anonymous and shadowed types within this document.
- Version 22 adds the `bool` type for `_Bool` and is emitted when it is used.
- Version 21 adds `long_long` and `unsigned_long_long` types and `ll` and
  `ull` literal suffixes. It is emitted when either type or suffix is used.
- Version 20 extends version 19 with the `l` integer-literal suffix, emitted
  for programs using a standalone `L` or `l` suffix.
- Version 19 extends version 18 with `signed_char` and `unsigned_char`, emitted
  for programs using either type. Plain `char` remains `byte`.
- Version 18 extends version 17, adds `short` and `unsigned_short` type kinds,
  and is emitted when the source uses either spelling.
- Version 17 adds `array` types with an `element` type. The declaration's
  `array_size` holds the parsed bound, or is null when inferred; resolved
  lengths are semantic results and are not serialized.
- The versioned type kinds `byte` and `real` represent the source types `char` and `double`, respectively.
- Expressions use descriptive `kind` names and named child fields. Integer literal `value` fields are decimal strings to avoid JSON-number precision loss. Unsigned suffixes and hexadecimal or octal literals preserve values through `18446744073709551615`; `suffix` is `none`, `u`, `ul` (`lu` normalizes to `ul`), `l` (version 20), or `ll` or `ull` (`llu` normalizes to `ull`, version 21). `out_of_range` records that the magnitude exceeds the applicable 64-bit range or cannot be parsed; its `value` is not meaningful when true. Real literals are JSON numbers, or `null` for non-finite values such as overflowing literals.
- Version 16 real literals with an `f` or `F` suffix include `"type":"float"` and a value rounded directly to binary32. Unsuffixed real literals retain their version 15 representation.
- A member expression has a `through_pointer` boolean: true for `->`, false for `.`.
- A `sizeof` expression has `target_type` and `operand` fields; one is null and the other holds the parsed type or expression. The semantic size is not serialized.
- A compound literal is represented as a `cast` whose operand is an
  `array_init` expression; its checked initializer plan is not serialized.
- Declarations are `variable`, `function`, `struct`, or `type_alias`. A `type_alias` has `name` and the expanded `type`; aliases do not declare storage. Variable and function declarations have `static` and `extern` booleans recording storage-class specifiers, including those applied by a group. Variable declarations include `dynamic_array` (always false, retained for format compatibility), an `array_size` expression or null, and an `initializer` object. Function declarations include a `noreturn` boolean, parameters, and a nullable body (null means prototype).
- Supported identifier-list definitions are normalized to the same parameter
  representation as prototype-style definitions; the original spelling is not
  retained. Their supported parameter types are specified in the language
  reference.
- A file-scope inline `typedef struct` is serialized as a `struct` declaration followed by its `type_alias` declarations, in source order.
- Structure declarations have an `anonymous` boolean. Unnamed structures have compiler-generated, non-source `name` identifiers used by their types. Nested and block-scope unnamed definitions are included as declarations in the document. An anonymous member is a variable declaration with `anonymous_member: true` and a null `name`; its type identifies the nested structure. Version 43 adds an optional `pack_alignment` field to packed structure and union definitions.
- Statements are `block`, `declaration`, `expression`, `if`, `switch`, `for`, `loop`, `do_while`, `break`, `continue`, `return`, `goto`, or `label`. Historical output used a `dim` statement kind, which current source parsing cannot emit. Switch statements contain an `expression` and an ordered `cases` array; each case has a nullable `value` (null for `default`) and a `statements` array.
- Source `while` statements retain the versioned statement kind `loop`.
- String literal values are JSON-escaped text with a separate `length` field. The bytes are the lexer/parser's current literal payload; escape-sequence decoding is not performed by this serialization layer. Adjacent literal concatenation can pad octal escapes or replace a terminal hexadecimal escape with an octal escape to preserve token boundaries.

The stream is suitable for `lexer | parser | semantic-analysis` pipelines. Diagnostics belong on standard error; only the AST document is written to standard output. The parser exits nonzero on invalid token streams, syntax errors, allocation failure, or output failure.

The parser selects the first applicable version in this order: v49 for narrow
variable-length hex escapes, v48 for wide
string literals, v47 for
non-static inline declarations, v46 for wide character literals, v45 for
message-less static assertions (whose `message` is null), v44 for
`__builtin_offsetof`, v43 for packed
structure or union definitions, v42 for
thread-local declarations, v41 for alignment
specifiers, v40 for scalar
compound literals, v39 for array
compound literals, v38 for
`_Alignof`, v37 for bit-fields, v36 for volatile types, v35 for static
assertions, v34 for nested
array declarators, explicit parameter-array bounds, or nonliteral typedef
bounds, v33 for long
double types or literals, v32 for array
typedefs, v31 for variadic
signatures, v30 for comma
expressions, v29 for `do`
statements, v28 for function
pointer types or unnamed prototype parameters, v27 for unions,
v26 for named
labels, `goto`, or a switch prefix, v25 for
designated initializers, v24 for embedded
enum definitions or block-scope named structure definitions, v23 for enums,
v22 for `_Bool`, v21 for long long types or suffixes, v20 for a
standalone `L` suffix, v19 for explicit signed or unsigned character types,
v18 for short types, v17 for fixed arrays,
v16 for `float` types or `f`-suffixed literals, and otherwise v15. An AST
version does not uniquely determine the emitted IR version; storage features
and the selected numeric mode also affect IR selection.

Version 15 adds unsigned type kinds and integer literal suffix and range metadata. Version 14 added `array_size` to variable declarations. Version 13 added `sizeof` expressions. Version 12 added anonymous structure and member metadata. Version 11 added the `power` binary operator. Version 10 added `type_alias` declarations and expands their uses to underlying types. Version 9 added the variable declaration's `extern` boolean. Version 8 added the function declaration's `extern` boolean. Version 7 added the declaration-level `static` boolean. Version 6 added the per-type `const` qualifier. This is a versioned public interchange contract. Consumers must reject unsupported versions.

`const { ... };`, `static { ... };`, and `static const { ... };` declaration
groups are flattened into ordinary qualified variable declarations in source
order. They do not introduce AST block nodes or lexical scopes.
