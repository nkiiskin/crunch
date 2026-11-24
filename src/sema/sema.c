#include "sema/sema.h"
#include "common/constant_expr.h"
#include "common/runtime_declaration.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct OwnedType OwnedType;
typedef struct ProcessedEnum ProcessedEnum;
typedef struct OwnedInitializerPlan OwnedInitializerPlan;
struct OwnedInitializerPlan {
    InitializerPlan plan;
    OwnedInitializerPlan *next;
};
struct ProcessedEnum {
    const Decl *declaration;
    ProcessedEnum *next;
};
struct OwnedType {
    Type value;
    OwnedType *next;
};
struct Symbol {
    const char *name;
    size_t length;
    SemanticSymbolKind kind;
    SourceLocation location;
    Decl *declaration;
    Parameter *parameter;
    Type *type;
    bool defined;
    bool tentative;
    bool internal;
    bool used;
    bool warn_unused;
    bool function_name;
    size_t alignment;
    int32_t enum_value;
    Symbol *next;
};
struct Scope {
    Symbol *symbols;
    Scope *parent;
};
struct SemanticModel {
    SemanticDiagnostic *diagnostics;
    size_t diagnostic_count;
    size_t diagnostic_capacity;
    Symbol *globals;
    Symbol *structures;
    size_t global_count;
    OwnedType *owned_types;
    ProcessedEnum *processed_enums;
    OwnedInitializerPlan *initializer_plans;
};
typedef struct FunctionLabel FunctionLabel;
typedef struct VlaJump VlaJump;
struct VlaJump {
    uint64_t active[8];
    SourceLocation location;
    VlaJump *next;
};
struct FunctionLabel {
    const char *name;
    size_t id;
    SourceLocation reference_location;
    bool defined;
    bool referenced;
    uint64_t active_vlas[8];
    VlaJump *jumps;
    FunctionLabel *next;
};
typedef struct {
    SemanticModel *model;
    AstProgram *program;
    Scope *scope;
    unsigned break_depth;
    unsigned loop_depth;
    unsigned expression_depth;
    unsigned statement_depth;
    unsigned unevaluated_depth;
    size_t unsigned_uses;
    size_t float_uses;
    size_t source_offset;
    bool suppress_unsigned_types;
    bool suppress_float_types;
    bool skip_evaluation_checks;
    bool warn_unused_variable;
    bool warn_case_fall_through;
    bool recursion_limit_reported;
    bool out_of_memory;
    bool at_file_scope;
    bool initializer_inference;
    bool allow_variable_array_bounds;
    Type *return_type;
    Decl *current_function;
    Decl *current_structure;
    Symbol *last_named_parameter;
    FunctionLabel *labels;
    size_t label_count;
    unsigned vla_count;
} Analyzer;

enum { SEMA_MAX_RECURSION_DEPTH = 64 };

static Type t_byte = {TYPE_BYTE, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_const_byte = {TYPE_BYTE, NULL, NULL, true, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_void = {TYPE_VOID, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_int = {TYPE_INT, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_long = {TYPE_LONG, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_long_long = {TYPE_LONG_LONG, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_unsigned_int = {TYPE_UNSIGNED_INT, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_unsigned_long = {TYPE_UNSIGNED_LONG, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_unsigned_long_long = {TYPE_UNSIGNED_LONG_LONG, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_real = {TYPE_REAL, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_float = {TYPE_FLOAT, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_long_double = {TYPE_LONG_DOUBLE, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_byte_pointer = {TYPE_POINTER, &t_byte, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_int_pointer = {TYPE_POINTER, &t_int, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type t_void_pointer = {TYPE_POINTER, &t_void, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
static Type *owned_pointer_type(Analyzer *a, Type *pointee);
static bool function_compatible(const Decl *a, const Decl *b);
static bool linked_declarations_compatible(const Decl *previous, const Decl *d);
static Type *check_expr(Analyzer *a, Expr *e);

static size_t infer_structure_array_count(Analyzer *, Type *, Expr *);
static void resolve_fixed_array(Analyzer *, Decl *, bool, bool);
static bool has_designator(const Expr *, unsigned);
static void check_initializer(Analyzer *, Decl *);
static void check_scalar_compound_initializer(Analyzer *, Type *, Expr *);
static void check_file_compound_constants(Analyzer *, const Expr *, unsigned);
static size_t process_designated_initializer(Analyzer *, Decl *, bool);
static bool contains_bit_field(Analyzer *, const Type *, unsigned);
static bool is_character_kind(TypeKind);

/**
 * @brief Allocates zero-initialized memory and records allocation failure.
 * @param a Analyzer state whose out-of-memory flag is updated.
 * @param size Number of bytes to allocate.
 * @return Allocated zeroed memory, or NULL if allocation fails.
 */
static void *analyzer_alloc(Analyzer *a, size_t size)
{
    void *result = a->out_of_memory ? NULL : calloc(1, size);
    if (!result)
        a->out_of_memory = true;
    return result;
}
/**
 * @brief Checks whether an array element can be initialized by a literal.
 * @param element Array element type.
 * @param literal String expression.
 * @return True for the matching narrow or target wide character type.
 */
static bool string_element_matches(const Type *element, const Expr *literal)
{
    return element && literal && literal->kind == EXPR_STRING &&
           (literal->string_literal_wide ? element->kind == TYPE_INT
                                         : is_character_kind(element->kind));
}

/**
 * @brief Creates a pointer type owned by the analyzer's semantic model.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param pointee Crunch type node being analyzed or emitted.
 * @return Analyzer-owned pointer type, or NULL if allocation fails.
 */
static Type *owned_pointer_type(Analyzer *a, Type *pointee)
{
    OwnedType *node = analyzer_alloc(a, sizeof(*node));
    if (!node)
        return NULL;
    node->value.kind = TYPE_POINTER;
    node->value.pointee = pointee;
    node->next = a->model->owned_types;
    a->model->owned_types = node;
    return &node->value;
}

/**
 * @brief Creates a function type referring to an existing declaration.
 * @param a Analyzer owning the new type.
 * @param signature Borrowed function declaration.
 * @return Function type, or NULL on allocation failure.
 */
static Type *owned_function_type(Analyzer *a, Decl *signature)
{
    OwnedType *node = analyzer_alloc(a, sizeof(*node));
    if (!node)
        return NULL;
    node->value.kind = TYPE_FUNCTION;
    node->value.function_signature = signature;
    node->next = a->model->owned_types;
    a->model->owned_types = node;
    return &node->value;
}

/**
 * @brief Returns an analyzer-owned copy with the requested qualifiers.
 * @param a Analyzer that owns any newly allocated type node.
 * @param source Type to qualify.
 * @param is_const Whether to add const.
 * @param is_volatile Whether to add volatile.
 * @return Qualified type, unchanged source, or NULL on allocation failure.
 */
static Type *qualified_type(Analyzer *a, Type *source, bool is_const, bool is_volatile)
{
    if (!source || ((!is_const || source->is_const) && (!is_volatile || source->is_volatile)))
        return source;
    OwnedType *node = analyzer_alloc(a, sizeof(*node));
    if (!node)
        return NULL;
    node->value = *source;
    node->value.is_const |= is_const;
    node->value.is_volatile |= is_volatile;
    node->next = a->model->owned_types;
    a->model->owned_types = node;
    return &node->value;
}
/**
 * @brief Compares two names using explicit lengths.
 * @param a First identifier string to compare.
 * @param an Length of the first identifier in bytes.
 * @param b Second identifier string to compare.
 * @param bn Length of the second identifier in bytes.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool same_name(const char *a, size_t an, const char *b, size_t bn)
{
    return an == bn && memcmp(a, b, an) == 0;
}
/**
 * @brief Adds a formatted semantic diagnostic at a source location.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param loc Source location associated with the token or diagnostic.
 * @param warning True for a warning; false for an error.
 * @param fmt printf-style format string.
 * @param ap Arguments matching fmt; consumed while formatting the diagnostic.
 * @return No value; appends an owned diagnostic or records allocation failure.
 */
static void add_diagnostic(Analyzer *a, SourceLocation loc, bool warning, const char *fmt,
                           va_list ap)
{
    if (a->out_of_memory)
        return;
    char buffer[512];
    int n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    if (n < 0)
        return;
    if (a->model->diagnostic_count == a->model->diagnostic_capacity) {
        size_t cap = a->model->diagnostic_capacity ? a->model->diagnostic_capacity * 2 : 8;
        if (cap < a->model->diagnostic_capacity || cap > SIZE_MAX / sizeof(SemanticDiagnostic)) {
            a->out_of_memory = true;
            return;
        }
        SemanticDiagnostic *p = realloc(a->model->diagnostics, cap * sizeof(*p));
        if (!p) {
            a->out_of_memory = true;
            return;
        }
        a->model->diagnostics = p;
        a->model->diagnostic_capacity = cap;
    }
    size_t len = strlen(buffer);
    char *message = analyzer_alloc(a, len + 1);
    if (!message)
        return;
    memcpy(message, buffer, len + 1);
    a->model->diagnostics[a->model->diagnostic_count++] =
        (SemanticDiagnostic){loc, message, warning};
}
/**
 * @brief Records an error diagnostic with printf-style formatting.
 * @param a Analyzer receiving the diagnostic.
 * @param loc Source location associated with the error.
 * @param fmt Format string followed by its corresponding variadic arguments.
 * @return No value; appends an error diagnostic unless allocation has failed.
 */
static void report(Analyzer *a, SourceLocation loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    add_diagnostic(a, loc, false, fmt, ap);
    va_end(ap);
}
/**
 * @brief Records a warning diagnostic with printf-style formatting.
 * @param a Analyzer receiving the diagnostic.
 * @param loc Source location associated with the warning.
 * @param fmt Format string followed by its corresponding variadic arguments.
 * @return No value; appends a warning diagnostic unless allocation has failed.
 */
static void warn(Analyzer *a, SourceLocation loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    add_diagnostic(a, loc, true, fmt, ap);
    va_end(ap);
}

/**
 * @brief Reads one decoded byte from a string literal, stopping at its first NUL.
 * @param literal String literal expression to read.
 * @param offset Current raw-byte offset, advanced on success.
 * @return Decoded byte, or -1 at the end of the string.
 */
static int format_byte(const Expr *literal, size_t *offset)
{
    const unsigned char *bytes = literal->as.string.bytes;
    size_t end = literal->as.string.length - 1;
    if (*offset >= end)
        return -1;
    unsigned value = bytes[(*offset)++];
    if (value == '\\' && *offset < end) {
        unsigned c = bytes[(*offset)++];
        if (c >= '0' && c <= '7') {
            value = c - '0';
            for (int digits = 1; digits < 3 && *offset < end &&
                 bytes[*offset] >= '0' && bytes[*offset] <= '7'; ++digits)
                value = value * 8 + (bytes[(*offset)++] - '0');
        } else switch (c) {
        case 'a': value = 7; break;
        case 'b': value = 8; break;
        case 'f': value = 12; break;
        case 'n': value = '\n'; break;
        case 'r': value = '\r'; break;
        case 't': value = '\t'; break;
        case 'v': value = 11; break;
        case 'e': case 'E': value = 27; break;
        case 'x':
            value = 0;
            while (*offset < end && isxdigit(bytes[*offset])) {
                unsigned h = bytes[(*offset)++];
                value = value * 16 + (h <= '9' ? h - '0' : (h | 32) - 'a' + 10);
            }
            break;
        default: value = c; break;
        }
    }
    return value ? (int)value : -1;
}

/**
 * @brief Identifies dynamic string precision in a decoded format literal.
 * @param format String literal to inspect.
 * @return True when a conversion uses %.*s.
 */
static bool uses_dynamic_string_precision(const Expr *format)
{
    size_t offset = 1;
    int c;
    while ((c = format_byte(format, &offset)) >= 0) {
        if (c != '%')
            continue;
        c = format_byte(format, &offset);
        if (c == '%')
            continue;
        while (c == '-' || c == '0' || (c >= '1' && c <= '9'))
            c = format_byte(format, &offset);
        if (c == '.' && format_byte(format, &offset) == '*' &&
            format_byte(format, &offset) == 's')
            return true;
    }
    return false;
}

/**
 * @brief Identifies a literal precision on a binary80 fixed-point conversion.
 * @param format String literal to inspect.
 * @return True when a conversion uses %.NLf or %.Lf.
 */
static bool uses_extended_fixed_precision(const Expr *format)
{
    size_t offset = 1;
    int c;
    while ((c = format_byte(format, &offset)) >= 0) {
        if (c != '%') continue;
        c = format_byte(format, &offset);
        if (c == '%') continue;
        while (c == '-' || c == '0' || (c >= '1' && c <= '9'))
            c = format_byte(format, &offset);
        if (c != '.') continue;
        c = format_byte(format, &offset);
        while (c >= '0' && c <= '9')
            c = format_byte(format, &offset);
        if (c == 'L' && format_byte(format, &offset) == 'f')
            return true;
    }
    return false;
}

/**
 * @brief Warns about argument-count mismatches in supported literal printf formats.
 * @param a Analyzer receiving the warning.
 * @param call Library formatted-output call to inspect.
 * @param format_index Argument index of the format string.
 * @param name Function name used in warnings.
 * @return No value; records a warning when the count differs.
 */
static void check_printf_format_count(Analyzer *a, const Expr *call,
                                      size_t format_index, const char *name)
{
    if (call->as.call.arguments.count <= format_index)
        return;
    const Expr *format = call->as.call.arguments.items[format_index];
    if (!format || format->kind != EXPR_STRING || format->string_literal_wide ||
        format->as.string.length < 2 ||
        !format->as.string.bytes) {
        a->program->contains_dynamic_string_precision = true;
        a->program->contains_extended_fixed_precision = true;
        return;
    }
    if (uses_dynamic_string_precision(format))
        a->program->contains_dynamic_string_precision = true;
    if (uses_extended_fixed_precision(format))
        a->program->contains_extended_fixed_precision = true;
    size_t offset = 1, needed = 0;
    int c;
    while ((c = format_byte(format, &offset)) >= 0) {
        if (c != '%')
            continue;
        c = format_byte(format, &offset);
        if (c == '%')
            continue;
        while (c == '-' || c == '0')
            c = format_byte(format, &offset);
        unsigned width = 0;
        while (c >= '0' && c <= '9') {
            unsigned digit = (unsigned)(c - '0');
            if (width > ((unsigned)INT32_MAX - digit) / 10)
                return;
            width = width * 10 + digit;
            c = format_byte(format, &offset);
        }
        if (c == '.') {
            c = format_byte(format, &offset);
            if (c == '*') {
                if (format_byte(format, &offset) != 's') return;
                a->program->contains_dynamic_string_precision = true;
                needed += 2;
                continue;
            }
            unsigned precision = 0;
            while (c >= '0' && c <= '9') {
                precision = precision * 10 + (unsigned)(c - '0');
                if (precision > 100)
                    return;
                c = format_byte(format, &offset);
            }
            if (c == 'L' && format_byte(format, &offset) == 'f') {
                needed++;
                continue;
            }
            return;
        }
        if (c == 'l') {
            c = format_byte(format, &offset);
            if (c == 'l')
                c = format_byte(format, &offset);
            if (c != 'd' && c != 'u' && c != 'x')
                return;
        } else if (c == 'z') {
            if (format_byte(format, &offset) != 'u')
                return;
        } else if (c != 'd' && c != 'u' && c != 'x' && c != 'f' && c != 's' && c != 'c')
            return;
        needed++;
    }
    size_t supplied = call->as.call.arguments.count - format_index - 1;
    if (needed != supplied)
        warn(a, call->location, "%s format expects %zu value%s but received %zu",
             name, needed, needed == 1 ? "" : "s", supplied);
}
/**
 * @brief Finds a local symbol by name and length.
 * @param list Symbol record or linked symbol list being searched or updated.
 * @param name Identifier name to search for or emit.
 * @param length Length of name in bytes.
 * @return Matching symbol, or NULL when name and length do not match.
 */
static Symbol *find_local(Symbol *list, const char *name, size_t length)
{
    for (Symbol *s = list; s; s = s->next)
        if (same_name(s->name, s->length, name, length))
            return s;
    return NULL;
}
/**
 * @brief Searches lexical scopes for a symbol.
 * @param scope Lexical scope being searched or updated.
 * @param name Identifier name to search for or emit.
 * @param length Length of name in bytes.
 * @return Nearest matching symbol in the scope chain, or NULL when absent.
 */
static Symbol *lookup_scope(Scope *scope, const char *name, size_t length)
{
    for (Scope *s = scope; s; s = s->parent) {
        Symbol *found = find_local(s->symbols, name, length);
        if (found)
            return found;
    }
    return NULL;
}
/**
 * @brief Finds a global symbol by name.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param name Identifier name to search for or emit.
 * @param length Length of name in bytes.
 * @return Matching global symbol, or NULL when absent.
 */
static Symbol *lookup_global(Analyzer *a, const char *name, size_t length)
{
    return find_local(a->model->globals, name, length);
}
/**
 * @brief Finds a structure symbol by name.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param name Identifier name to search for or emit.
 * @param length Length of name in bytes.
 * @return Matching structure symbol, or NULL when absent.
 */
static Symbol *lookup_struct(Analyzer *a, const char *name, size_t length)
{
    Symbol *symbol = find_local(a->model->structures, name, length);
    return symbol && symbol->declaration &&
           symbol->declaration->as.structure.completion_offset <= a->source_offset
               ? symbol : NULL;
}
/**
 * @brief Resolves a structure name from the semantic model.
 * @param context Semantic analyzer at the current source location.
 * @param name Structure name.
 * @return Matching declaration, or NULL.
 */
static const Decl *layout_find_struct(const void *context, const char *name)
{
    Analyzer *analyzer = (Analyzer *)context;
    Symbol *symbol = lookup_struct(analyzer, name, strlen(name));
    return symbol ? symbol->declaration : NULL;
}
/**
 * @brief Returns a declaration name and its length.
 * @param d Declaration node being inspected or emitted.
 * @param length Output parameter receiving the length value.
 * @return Pointer to the identifier stored in the declaration; writes its byte length to length.
 */
static const char *decl_name(const Decl *d, size_t *length)
{
    if (d->kind == DECL_VARIABLE) {
        *length = d->as.variable.name_length;
        return d->as.variable.name;
    }
    if (d->kind == DECL_FUNCTION) {
        *length = d->as.function.name_length;
        return d->as.function.name;
    }
    if (d->kind == DECL_TYPE_ALIAS) {
        *length = d->as.type_alias.name_length;
        return d->as.type_alias.name;
    }
    *length = d->as.structure.name_length;
    return d->as.structure.name;
}
/**
 * @brief Returns the type declared by a variable or function.
 * @param d Declaration node being analyzed or emitted.
 * @return Declared type pointer, or NULL if the declaration does not carry a type.
 */
static Type *decl_type(Decl *d)
{
    if (!d)
        return NULL;
    if (d->kind == DECL_VARIABLE)
        return d->as.variable.type;
    if (d->kind == DECL_FUNCTION)
        return d->as.function.return_type;
    if (d->kind == DECL_TYPE_ALIAS)
        return d->as.type_alias.type;
    return NULL;
}
/**
 * @brief Checks type compatibility, ignoring qualifiers.
 * @param a Crunch type node being inspected or emitted.
 * @param b Crunch type node being inspected or emitted.
 * @return True when the types are compatible; otherwise false.
 */
static bool type_equal(const Type *a, const Type *b)
{
    if (!a || !b)
        return a == b;
    if (a->kind != b->kind)
        return (a->kind == TYPE_ENUM && type_effective_kind(a) == b->kind) ||
               (b->kind == TYPE_ENUM && type_effective_kind(b) == a->kind);
    if (a->kind == TYPE_ARRAY)
        return (a->array_length == b->array_length ||
                (a->array_bound && a->array_bound->variable) ||
                (b->array_bound && b->array_bound->variable)) &&
               type_equal(a->pointee, b->pointee);
    if (a->kind == TYPE_POINTER)
        return type_equal(a->pointee, b->pointee);
    if (a->kind == TYPE_FUNCTION)
        return a->function_signature && b->function_signature &&
               function_compatible(a->function_signature, b->function_signature);
    if (a->kind == TYPE_ENUM)
        return a->array_length == b->array_length;
    if (a->kind == TYPE_STRUCT)
        return a->is_union == b->is_union && a->struct_name && b->struct_name &&
               strcmp(a->struct_name, b->struct_name) == 0;
    return true;
}

/**
 * @brief Compares types including qualification at every pointer level.
 * @param a First type to compare.
 * @param b Second type to compare.
 * @return True when both types and all nested qualifiers match.
 */
static bool qualified_equal(const Type *a, const Type *b)
{
    return type_equal(a, b) && (!a || (a->is_const == b->is_const &&
           a->is_volatile == b->is_volatile &&
           ((a->kind != TYPE_POINTER && a->kind != TYPE_ARRAY) ||
            qualified_equal(a->pointee, b->pointee))));
}

/**
 * @brief Compares identical types, as required for typedef redeclarations.
 * @param a First type to compare.
 * @param b Second type to compare.
 * @return True when the types and their qualifiers are identical.
 */
static bool identical_type(const Type *a, const Type *b)
{
    if (!a || !b)
        return a == b;
    if (a->kind != b->kind || a->is_const != b->is_const ||
        a->is_volatile != b->is_volatile || !type_equal(a, b))
        return false;
    return (a->kind != TYPE_POINTER && a->kind != TYPE_ARRAY) ||
           identical_type(a->pointee, b->pointee);
}

/**
 * @brief Compares value types while ignoring top-level qualification.
 * @param a First type to compare.
 * @param b Second type to compare.
 * @return True when the value types and pointee qualifications match.
 */
static bool value_type_equal(const Type *a, const Type *b)
{
    return type_equal(a, b) && (!a || a->kind != TYPE_POINTER ||
                              qualified_equal(a->pointee, b->pointee));
}

/**
 * @brief Checks whether one pointer type can be assigned to another.
 * @param to Destination pointer type.
 * @param from Source pointer type.
 * @return True when pointee types are compatible and qualification is not discarded.
 */
static bool pointer_assignable(const Type *to, const Type *from)
{
    if (!to || !from || to->kind != TYPE_POINTER || from->kind != TYPE_POINTER ||
        !to->pointee || !from->pointee)
        return false;
    const Type *t = to->pointee, *f = from->pointee;
    if ((t->kind == TYPE_FUNCTION || f->kind == TYPE_FUNCTION) && t->kind != f->kind)
        return false;
    if (t->kind == TYPE_VOID || f->kind == TYPE_VOID)
        return (!f->is_const || t->is_const) && (!f->is_volatile || t->is_volatile);
    if (!type_equal(to, from))
        return false;
    return (!f->is_const || t->is_const) && (!f->is_volatile || t->is_volatile) &&
           ((t->kind != TYPE_POINTER && t->kind != TYPE_ARRAY) ||
            qualified_equal(t->pointee, f->pointee));
}

/**
 * @brief Determines whether a type contains a const-qualified value.
 * @param a Analyzer used to resolve structure declarations.
 * @param t Type to inspect recursively.
 * @param depth Current structure-recursion depth.
 * @return True if the type or one of its structure fields is const-qualified.
 */
static bool read_only(Analyzer *a, const Type *t, unsigned depth)
{
    if (!t)
        return false;
    if (t->is_const)
        return true;
    if (t->kind == TYPE_ARRAY)
        return depth < SEMA_MAX_RECURSION_DEPTH && read_only(a, t->pointee, depth + 1);
    if (t->kind != TYPE_STRUCT || !t->struct_name || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    Symbol *s = lookup_struct(a, t->struct_name, strlen(t->struct_name));
    if (s && s->declaration)
        for (size_t i = 0; i < s->declaration->as.structure.field_count; ++i)
            if (read_only(a, s->declaration->as.structure.fields[i]->as.variable.type, depth + 1))
                return true;
    return false;
}
/**
 * @brief Checks whether a type is a numeric scalar.
 * @param t Crunch type node being inspected or emitted.
 * @return True when the type is a numeric scalar; otherwise false.
 */
static bool is_numeric(const Type *t)
{
    return t && (t->kind == TYPE_BYTE || t->kind == TYPE_BOOL || t->kind == TYPE_SIGNED_CHAR ||
                 t->kind == TYPE_UNSIGNED_CHAR || t->kind == TYPE_SHORT ||
                 t->kind == TYPE_UNSIGNED_SHORT || t->kind == TYPE_INT || t->kind == TYPE_ENUM || t->kind == TYPE_LONG ||
                 t->kind == TYPE_LONG_LONG || t->kind == TYPE_UNSIGNED_LONG_LONG ||
                 t->kind == TYPE_UNSIGNED_INT || t->kind == TYPE_UNSIGNED_LONG ||
                 t->kind == TYPE_REAL || t->kind == TYPE_FLOAT || t->kind == TYPE_LONG_DOUBLE);
}

/**
 * @brief Identifies a floating-point type.
 * @param kind Type kind to inspect.
 * @return True for a floating-point type.
 */
static bool is_floating(TypeKind kind)
{
    return kind == TYPE_REAL || kind == TYPE_FLOAT || kind == TYPE_LONG_DOUBLE;
}

/**
 * @brief Identifies an explicitly unsigned integer type.
 * @param kind Type kind to inspect.
 * @return True for an explicitly unsigned integer type.
 */
static bool is_unsigned_kind(TypeKind kind)
{
    return kind == TYPE_BOOL || kind == TYPE_UNSIGNED_CHAR || kind == TYPE_UNSIGNED_SHORT ||
           kind == TYPE_UNSIGNED_INT || kind == TYPE_UNSIGNED_LONG ||
           kind == TYPE_UNSIGNED_LONG_LONG;
}
/**
 * @brief Identifies the new character and short types subject to integer promotion.
 * @param kind Type kind to inspect.
 * @return True for signed or unsigned character and short types.
 */
static bool is_promoted_small_kind(TypeKind kind)
{
    return kind == TYPE_BOOL || kind == TYPE_SIGNED_CHAR || kind == TYPE_UNSIGNED_CHAR ||
           kind == TYPE_SHORT || kind == TYPE_UNSIGNED_SHORT;
}
/**
 * @brief Identifies the three distinct character types.
 * @param kind Type kind to inspect.
 * @return True for plain, signed, or unsigned char.
 */
static bool is_character_kind(TypeKind kind)
{
    return kind == TYPE_BYTE || kind == TYPE_SIGNED_CHAR || kind == TYPE_UNSIGNED_CHAR;
}
/**
 * @brief Returns a numeric type's widening rank.
 * @param t Crunch type node being inspected or emitted.
 * @return Numeric widening rank (byte and short 0, int 1, long 2, real 3), or -1.
 */
static int numeric_rank(const Type *t)
{
    if (!t)
        return -1;
    switch (t->kind) {
    case TYPE_BYTE:
        return 0;
    case TYPE_SHORT:
    case TYPE_UNSIGNED_SHORT:
        return 0;
    case TYPE_INT:
        return 1;
    case TYPE_LONG:
        return 2;
    case TYPE_REAL:
        return 3;
    case TYPE_LONG_DOUBLE:
        return 4;
    default:
        return -1;
    }
}
/**
 * @brief Returns the common widened type of numeric operands.
 * @param a Crunch type node being analyzed or emitted.
 * @param b Crunch type node being analyzed or emitted.
 * @param c11_numeric Whether plain char undergoes integer promotion.
 * @return Common numeric type, or NULL when either input is not numeric.
 */
static Type *promote(Type *a, Type *b, bool c11_numeric)
{
    if (!is_numeric(a) || !is_numeric(b))
        return NULL;
    if (a->kind == TYPE_ENUM)
        a = type_effective_kind(a) == TYPE_UNSIGNED_INT ? &t_unsigned_int : &t_int;
    if (b->kind == TYPE_ENUM)
        b = type_effective_kind(b) == TYPE_UNSIGNED_INT ? &t_unsigned_int : &t_int;
    if (is_promoted_small_kind(a->kind) || (c11_numeric && a->kind == TYPE_BYTE))
        a = &t_int;
    if (is_promoted_small_kind(b->kind) || (c11_numeric && b->kind == TYPE_BYTE))
        b = &t_int;
    if (a->kind == TYPE_LONG_DOUBLE || b->kind == TYPE_LONG_DOUBLE)
        return &t_long_double;
    if (a->kind == TYPE_REAL || b->kind == TYPE_REAL)
        return &t_real;
    if (a->kind == TYPE_FLOAT || b->kind == TYPE_FLOAT)
        return &t_float;
    if (a->kind == TYPE_UNSIGNED_LONG_LONG || b->kind == TYPE_UNSIGNED_LONG_LONG)
        return &t_unsigned_long_long;
    if (a->kind == TYPE_LONG_LONG || b->kind == TYPE_LONG_LONG)
        return a->kind == TYPE_UNSIGNED_LONG || b->kind == TYPE_UNSIGNED_LONG
                   ? &t_unsigned_long_long : &t_long_long;
    if (a->kind == TYPE_UNSIGNED_LONG || b->kind == TYPE_UNSIGNED_LONG)
        return &t_unsigned_long;
    if (a->kind == TYPE_UNSIGNED_INT || b->kind == TYPE_UNSIGNED_INT)
        return a->kind == TYPE_LONG || b->kind == TYPE_LONG ? &t_long : &t_unsigned_int;
    int ar = numeric_rank(a), br = numeric_rank(b);
    if (ar < 0 || br < 0)
        return NULL;
    int r = ar > br ? ar : br;
    return r == 3 ? &t_real : r == 2 ? &t_long : r == 1 ? &t_int : &t_byte;
}

typedef struct {
    uint64_t bits;
    TypeKind kind;
    bool uses_unsigned;
} IntegerValue;

static bool constant_number(const Expr *e, long double *value);
static bool constant_number_impl(const Expr *e, long double *value, bool precise_integer,
                                 bool runtime_conversions);
static bool numeric_value_fits(const Type *to, long double value);
static bool integer_constant(const Expr *e, IntegerValue *result, unsigned depth);
static bool integer_constant_mode(const Expr *e, IntegerValue *result, unsigned depth,
                                  bool runtime_conversions);

/**
 * @brief Checks whether an unevaluated expression has supported constant-expression syntax.
 * @param e Expression to inspect.
 * @param depth Current recursion depth.
 * @return True when it contains only supported constant forms.
 */
static bool constant_expression_form(const Expr *e, unsigned depth)
{
    if (!e || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    switch (e->kind) {
    case EXPR_INTEGER:
        return !e->integer_literal_out_of_range;
    case EXPR_NAME:
        return e->semantic_enum_constant;
    case EXPR_CHAR:
    case EXPR_REAL:
    case EXPR_ALIGNOF:
        return true;
    case EXPR_OFFSETOF:
        return e->as.offset_of.constant;
    case EXPR_SIZEOF:
        return e->as.size_of.semantic_vla_decl == NULL &&
               e->as.size_of.semantic_vla_parameter == NULL;
    case EXPR_UNARY:
        return (e->as.unary.op == UNARY_PLUS || e->as.unary.op == UNARY_MINUS ||
                e->as.unary.op == UNARY_LOGICAL_NOT || e->as.unary.op == UNARY_BITWISE_NOT) &&
               constant_expression_form(e->as.unary.operand, depth + 1);
    case EXPR_BINARY:
        return e->as.binary.op != BINARY_POWER && e->as.binary.op != BINARY_COMMA &&
               constant_expression_form(e->as.binary.left, depth + 1) &&
               constant_expression_form(e->as.binary.right, depth + 1);
    case EXPR_CONDITIONAL:
        return constant_expression_form(e->as.conditional.condition, depth + 1) &&
               constant_expression_form(e->as.conditional.then_expr, depth + 1) &&
               constant_expression_form(e->as.conditional.else_expr, depth + 1);
    case EXPR_CAST:
        return is_numeric(e->as.cast.target_type) &&
               constant_expression_form(e->as.cast.operand, depth + 1);
    default:
        return false;
    }
}

/**
 * @brief Evaluates the truth value of a constant scalar expression.
 * @param e Checked scalar expression.
 * @param value Receives the truth value.
 * @param depth Current recursion depth.
 * @param runtime_conversions Use the backend's results for invalid numeric conversions.
 * @return True when the expression can be evaluated.
 */
static bool constant_truth(const Expr *e, bool *value, unsigned depth, bool runtime_conversions)
{
    if (!e || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    if (is_floating(e->semantic_type)) {
        long double real;
        if (!constant_number_impl(e, &real, e->semantic_float || runtime_conversions,
                                  runtime_conversions))
            return false;
        *value = real != 0;
        return true;
    }
    IntegerValue integer;
    if (!integer_constant_mode(e, &integer, depth, runtime_conversions))
        return false;
    *value = integer.bits != 0;
    return true;
}

/**
 * @brief Converts integer bits to the canonical representation of a type.
 * @param bits Source bits, including sign extension for signed values.
 * @param kind Destination integer type.
 * @return Canonical destination bits.
 */
static uint64_t integer_bits(uint64_t bits, TypeKind kind)
{
    if (kind == TYPE_BOOL)
        return bits != 0;
    if (kind == TYPE_BYTE || kind == TYPE_UNSIGNED_CHAR)
        return bits & UINT64_C(0xff);
    if (kind == TYPE_SIGNED_CHAR) {
        bits &= UINT64_C(0xff);
        return bits > INT8_MAX ? bits | UINT64_C(0xffffffffffffff00) : bits;
    }
    if (kind == TYPE_UNSIGNED_SHORT)
        return bits & UINT64_C(0xffff);
    if (kind == TYPE_SHORT) {
        bits &= UINT64_C(0xffff);
        return bits > INT16_MAX ? bits | UINT64_C(0xffffffffffff0000) : bits;
    }
    if (kind == TYPE_UNSIGNED_INT)
        return bits & UINT64_C(0xffffffff);
    if (kind == TYPE_INT || kind == TYPE_ENUM) {
        bits &= UINT64_C(0xffffffff);
        if (bits > INT32_MAX)
            bits |= UINT64_C(0xffffffff00000000);
    }
    return bits;
}

/**
 * @brief Interprets a two's-complement bit pattern as a signed 64-bit value.
 * @param bits Canonical integer bits.
 * @return Corresponding signed value.
 */
static int64_t signed_bits(uint64_t bits)
{
    return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
}

/**
 * @brief Checks the backend's floating-to-integer conversion interval.
 * @param value Source floating value.
 * @param source Source floating type.
 * @param target Destination integer type.
 * @param c11_numeric Whether to check the truncated signed value's range.
 * @return True when the conversion is within the selected mode's bounds.
 */
static bool floating_integer_in_range(long double value, TypeKind source, TypeKind target,
                                      bool c11_numeric)
{
    if (!isfinite(value))
        return false;
    if (c11_numeric && (target == TYPE_SIGNED_CHAR || target == TYPE_SHORT ||
                        target == TYPE_INT || target == TYPE_ENUM || target == TYPE_LONG || target == TYPE_LONG_LONG)) {
        long double truncated = truncl(value);
        long double lower = target == TYPE_SIGNED_CHAR ? -128.0L
                            : target == TYPE_SHORT ? -32768.0L
                            : target == TYPE_INT || target == TYPE_ENUM ? -2147483648.0L
                            : -9223372036854775808.0L;
        long double upper = -lower;
        return truncated >= lower && truncated < upper;
    }
    if (target == TYPE_SIGNED_CHAR)
        return value >= -128.0L && value < 128.0L;
    if (target == TYPE_UNSIGNED_CHAR)
        return value > -1.0L && value < 256.0L;
    if (target == TYPE_SHORT)
        return value >= -32768.0L && value < 32768.0L;
    if (target == TYPE_UNSIGNED_SHORT)
        return value > -1.0L && value < 65536.0L;
    if (target == TYPE_BYTE && (source == TYPE_FLOAT || c11_numeric)) {
        return value > -1.0L && value < 256.0L;
    }
    if (target == TYPE_UNSIGNED_INT || target == TYPE_UNSIGNED_LONG ||
        target == TYPE_UNSIGNED_LONG_LONG) {
        long double upper = target == TYPE_UNSIGNED_INT ? 4294967296.0L
                                                        : 18446744073709551616.0L;
        return value > -1.0L && value < upper;
    }
    long double lower = target == TYPE_LONG || target == TYPE_LONG_LONG
                            ? -9223372036854775808.0L : -2147483648.0L;
    long double upper = target == TYPE_LONG || target == TYPE_LONG_LONG
                            ? 9223372036854775808.0L : 2147483648.0L;
    return value >= lower && value < upper;
}

/**
 * @brief Computes the backend's floating-to-integer conversion result.
 * @param value Source floating value.
 * @param source Source floating type.
 * @param target Destination integer type.
 * @param c11_numeric Whether to use C11-mode conversion bounds.
 * @return Canonical integer bits, including the backend's zero result for invalid conversions.
 */
static uint64_t runtime_floating_integer(long double value, TypeKind source, TypeKind target,
                                         bool c11_numeric)
{
    if (!floating_integer_in_range(value, source, target, c11_numeric))
        return 0;
    uint64_t bits = target == TYPE_UNSIGNED_SHORT || target == TYPE_UNSIGNED_INT ||
                    target == TYPE_UNSIGNED_LONG || target == TYPE_UNSIGNED_LONG_LONG
                        ? (uint64_t)value : (uint64_t)(int64_t)value;
    return integer_bits(bits, target);
}

/**
 * @brief Converts a numeric value to round-to-nearest binary32 bits.
 * @param value Numeric value to convert.
 * @return Raw IEEE-754 binary32 bits.
 */
uint32_t sema_binary32_bits(long double value)
{
    if (!isnan(value) && fabsl(value) > FLT_MAX) {
        uint32_t magnitude = fabsl(value) < 0x1.ffffffp127L ? UINT32_C(0x7f7fffff)
                                                            : UINT32_C(0x7f800000);
        return magnitude | (signbit(value) ? UINT32_C(0x80000000) : 0);
    }
    float number = (float)value;
    uint32_t bits;
    memcpy(&bits, &number, sizeof bits);
    return bits;
}

/**
 * @brief Evaluates a typed integer constant expression without signed overflow.
 * @param e Semantically checked expression.
 * @param result Receives the typed value and whether unsigned semantics were used.
 * @param depth Current recursion depth.
 * @param runtime_conversions Use the backend's results for invalid numeric conversions.
 * @return True when the expression can be evaluated.
 */
static bool integer_constant_mode(const Expr *e, IntegerValue *result, unsigned depth,
                                  bool runtime_conversions)
{
    if (!e || !result || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    TypeKind kind = e->semantic_type;
    IntegerValue l, r;
    result->kind = kind;
    result->uses_unsigned = e->semantic_unsigned || is_unsigned_kind(kind);
    switch (e->kind) {
    case EXPR_NAME:
        if (!e->semantic_enum_constant)
            return false;
        result->bits = (uint64_t)(int64_t)e->semantic_enum_value;
        result->kind = TYPE_INT;
        return true;
    case EXPR_INTEGER:
        if (e->integer_literal_out_of_range)
            return false;
        result->bits = integer_bits((e->integer_suffix == INTEGER_SUFFIX_NONE ||
                                     e->integer_suffix == INTEGER_SUFFIX_L ||
                                     e->integer_suffix == INTEGER_SUFFIX_LL) &&
                                    !e->integer_literal_unsigned
                                        ? (uint64_t)e->as.integer_value
                                        : e->unsigned_integer_value, kind);
        result->uses_unsigned |= (e->integer_suffix != INTEGER_SUFFIX_NONE &&
                                  e->integer_suffix != INTEGER_SUFFIX_L &&
                                  e->integer_suffix != INTEGER_SUFFIX_LL) ||
                                 e->integer_literal_unsigned;
        return true;
    case EXPR_CHAR:
        result->bits = (uint64_t)(int64_t)e->as.char_value;
        return true;
    case EXPR_OFFSETOF:
        if (!e->as.offset_of.resolved || !e->as.offset_of.constant)
            return false;
        result->bits = e->as.offset_of.value;
        return true;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (!e->as.size_of.resolved)
            return false;
        result->bits = (uint64_t)e->as.size_of.size;
        return true;
    case EXPR_UNARY:
        if (e->as.unary.op == UNARY_LOGICAL_NOT && e->as.unary.operand &&
            is_floating(e->as.unary.operand->semantic_type)) {
            bool truth;
            if (!constant_truth(e->as.unary.operand, &truth, depth + 1, runtime_conversions))
                return false;
            result->bits = !truth;
            return true;
        }
        if (!integer_constant_mode(e->as.unary.operand, &l, depth + 1, runtime_conversions))
            return false;
        result->uses_unsigned |= l.uses_unsigned;
        switch (e->as.unary.op) {
        case UNARY_PLUS: result->bits = integer_bits(l.bits, kind); return true;
        case UNARY_MINUS: result->bits = integer_bits(0 - l.bits, kind); return true;
        case UNARY_BITWISE_NOT: result->bits = integer_bits(~l.bits, kind); return true;
        case UNARY_LOGICAL_NOT: result->bits = !l.bits; return true;
        default: return false;
        }
    case EXPR_CAST:
        if (kind == TYPE_BOOL) {
            bool truth;
            if (!constant_truth(e->as.cast.operand, &truth, depth + 1, runtime_conversions))
                return false;
            result->bits = truth;
            return true;
        }
        if (kind != TYPE_BYTE && kind != TYPE_SIGNED_CHAR &&
            kind != TYPE_SHORT && kind != TYPE_INT && kind != TYPE_ENUM && kind != TYPE_LONG &&
            kind != TYPE_LONG_LONG &&
            !is_unsigned_kind(kind))
            return false;
        if (e->as.cast.operand && is_floating(e->as.cast.operand->semantic_type)) {
            long double real;
            if (!constant_number_impl(e->as.cast.operand, &real,
                                      e->as.cast.operand->semantic_float || runtime_conversions,
                                      runtime_conversions))
                return false;
            if (runtime_conversions) {
                result->bits = runtime_floating_integer(real, e->as.cast.operand->semantic_type,
                                                        kind, e->semantic_c11_numeric);
                return true;
            }
            if (!floating_integer_in_range(real, e->as.cast.operand->semantic_type, kind,
                                           e->semantic_c11_numeric))
                return false;
            result->bits = runtime_floating_integer(real, e->as.cast.operand->semantic_type, kind,
                                                    e->semantic_c11_numeric);
            return true;
        }
        if (!integer_constant_mode(e->as.cast.operand, &l, depth + 1, runtime_conversions))
            return false;
        result->uses_unsigned |= l.uses_unsigned;
        result->bits = integer_bits(l.bits, kind);
        return true;
    case EXPR_CONDITIONAL:
        if (is_floating(kind))
            return false;
        if (is_floating(e->as.conditional.condition->semantic_type)) {
            long double condition;
            if (!constant_number_impl(e->as.conditional.condition, &condition,
                                      e->as.conditional.condition->semantic_float ||
                                          runtime_conversions,
                                      runtime_conversions))
                return false;
            l.bits = condition != 0;
            l.uses_unsigned = e->as.conditional.condition->semantic_unsigned;
        } else if (!integer_constant_mode(e->as.conditional.condition, &l, depth + 1,
                                          runtime_conversions))
            return false;
        if (e->semantic_c11_numeric) {
            const Expr *selected = l.bits ? e->as.conditional.then_expr
                                          : e->as.conditional.else_expr;
            const Expr *skipped = l.bits ? e->as.conditional.else_expr
                                         : e->as.conditional.then_expr;
            if (!constant_expression_form(skipped, depth + 1) ||
                !integer_constant_mode(selected, &r, depth + 1, runtime_conversions))
                return false;
            result->uses_unsigned |= l.uses_unsigned || r.uses_unsigned;
            result->bits = integer_bits(r.bits, kind);
            return true;
        }
        IntegerValue then_value, else_value;
        if (!integer_constant_mode(e->as.conditional.then_expr, &then_value, depth + 1,
                                   runtime_conversions) ||
            !integer_constant_mode(e->as.conditional.else_expr, &else_value, depth + 1,
                                   runtime_conversions))
            return false;
        r = l.bits ? then_value : else_value;
        result->uses_unsigned |= l.uses_unsigned || r.uses_unsigned ||
            is_unsigned_kind(e->as.conditional.then_expr->semantic_type) ||
            is_unsigned_kind(e->as.conditional.else_expr->semantic_type);
        result->bits = integer_bits(r.bits, kind);
        return true;
    case EXPR_BINARY: {
        BinaryOp op = e->as.binary.op;
        if (op == BINARY_LOGICAL_AND || op == BINARY_LOGICAL_OR) {
            bool left, right;
            if (!constant_truth(e->as.binary.left, &left, depth + 1, runtime_conversions))
                return false;
            if (e->semantic_c11_numeric &&
                ((op == BINARY_LOGICAL_AND && !left) || (op == BINARY_LOGICAL_OR && left))) {
                if (!constant_expression_form(e->as.binary.right, depth + 1))
                    return false;
                result->bits = left;
                return true;
            }
            if (!constant_truth(e->as.binary.right, &right, depth + 1, runtime_conversions))
                return false;
            result->bits = op == BINARY_LOGICAL_AND ? left && right : left || right;
            return true;
        }
        if ((is_floating(e->as.binary.left->semantic_type) ||
             is_floating(e->as.binary.right->semantic_type)) &&
            op >= BINARY_EQUAL && op <= BINARY_GREATER_EQUAL) {
            long double left, right;
            if (!constant_number_impl(e->as.binary.left, &left, true, runtime_conversions) ||
                !constant_number_impl(e->as.binary.right, &right, true, runtime_conversions))
                return false;
            bool extended = e->as.binary.left->semantic_type == TYPE_LONG_DOUBLE ||
                            e->as.binary.right->semantic_type == TYPE_LONG_DOUBLE;
            bool single = !extended && e->as.binary.left->semantic_type != TYPE_REAL &&
                          e->as.binary.right->semantic_type != TYPE_REAL;
            long double x = extended ? left : single ? (float)left : (double)left;
            long double y = extended ? right : single ? (float)right : (double)right;
            switch (op) {
            case BINARY_EQUAL: result->bits = x == y; return true;
            case BINARY_NOT_EQUAL: result->bits = x != y; return true;
            case BINARY_LESS: result->bits = x < y; return true;
            case BINARY_LESS_EQUAL: result->bits = x <= y; return true;
            case BINARY_GREATER: result->bits = x > y; return true;
            case BINARY_GREATER_EQUAL: result->bits = x >= y; return true;
            default: return false;
            }
        }

        if (!integer_constant_mode(e->as.binary.left, &l, depth + 1, runtime_conversions))
            return false;
        if (!integer_constant_mode(e->as.binary.right, &r, depth + 1, runtime_conversions))
            return false;
        result->uses_unsigned |= l.uses_unsigned || r.uses_unsigned;
        if (op == BINARY_SHIFT_LEFT || op == BINARY_SHIFT_RIGHT) {
            unsigned width = (kind == TYPE_LONG || kind == TYPE_UNSIGNED_LONG ||
                              kind == TYPE_LONG_LONG || kind == TYPE_UNSIGNED_LONG_LONG) ? 64 :
                             kind == TYPE_BYTE ? 8 : 32;
            if ((!is_unsigned_kind(r.kind) && signed_bits(r.bits) < 0) || r.bits >= width)
                return false;
            uint64_t bits = integer_bits(l.bits, kind);
            if (op == BINARY_SHIFT_LEFT)
                result->bits = integer_bits(bits << (unsigned)r.bits, kind);
            else {
                uint64_t shifted = bits >> (unsigned)r.bits;
                if (kind != TYPE_BYTE && !is_unsigned_kind(kind) &&
                    signed_bits(bits) < 0 && r.bits)
                    shifted |= UINT64_MAX << (64U - (unsigned)r.bits);
                result->bits = integer_bits(shifted, kind);
            }
            return true;
        }
        Type lt = {l.kind, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false}, rt = {r.kind, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
        Type *common = promote(&lt, &rt, e->semantic_c11_numeric);
        if (!common || is_floating(common->kind))
            return false;
        uint64_t x = integer_bits(l.bits, common->kind);
        uint64_t y = integer_bits(r.bits, common->kind);
        int64_t sx = signed_bits(x), sy = signed_bits(y);
        bool uns = is_unsigned_kind(common->kind) || common->kind == TYPE_BYTE;
        switch (op) {
        case BINARY_ADD: result->bits = x + y; break;
        case BINARY_SUBTRACT: result->bits = x - y; break;
        case BINARY_MULTIPLY: result->bits = x * y; break;
        case BINARY_DIVIDE:
        case BINARY_REMAINDER:
            if (!y || (!uns && sy == -1 &&
                       ((common->kind == TYPE_INT && sx == INT32_MIN) ||
                        ((common->kind == TYPE_LONG || common->kind == TYPE_LONG_LONG) &&
                         sx == INT64_MIN))))
                return false;
            result->bits = uns ? (op == BINARY_DIVIDE ? x / y : x % y) :
                (uint64_t)(op == BINARY_DIVIDE ? sx / sy : sx % sy);
            break;
        case BINARY_BITWISE_AND: result->bits = x & y; break;
        case BINARY_BITWISE_OR: result->bits = x | y; break;
        case BINARY_BITWISE_XOR: result->bits = x ^ y; break;
        case BINARY_EQUAL: result->bits = x == y; return true;
        case BINARY_NOT_EQUAL: result->bits = x != y; return true;
        case BINARY_LESS: result->bits = uns ? x < y : sx < sy; return true;
        case BINARY_LESS_EQUAL: result->bits = uns ? x <= y : sx <= sy; return true;
        case BINARY_GREATER: result->bits = uns ? x > y : sx > sy; return true;
        case BINARY_GREATER_EQUAL: result->bits = uns ? x >= y : sx >= sy; return true;
        default: return false;
        }
        result->bits = integer_bits(result->bits, kind);
        return true;
    }
    default:
        return false;
    }
}

/**
 * @brief Evaluates an integer constant without accepting invalid conversions.
 * @param e Semantically checked expression.
 * @param result Receives the typed integer value.
 * @param depth Current recursion depth.
 * @return True when the expression can be evaluated.
 */
static bool integer_constant(const Expr *e, IntegerValue *result, unsigned depth)
{
    return integer_constant_mode(e, result, depth, false);
}

/**
 * @brief Evaluates a checked integer constant for a later compiler stage.
 * @param expression Semantically checked expression.
 * @param bits Receives the canonical integer bits.
 * @param kind Receives the expression's type.
 * @return True if the expression is an evaluable integer constant.
 */
bool sema_integer_constant(const Expr *expression, uint64_t *bits, TypeKind *kind)
{
    IntegerValue result;
    if (!bits || !kind || !integer_constant(expression, &result, 0))
        return false;
    *bits = result.bits;
    *kind = result.kind;
    return true;
}

/**
 * @brief Diagnoses an invalid constant shift count for an unsigned-involved shift.
 * @param a Analyzer receiving diagnostics.
 * @param location Location of the shift operator.
 * @param left Promoted left operand type.
 * @param right Checked right operand expression.
 * @return No value; reports an invalid constant count.
 */
static void check_unsigned_shift_count(Analyzer *a, SourceLocation location, const Type *left,
                                       const Expr *right)
{
    if (a->skip_evaluation_checks)
        return;
    unsigned width = left->kind == TYPE_LONG || left->kind == TYPE_UNSIGNED_LONG ||
                     left->kind == TYPE_LONG_LONG || left->kind == TYPE_UNSIGNED_LONG_LONG ? 64 : 32;
    IntegerValue count;
    if (integer_constant(right, &count, 0) &&
        ((!is_unsigned_kind(count.kind) && signed_bits(count.bits) < 0) || count.bits >= width))
        report(a, location, "shift count is outside the result type width");
}

/**
 * @brief Diagnoses a constant zero divisor for an unsigned-involved operation.
 * @param a Analyzer receiving diagnostics.
 * @param location Location of the operator.
 * @param right Checked divisor expression.
 * @param remainder True for remainder, false for division.
 * @return No value; reports a zero divisor.
 */
static void check_unsigned_divisor(Analyzer *a, SourceLocation location, const Expr *right,
                                   bool remainder)
{
    if (a->skip_evaluation_checks)
        return;
    IntegerValue divisor;
    if (integer_constant(right, &divisor, 0) && !divisor.bits)
        report(a, location, remainder ? "integer remainder by zero" : "integer division by zero");
}

/**
 * @brief Recognizes an integer constant zero, including pointer casts of zero.
 * @param e Expression to evaluate as a null pointer constant.
 * @return True when the expression is a supported constant zero; otherwise false.
 */
static bool is_null_literal(const Expr *e)
{
    if (e && e->kind == EXPR_CAST && e->as.cast.target_type &&
        e->as.cast.target_type->kind == TYPE_POINTER)
        return is_null_literal(e->as.cast.operand);
    IntegerValue value;
    if (integer_constant(e, &value, 0) && value.uses_unsigned)
        return value.bits == 0;
    return constant_expr_is_zero(e);
}
/**
 * @brief Evaluates a supported numeric constant expression.
 * @param e Expression to evaluate.
 * @param value Output receiving the result on success.
 * @param precise_integer Apply integer arithmetic within mixed numeric expressions.
 * @param runtime_conversions Use the backend's results for invalid numeric conversions.
 * @return True when e can be evaluated as a numeric constant; otherwise false.
 */
static bool constant_number_impl(const Expr *e, long double *value, bool precise_integer,
                                 bool runtime_conversions)
{
    if (!e || !value)
        return false;
    IntegerValue integer;
    if (integer_constant_mode(e, &integer, 0, runtime_conversions) &&
        (integer.uses_unsigned || e->semantic_float || precise_integer || e->kind == EXPR_CAST)) {
        *value = is_unsigned_kind(integer.kind) || integer.kind == TYPE_BYTE
                     ? (long double)integer.bits : (long double)signed_bits(integer.bits);
        return true;
    }
    switch (e->kind) {
    case EXPR_NAME:
        if (!e->semantic_enum_constant)
            return false;
        *value = e->semantic_enum_value;
        return true;
    case EXPR_OFFSETOF:
        if (!e->as.offset_of.resolved || !e->as.offset_of.constant)
            return false;
        *value = (long double)e->as.offset_of.value;
        return true;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (!e->as.size_of.resolved)
            return false;
        *value = (long double)e->as.size_of.size;
        return true;
    case EXPR_INTEGER:
        if (e->integer_literal_out_of_range ||
            (e->integer_suffix != INTEGER_SUFFIX_NONE && e->integer_suffix != INTEGER_SUFFIX_L &&
             e->integer_suffix != INTEGER_SUFFIX_LL) ||
            e->integer_literal_unsigned)
            return false;
        *value = (long double)e->as.integer_value;
        return true;
    case EXPR_REAL:
        if (e->real_literal_extended_text)
            *value = strtold(e->real_literal_extended_text, NULL);
        else
            *value = (long double)e->as.real_value;
        return true;
    case EXPR_CHAR:
        *value = (long double)e->as.char_value;
        return true;
    case EXPR_UNARY: {
        long double v;
        if (!constant_number_impl(e->as.unary.operand, &v, precise_integer, runtime_conversions))
            return false;
        if (e->as.unary.op == UNARY_PLUS) {
            *value = v;
            return true;
        }
        if (e->as.unary.op == UNARY_MINUS) {
            *value = -v;
            return true;
        }
        return false;
    }
    case EXPR_BINARY: {
        long double l, r;
        if (!constant_number_impl(e->as.binary.left, &l, precise_integer, runtime_conversions) ||
            !constant_number_impl(e->as.binary.right, &r, precise_integer, runtime_conversions))
            return false;
        if (e->semantic_type == TYPE_FLOAT) {
            float left = (float)l, right = (float)r;
            switch (e->as.binary.op) {
            case BINARY_ADD: *value = (float)(left + right); return true;
            case BINARY_SUBTRACT: *value = (float)(left - right); return true;
            case BINARY_MULTIPLY: *value = (float)(left * right); return true;
            case BINARY_DIVIDE: *value = (float)(left / right); return true;
            default: return false;
            }
        }
        if (e->semantic_type == TYPE_REAL) {
            double left = (double)l, right = (double)r;
            switch (e->as.binary.op) {
            case BINARY_ADD: *value = left + right; return true;
            case BINARY_SUBTRACT: *value = left - right; return true;
            case BINARY_MULTIPLY: *value = left * right; return true;
            case BINARY_DIVIDE: *value = left / right; return true;
            default: return false;
            }
        }
        if (e->semantic_type == TYPE_LONG_DOUBLE) {
            switch (e->as.binary.op) {
            case BINARY_ADD: *value = l + r; return true;
            case BINARY_SUBTRACT: *value = l - r; return true;
            case BINARY_MULTIPLY: *value = l * r; return true;
            case BINARY_DIVIDE: *value = l / r; return true;
            default: return false;
            }
        }
        switch (e->as.binary.op) {
        case BINARY_ADD:
            *value = l + r;
            return true;
        case BINARY_SUBTRACT:
            *value = l - r;
            return true;
        case BINARY_MULTIPLY:
            *value = l * r;
            return true;
        case BINARY_DIVIDE:
            if (r == 0)
                return false;
            *value = l / r;
            return true;
        case BINARY_REMAINDER:
            return false;
        default:
            return false;
        }
    }
    case EXPR_CAST:
        if (!constant_number_impl(e->as.cast.operand, value, precise_integer, runtime_conversions))
            return false;
        if (e->semantic_type == TYPE_FLOAT) {
            uint32_t bits = sema_binary32_bits(*value);
            float number;
            memcpy(&number, &bits, sizeof number);
            *value = number;
        }
        else if (!is_floating(e->semantic_type) && e->as.cast.operand &&
                 is_floating(e->as.cast.operand->semantic_type)) {
            if (runtime_conversions) {
                uint64_t bits = runtime_floating_integer(*value, e->as.cast.operand->semantic_type,
                                                         e->semantic_type, e->semantic_c11_numeric);
                *value = is_unsigned_kind(e->semantic_type) || e->semantic_type == TYPE_BYTE
                             ? (long double)bits : (long double)signed_bits(bits);
                return true;
            }
            if (!floating_integer_in_range(*value, e->as.cast.operand->semantic_type,
                                           e->semantic_type, e->semantic_c11_numeric))
                return false;
            *value = is_unsigned_kind(e->semantic_type) ? (long double)(uint64_t)*value
                                                       : (long double)(int64_t)*value;
        }
        if ((e->semantic_unsigned || e->semantic_float) && e->semantic_type == TYPE_REAL)
            *value = (double)*value;
        return true;
    case EXPR_CONDITIONAL: {
        long double condition, then_value, else_value;
        if (e->semantic_c11_numeric) {
            if (!constant_number_impl(e->as.conditional.condition, &condition, precise_integer,
                                      runtime_conversions))
                return false;
            const Expr *selected = condition != 0 ? e->as.conditional.then_expr
                                                  : e->as.conditional.else_expr;
            const Expr *skipped = condition != 0 ? e->as.conditional.else_expr
                                                 : e->as.conditional.then_expr;
            if (!constant_expression_form(skipped, 0) ||
                !constant_number_impl(selected, value, precise_integer, runtime_conversions))
                return false;
            if (e->semantic_type == TYPE_FLOAT)
                *value = (float)*value;
            else if (e->semantic_type == TYPE_REAL)
                *value = (double)*value;
            return true;
        }
        if ((!e->semantic_unsigned && !e->semantic_float) ||
            !constant_number_impl(e->as.conditional.condition, &condition, precise_integer,
                                  runtime_conversions) ||
            !constant_number_impl(e->as.conditional.then_expr, &then_value, precise_integer,
                                  runtime_conversions) ||
            !constant_number_impl(e->as.conditional.else_expr, &else_value, precise_integer,
                                  runtime_conversions))
            return false;
        *value = condition != 0 ? then_value : else_value;
        if (e->semantic_type == TYPE_FLOAT)
            *value = (float)*value;
        if (e->semantic_type == TYPE_REAL)
            *value = (double)*value;
        return true;
    }
    default:
        return false;
    }
}

/**
 * @brief Evaluates a numeric constant using the expression's semantic context.
 * @param e Checked expression to evaluate.
 * @param value Receives the numeric value.
 * @return True when the expression is an evaluable numeric constant.
 */
static bool constant_number(const Expr *e, long double *value)
{
    return constant_number_impl(e, value,
                                e && (e->semantic_float || e->semantic_c11_numeric), false);
}

/**
 * @brief Evaluates a checked numeric constant using semantic conversion rules.
 * @param expression Checked expression to evaluate.
 * @param value Receives the numeric value.
 * @return True when the expression is an evaluable numeric constant.
 */
bool sema_numeric_constant(const Expr *expression, long double *value)
{
    return constant_number_impl(expression, value, true, true);
}

/**
 * @brief Evaluates a numeric constant converted to an integer storage type.
 * @param expression Semantically checked expression.
 * @param target Destination integer type.
 * @param bits Receives the destination's canonical integer bits.
 * @return True when the conversion can be evaluated.
 */
bool sema_constant_integer_conversion(const Expr *expression, TypeKind target, uint64_t *bits)
{
    if (!expression || !bits ||
        (target != TYPE_BYTE && target != TYPE_SIGNED_CHAR &&
         target != TYPE_SHORT && target != TYPE_INT && target != TYPE_ENUM && target != TYPE_LONG &&
         target != TYPE_LONG_LONG &&
         !is_unsigned_kind(target)))
        return false;
    IntegerValue integer;
    if (integer_constant_mode(expression, &integer, 0, true)) {
        *bits = integer_bits(integer.bits, target);
        return true;
    }
    long double value;
    if (!is_floating(expression->semantic_type) ||
        !constant_number_impl(expression, &value, true, true))
        return false;
    if (target == TYPE_BOOL) {
        *bits = value != 0;
        return true;
    }
    *bits = runtime_floating_integer(value, expression->semantic_type, target,
                                     expression->semantic_c11_numeric);
    return true;
}

/**
 * @brief Checks whether a value fits a destination numeric type.
 * @param to Crunch type node being inspected or emitted.
 * @param value Value to inspect, encode, or store.
 * @return True when the constant value is representable by the target type; otherwise false.
 */
static bool numeric_value_fits(const Type *to, long double value)
{
    if (!to || !isfinite(value))
        return false;
    if (to->kind == TYPE_LONG_DOUBLE)
        return true;
    if (to->kind == TYPE_REAL)
        return value >= -DBL_MAX && value <= DBL_MAX;
    if (to->kind == TYPE_FLOAT)
        return value >= -FLT_MAX && value <= FLT_MAX;
    switch (type_effective_kind(to)) {
    case TYPE_BYTE:
    case TYPE_UNSIGNED_CHAR:
        return value > -1.0L && value < 256.0L;
    case TYPE_SIGNED_CHAR:
        return value > -129.0L && value < 128.0L;
    case TYPE_SHORT:
        return value > -32769.0L && value < 32768.0L;
    case TYPE_UNSIGNED_SHORT:
        return value > -1.0L && value < 65536.0L;
    case TYPE_INT:
    case TYPE_ENUM:
        return value > -2147483649.0L && value < 2147483648.0L;
    case TYPE_LONG:
    case TYPE_LONG_LONG:
        return value > -9223372036854775809.0L && value < 9223372036854775808.0L;
    case TYPE_UNSIGNED_INT:
        return value > -1.0L && value < 4294967296.0L;
    case TYPE_UNSIGNED_LONG:
    case TYPE_UNSIGNED_LONG_LONG:
        return value > -1.0L && value < 18446744073709551616.0L;
    default:
        return true;
    }
}
/**
 * @brief Checks whether numeric constant expressions contain finite real literals.
 * @param e Expression to inspect.
 * @return True when every real literal in the supported expression is finite.
 */
static bool finite_real_literals(const Expr *e)
{
    if (!e)
        return false;
    switch (e->kind) {
    case EXPR_REAL:
        return e->real_literal_extended_text
                   ? isfinite(strtold(e->real_literal_extended_text, NULL))
                   : isfinite(e->as.real_value);
    case EXPR_UNARY:
        return finite_real_literals(e->as.unary.operand);
    case EXPR_BINARY:
        return finite_real_literals(e->as.binary.left) &&
               finite_real_literals(e->as.binary.right);
    case EXPR_CAST:
        return finite_real_literals(e->as.cast.operand);
    case EXPR_CONDITIONAL:
        return finite_real_literals(e->as.conditional.condition) &&
               finite_real_literals(e->as.conditional.then_expr) &&
               finite_real_literals(e->as.conditional.else_expr);
    default:
        return true;
    }
}
/**
 * @brief Diagnoses a constant value outside its destination type range.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param to Crunch type node being inspected or emitted.
 * @param e Expression node being inspected or emitted.
 * @param context Text input to parse, compare, or serialize.
 * @return No value; the function performs the documented output or state update.
 */
static void check_constant_range(Analyzer *a, const Type *to, const Expr *e, const char *context)
{
    long double value;
    if (to && to->kind == TYPE_BOOL)
        return;
    if (to && e && !is_floating(e->semantic_type) && is_numeric(to) && !is_floating(to->kind) &&
        (is_unsigned_kind(type_effective_kind(to)) || is_unsigned_kind(e->semantic_type)))
        return;
    if (to && is_numeric(to) && constant_number(e, &value) &&
        !(e && is_floating(e->semantic_type) && !is_floating(to->kind)
              ? floating_integer_in_range(value, e->semantic_type, type_effective_kind(to),
                                          a->program->c11_numeric)
              : numeric_value_fits(to, value)) &&
        !(is_floating(to->kind) && !isfinite(value) && finite_real_literals(e))) {
        const char *name = to->kind == TYPE_BYTE ? "char"
                           : to->kind == TYPE_SIGNED_CHAR ? "signed char"
                           : to->kind == TYPE_UNSIGNED_CHAR ? "unsigned char"
                           : to->kind == TYPE_SHORT ? "short"
                           : to->kind == TYPE_UNSIGNED_SHORT ? "unsigned short"
                           : to->kind == TYPE_INT ? "int"
                           : to->kind == TYPE_LONG ? "long"
                           : to->kind == TYPE_LONG_LONG ? "long long"
                           : to->kind == TYPE_UNSIGNED_INT ? "unsigned int"
                           : to->kind == TYPE_UNSIGNED_LONG ? "unsigned long"
                           : to->kind == TYPE_UNSIGNED_LONG_LONG ? "unsigned long long"
                           : to->kind == TYPE_FLOAT ? "float"
                           : to->kind == TYPE_LONG_DOUBLE ? "long double" : "double";
        if (!finite_real_literals(e))
            report(a, e->location, "constant value is out of range for %s in %s", name, context);
        else
            warn(a, e->location, "constant value is out of range for %s in %s", name, context);
    }
}
/* Crunch permits widening numeric conversions only: byte -> int -> long -> real. */
/**
 * @brief Checks whether a value may be assigned to a destination type.
 * @param to Crunch type node being inspected or emitted.
 * @param from Crunch type node being inspected or emitted.
 * @return True when a value of the source type can be assigned to the target type; otherwise false.
 */
static bool assignable(const Type *to, const Type *from)
{
    if (!to || !from)
        return false;
    if (to->kind == TYPE_POINTER || from->kind == TYPE_POINTER)
        return pointer_assignable(to, from);
    if (type_equal(to, from))
        return true;
    if (is_numeric(to) && is_numeric(from)) {
        if (to->kind == TYPE_ENUM || from->kind == TYPE_ENUM ||
            is_promoted_small_kind(to->kind) || is_promoted_small_kind(from->kind))
            return true;
        if (to->kind == TYPE_LONG_DOUBLE || from->kind == TYPE_LONG_DOUBLE ||
            to->kind == TYPE_FLOAT || from->kind == TYPE_FLOAT)
            return true;
        if (is_unsigned_kind(from->kind) && to->kind == TYPE_REAL)
            return true;
        if (to->kind != TYPE_REAL && from->kind != TYPE_REAL &&
            (is_unsigned_kind(to->kind) || is_unsigned_kind(from->kind)))
            return true;
        return numeric_rank(from) <= numeric_rank(to);
    }
    return false;
}
/**
 * @brief Checks assignment compatibility, including a null pointer constant.
 * @param to Destination type.
 * @param from Source expression type.
 * @param value Source expression, used to recognize a null pointer constant.
 * @return True when the value is assignable to the destination type.
 */
static bool assignable_expr(const Type *to, const Type *from, const Expr *value)
{
    if (to && to->kind == TYPE_BOOL && from && from->kind == TYPE_POINTER)
        return true;
    if (value && value->semantic_c11_numeric && is_numeric(to) && is_numeric(from))
        return true;
    if (to && to->kind == TYPE_POINTER && is_null_literal(value))
        return true;
    return assignable(to, from);
}
/**
 * @brief Allocates and initializes a semantic symbol.
 * @param a Semantic analyzer state recording allocation failure.
 * @param name Identifier name to search for or emit.
 * @param len Length of the symbol name in bytes.
 * @param kind Token, AST-node, or type kind to process.
 * @param loc Source location associated with the token or diagnostic.
 * @param d Declaration node being analyzed or emitted.
 * @param type Crunch type node being analyzed or emitted.
 * @param defined Whether the function symbol has a definition.
 * @return Allocated symbol record, or NULL if allocation fails.
 */
static Symbol *new_symbol(Analyzer *a, const char *name, size_t len, SemanticSymbolKind kind,
                          SourceLocation loc, Decl *d, Type *type, bool defined)
{
    Symbol *s = analyzer_alloc(a, sizeof(*s));
    if (!s)
        return NULL;
    s->name = name;
    s->length = len;
    s->kind = kind;
    s->location = loc;
    s->declaration = d;
    s->type = type;
    s->defined = defined;
    s->internal = d && d->is_static;
    return s;
}
/**
 * @brief Adds a declaration to a scope and detects duplicates.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param scope Lexical scope being searched or updated.
 * @param name Identifier name to search for or emit.
 * @param len Length of the symbol name in bytes.
 * @param kind Token, AST-node, or type kind to process.
 * @param loc Source location associated with the token or diagnostic.
 * @param d Declaration node being analyzed or emitted.
 * @param type Crunch type node being analyzed or emitted.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool add_scope_symbol(Analyzer *a, Scope *scope, const char *name, size_t len,
                             SemanticSymbolKind kind, SourceLocation loc, Decl *d, Type *type)
{
    if (find_local(scope->symbols, name, len)) {
        report(a, loc, "duplicate declaration of '%.*s' in the same scope", (int)len, name);
        return true;
    }
    Symbol *s = new_symbol(a, name, len, kind, loc, d, type, false);
    if (!s)
        return false;
    s->internal = d && d->is_extern && d->semantic_internal_linkage;
    s->next = scope->symbols;
    scope->symbols = s;
    return true;
}
/**
 * @brief Checks whether function declarations have compatible signatures.
 * @param a Declaration node being inspected or emitted.
 * @param b Declaration node being inspected or emitted.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool function_compatible(const Decl *a, const Decl *b)
{
    if (!value_type_equal(a->as.function.return_type, b->as.function.return_type) ||
        a->as.function.parameters.count != b->as.function.parameters.count ||
        a->as.function.variadic != b->as.function.variadic)
        return false;
    for (size_t i = 0; i < a->as.function.parameters.count; i++)
        if (!value_type_equal(a->as.function.parameters.items[i].type,
                        b->as.function.parameters.items[i].type))
            return false;
    return true;
}
/**
 * @brief Adds a global declaration to semantic symbol tables.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param d Declaration node being analyzed or emitted.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool add_global(Analyzer *a, Decl *d)
{
    size_t len;
    const char *name = decl_name(d, &len);
    if (d->kind == DECL_VARIABLE || d->kind == DECL_FUNCTION) {
        for (size_t i = 0; i < a->program->block_externals.count; ++i) {
            Decl *block = a->program->block_externals.items[i];
            size_t block_length;
            const char *block_name = decl_name(block, &block_length);
            if (same_name(name, len, block_name, block_length)) {
                bool compatible = linked_declarations_compatible(block, d);
                bool conflicting_linkage = d->is_static && !block->semantic_internal_linkage;
                if (!compatible)
                    report(a, d->location, "conflicting declaration of '%s'", name);
                else if (conflicting_linkage)
                    report(a, d->location,
                           "static declaration of '%s' follows external declaration", name);
                if (!compatible || conflicting_linkage)
                    break;
            }
        }
    }
    if (d->is_static && d->kind == DECL_FUNCTION && strcmp(name, "main") == 0)
        report(a, d->location, "main cannot have static linkage");
    if (d->kind == DECL_STRUCT) {
        Symbol *st = lookup_struct(a, name, len);
        if (st) {
            report(a, d->location, "duplicate structure declaration '%s'", name);
            return true;
        }
        Symbol *s = new_symbol(a, name, len, SEMANTIC_SYMBOL_STRUCTURE, d->location, d, NULL, false);
        if (!s)
            return false;
        s->next = a->model->structures;
        a->model->structures = s;
        return true;
    }
    Symbol *old = find_local(a->model->globals, name, len);
    if (old) {
        if (d->kind == DECL_TYPE_ALIAS && old->kind == SEMANTIC_SYMBOL_TYPE_ALIAS) {
            if (!identical_type(old->type, d->as.type_alias.type))
                report(a, d->location, "conflicting declaration of type alias '%s'", name);
            return true;
        }
        if (d->kind == DECL_VARIABLE && old->kind == SEMANTIC_SYMBOL_VARIABLE) {
            if (d->semantic_alignment > old->alignment)
                old->alignment = d->semantic_alignment;
            if (d->is_thread_local != old->declaration->is_thread_local) {
                report(a, d->location, "conflicting thread-local declaration of '%s'", name);
            } else if (d->is_static && !old->internal) {
                report(a, d->location, "static declaration of '%s' follows external declaration",
                       name);
            } else if (!d->is_extern && !d->is_static && old->internal) {
                report(a, d->location, "non-static declaration of '%s' follows static declaration",
                       name);
            } else if (!qualified_equal(old->type, d->as.variable.type) &&
                       !(old->type && d->as.variable.type &&
                         old->type->kind == TYPE_ARRAY &&
                         d->as.variable.type->kind == TYPE_ARRAY &&
                         (!old->type->array_length ||
                          !d->as.variable.type->array_length) &&
                         old->type->is_const == d->as.variable.type->is_const &&
                         old->type->is_volatile == d->as.variable.type->is_volatile &&
                         qualified_equal(old->type->pointee,
                                         d->as.variable.type->pointee))) {
                report(a, d->location, "conflicting declaration of variable '%s'", name);
            } else if (d->as.variable.initializer.kind != INIT_NONE && old->defined) {
                report(a, d->location, "duplicate declaration of '%s'", name);
            } else if (d->as.variable.initializer.kind != INIT_NONE) {
                old->defined = true;
                old->declaration = d;
            } else if (!d->is_extern && !old->defined && !old->tentative) {
                old->tentative = true;
                old->declaration = d;
            }
            if (old->type && d->as.variable.type &&
                       old->type->kind == TYPE_ARRAY && d->as.variable.type->kind == TYPE_ARRAY &&
                       !old->type->array_length && d->as.variable.type->array_length)
                old->type = d->as.variable.type;
            return true;
        }
        if (d->kind == DECL_FUNCTION && old->kind == SEMANTIC_SYMBOL_FUNCTION) {
            Decl *prior = old->declaration;
            if (d->is_static && !old->internal) {
                report(a, d->location, "static declaration of '%s' follows external declaration",
                       name);
                return true;
            }
            if (!function_compatible(prior, d)) {
                report(a, d->location, "conflicting declaration of function '%s'", name);
                return true;
            }
            if (d->as.function.body && old->defined) {
                report(a, d->location, "duplicate definition of function '%s'", name);
                return true;
            }
            if (d->as.function.body) {
                old->defined = true;
                old->declaration = d;
                old->type = d->as.function.return_type;
            } else if (d->is_extern && !old->defined) {
                old->declaration = d;
            }
            return true;
        }
        report(a, d->location, "duplicate declaration of '%s'", name);
        return true;
    }
    Symbol *s = new_symbol(
        a, name, len,
        d->kind == DECL_FUNCTION ? SEMANTIC_SYMBOL_FUNCTION :
        d->kind == DECL_TYPE_ALIAS ? SEMANTIC_SYMBOL_TYPE_ALIAS : SEMANTIC_SYMBOL_VARIABLE,
        d->location, d, decl_type(d),
        d->kind == DECL_FUNCTION ? d->as.function.body != NULL :
        d->kind == DECL_VARIABLE && d->as.variable.initializer.kind != INIT_NONE);
    if (!s)
        return false;
    s->tentative = d->kind == DECL_VARIABLE && !d->is_extern &&
                   d->as.variable.initializer.kind == INIT_NONE;
    if (d->kind == DECL_VARIABLE)
        s->alignment = d->semantic_alignment;
    s->next = a->model->globals;
    a->model->globals = s;
    a->model->global_count++;
    return true;
}
/**
 * @brief Finds a flexible array within a by-value aggregate type.
 * @param a Analyzer holding the structure declarations.
 * @param type Type to inspect.
 * @param depth Current recursion depth.
 * @return True when the type contains a flexible array member.
 */
static bool contains_flexible_array(Analyzer *a, const Type *type, unsigned depth)
{
    if (!type || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    if (type->kind == TYPE_ARRAY)
        return contains_flexible_array(a, type->pointee, depth + 1);
    if (type->kind != TYPE_STRUCT || !type->struct_name)
        return false;
    Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
    Decl *structure = symbol ? symbol->declaration : NULL;
    if (!structure)
        return false;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        Decl *field = structure->as.structure.fields[i];
        if (field_is_flexible_array(field) ||
            contains_flexible_array(a, field->as.variable.type, depth + 1))
            return true;
    }
    return false;
}

/**
 * @brief Checks whether an array type contains a runtime bound.
 * @param t Type to inspect.
 * @return True when any array dimension has a runtime bound.
 */
static bool variable_array_type(const Type *t)
{
    while (t && t->kind == TYPE_ARRAY) {
        if (t->array_bound && t->array_bound->variable)
            return true;
        t = t->pointee;
    }
    return false;
}

/**
 * @brief Finds the active VLA dimension represented by an array type.
 * @param a Analyzer holding the visible declarations.
 * @param type Array type produced by expression analysis.
 * @param dimension Receives the dimension index.
 * @param parameter Receives the parameter when the dimension belongs to one.
 * @return Borrowed VLA declaration, or NULL for a fixed-size type.
 */
static Decl *find_vla_dimension(Analyzer *a, const Type *type, unsigned *dimension,
                                Parameter **parameter)
{
    if (!type || type->kind != TYPE_ARRAY)
        return NULL;
    for (Scope *scope = a->scope; scope; scope = scope->parent) {
        for (Symbol *symbol = scope->symbols; symbol; symbol = symbol->next) {
            Decl *d = symbol->declaration;
            Parameter *p = symbol->parameter;
            if ((!d || d->kind != DECL_VARIABLE ||
                 (!d->as.variable.semantic_vla && !d->as.variable.semantic_vla_pointer)) &&
                (!p || !p->type || p->type->kind != TYPE_POINTER ||
                 !variable_array_type(p->type->pointee)))
                continue;
            unsigned index = 0;
            const Type *start = p ? p->type : d->as.variable.type;
            if (p || d->as.variable.semantic_vla_pointer)
                start = start->pointee;
            for (const Type *part = start; part && part->kind == TYPE_ARRAY;
                 part = part->pointee, ++index) {
                if ((part == type || (part->array_bound && part->array_bound == type->array_bound)) &&
                    (index == 0 || variable_array_type(part))) {
                    for (const Type *dim = part; dim && dim->kind == TYPE_ARRAY;
                         dim = dim->pointee)
                        if (!dim->array_length &&
                            !(dim->array_bound && dim->array_bound->variable) &&
                            !(d && d->as.variable.semantic_vla && dim == d->as.variable.type))
                            return NULL;
                    *dimension = index;
                    *parameter = p;
                    return d;
                }
            }
        }
    }
    return NULL;
}

static void validate_type(Analyzer *a, const Type *t, SourceLocation loc);

/**
 * @brief Identifies a structure or union that is incomplete at this source location.
 * @param a Analyzer holding the structure definitions.
 * @param type Type to inspect.
 * @return True when the aggregate definition is not yet complete.
 */
static bool incomplete_structure(Analyzer *a, const Type *type)
{
    return type && type->kind == TYPE_STRUCT && type->struct_name &&
           !lookup_struct(a, type->struct_name, strlen(type->struct_name));
}

/**
 * @brief Validates runtime bounds in an adjusted function parameter.
 * @param a Analyzer holding the preceding parameters in scope.
 * @param p Parameter to validate.
 * @param allow_incomplete Whether a prototype may use an incomplete aggregate.
 * @return No value; records diagnostics and resolved bounds.
 */
static void validate_parameter_type(Analyzer *a, Parameter *p, bool allow_incomplete)
{
    Type *type = p->type;
    if (allow_incomplete && incomplete_structure(a, type))
        return;
    bool saved = a->allow_variable_array_bounds;
    if (type && type->kind == TYPE_POINTER &&
        (type->array_bound || (type->pointee && type->pointee->kind == TYPE_ARRAY))) {
        Type pointer = *type;
        pointer.pointee = NULL;
        a->allow_variable_array_bounds = true;
        validate_type(a, &pointer, p->location);
        if (type->pointee && type->pointee->kind == TYPE_ARRAY)
            validate_type(a, type->pointee, p->location);
        else {
            a->allow_variable_array_bounds = false;
            validate_type(a, type->pointee, p->location);
        }
        const Type *element = type->pointee;
        size_t size, alignment;
        while (element && element->kind == TYPE_ARRAY)
            element = element->pointee;
        if ((type->array_bound || variable_array_type(type->pointee)) &&
            !type_layout_size(element, layout_find_struct, a, &size, &alignment))
            report(a, p->location, "array parameter requires a complete element type");
    } else {
        a->allow_variable_array_bounds = false;
        validate_type(a, type, p->location);
    }
    a->allow_variable_array_bounds = saved;
}

/**
 * @brief Validates a type and any nested array bounds.
 * @param a Analyzer receiving diagnostics.
 * @param t Type to validate.
 * @param loc Location for diagnostics.
 * @return No value; records diagnostics and resolved array bounds.
 */
static void validate_type(Analyzer *a, const Type *t, SourceLocation loc)
{
    if (!t)
        return;
    if (t->array_bound) {
        ArrayBound *bound = t->array_bound;
        if (!bound->evaluated) {
            bound->evaluated = true;
            Expr *expression = bound->expression;
            bool allow_variable = a->allow_variable_array_bounds;
            a->allow_variable_array_bounds = false;
            Type *count_type = expression ? check_expr(a, expression) : NULL;
            a->allow_variable_array_bounds = allow_variable;
            IntegerValue value;
            bool constant = integer_constant(expression, &value, 0);
            if (allow_variable && count_type && is_numeric(count_type) &&
                !is_floating(count_type->kind) && !constant &&
                !constant_expression_form(expression, 0)) {
                bound->variable = true;
            } else if (!count_type || !is_numeric(count_type) || is_floating(count_type->kind) ||
                !constant || !value.bits ||
                (!is_unsigned_kind(value.kind) && signed_bits(value.bits) <= 0) ||
                value.bits > SIZE_MAX) {
                report(a, expression ? expression->location : loc,
                       "fixed array bound must be a positive integer constant");
            } else {
                bound->valid = true;
                bound->length = (size_t)value.bits;
            }
        } else if (bound->variable && a->allow_variable_array_bounds) {
            bool allow_variable = a->allow_variable_array_bounds;
            a->allow_variable_array_bounds = false;
            Type *count_type = check_expr(a, bound->expression);
            a->allow_variable_array_bounds = allow_variable;
            if (!count_type || !is_numeric(count_type) || is_floating(count_type->kind))
                report(a, bound->expression->location, "variable array bound must have integer type");
        } else if (!bound->valid) {
            report(a, bound->expression ? bound->expression->location : loc,
                   "fixed array bound must be a positive integer constant");
        }
        if (bound->valid && t->kind == TYPE_ARRAY)
            ((Type *)t)->array_length = bound->length;
    }
    if (t->kind == TYPE_LONG_DOUBLE && !a->suppress_unsigned_types) {
        a->program->contains_long_double = true;
        if (!a->unevaluated_depth)
            a->program->requires_long_double_ir = true;
    }
    if (t->kind == TYPE_FLOAT && !a->program->contains_float) {
        a->program->contains_float = true;
        a->program->float_location = loc;
    }
    if (t->kind == TYPE_FLOAT && !a->suppress_float_types)
        a->program->contains_float_ir = true;
    if (t->kind == TYPE_POINTER || t->kind == TYPE_ARRAY) {
        if (t->kind == TYPE_ARRAY && contains_flexible_array(a, t->pointee, 0))
            report(a, loc, "array element cannot contain a flexible array member");
        if (t->kind == TYPE_POINTER && t->pointee &&
            t->pointee->kind == TYPE_STRUCT && t->pointee->struct_name &&
            !lookup_struct(a, t->pointee->struct_name, strlen(t->pointee->struct_name))) {
            return;
        } else if (t->kind == TYPE_POINTER && t->pointee &&
            t->pointee->kind == TYPE_LONG_DOUBLE) {
            bool requires_long_double_ir = a->program->requires_long_double_ir;
            validate_type(a, t->pointee, loc);
            a->program->requires_long_double_ir = requires_long_double_ir;
        } else {
            bool allow_variable = a->allow_variable_array_bounds;
            if (t->kind == TYPE_POINTER)
                a->allow_variable_array_bounds = false;
            validate_type(a, t->pointee, loc);
            a->allow_variable_array_bounds = allow_variable;
        }
        if (t->kind == TYPE_ARRAY &&
            (t->array_length_from_typedef || (t->array_bound && t->array_bound->valid)) &&
            !variable_array_type(t)) {
            size_t size, alignment;
            if (!type_layout_size(t, layout_find_struct, a, &size, &alignment) ||
                size > INT64_MAX)
                report(a, loc, "fixed array has an incomplete element type or unsupported size");
        }
        return;
    }
    if (t->kind == TYPE_FUNCTION && t->function_signature) {
        Decl *signature = t->function_signature;
        if (signature->as.function.return_type &&
            signature->as.function.return_type->kind == TYPE_ARRAY)
            report(a, loc, "functions cannot return arrays");
        if (signature->as.function.return_type &&
            signature->as.function.return_type->kind == TYPE_FUNCTION)
            report(a, loc, "functions cannot return functions");
        if (!incomplete_structure(a, signature->as.function.return_type))
            validate_type(a, signature->as.function.return_type, loc);
        for (size_t i = 0; i < signature->as.function.parameters.count; ++i)
            if (!incomplete_structure(a, signature->as.function.parameters.items[i].type))
                validate_type(a, signature->as.function.parameters.items[i].type, loc);
        return;
    }
    if (is_unsigned_kind(t->kind) && !a->suppress_unsigned_types) {
        ++a->unsigned_uses;
        if (!a->program->contains_unsigned) {
            a->program->contains_unsigned = true;
            a->program->unsigned_location = loc;
        }
    }
    if (t->kind == TYPE_STRUCT && t->struct_name &&
        !lookup_struct(a, t->struct_name, strlen(t->struct_name)))
        report(a, loc, "unknown structure type '%s'", t->struct_name);
}

/**
 * @brief Identifies a pointer whose structure type has no definition.
 * @param a Analyzer holding the structure definitions.
 * @param type Type to inspect.
 * @return True for a pointer to an incomplete structure or union.
 */
static bool incomplete_structure_pointer(Analyzer *a, const Type *type)
{
    const Type *pointee = type && type->kind == TYPE_POINTER ? type->pointee : NULL;
    return pointee && pointee->kind == TYPE_STRUCT && pointee->struct_name &&
           !lookup_struct(a, pointee->struct_name, strlen(pointee->struct_name));
}

/**
 * @brief Identifies an external object declaration with an incomplete tag type.
 * @param a Analyzer holding the structure definitions.
 * @param declaration Variable declaration to inspect.
 * @return True when the declaration is not a definition and its type is incomplete.
 */
static bool incomplete_external_structure(Analyzer *a, const Decl *declaration)
{
    const Type *type = declaration && declaration->kind == DECL_VARIABLE
                           ? declaration->as.variable.type : NULL;
    return incomplete_structure(a, type) && declaration->is_extern &&
           declaration->as.variable.initializer.kind == INIT_NONE;
}

/**
 * @brief Identifies a pointer to an incomplete array.
 * @param type Type to inspect.
 * @return True when the pointed-to array has no known bound.
 */
static bool incomplete_array_pointer(const Type *type)
{
    const Type *pointee = type && type->kind == TYPE_POINTER ? type->pointee : NULL;
    return pointee && pointee->kind == TYPE_ARRAY && !pointee->array_length &&
           !(pointee->array_bound && pointee->array_bound->variable);
}

/**
 * @brief Records a reference to a runtime-named external function.
 * @param a Semantic analyzer containing the program.
 * @param name Function name to mark on all matching declarations.
 * @return No value.
 */
static void mark_runtime_referenced(Analyzer *a, const char *name) {
    for (size_t i = 0; i < a->program->declarations.count; ++i) {
        Decl *d = a->program->declarations.items[i];
        if (d && d->kind == DECL_FUNCTION && !strcmp(d->as.function.name, name))
            d->as.function.runtime_referenced = true;
    }
}

/**
 * @brief Distinguishes a visible runtime prototype from a later user definition.
 * @param a Analyzer containing the translation unit.
 * @param declaration Visible function declaration.
 * @return Definition when present, otherwise the visible declaration.
 */
static const Decl *runtime_effective_declaration(Analyzer *a, const Decl *declaration)
{
    if (!crunch_is_runtime_declaration(declaration))
        return declaration;
    for (size_t i = 0; i < a->program->declarations.count; ++i) {
        Decl *d = a->program->declarations.items[i];
        if (d && d->kind == DECL_FUNCTION && d->as.function.body &&
            !strcmp(d->as.function.name, declaration->as.function.name))
            return d;
    }
    return declaration;
}

static Type *check_expr(Analyzer *a, Expr *e);
static Type *check_expr_raw(Analyzer *a, Expr *e);
static Type *check_expr_impl(Analyzer *a, Expr *e);
static void check_stmt(Analyzer *a, Stmt *s);
static void check_stmt_impl(Analyzer *a, Stmt *s);
static void check_enum_decl(Analyzer *a, Decl *d, bool global);
static void validate_struct_fields(Analyzer *a, Decl *d);

/**
 * @brief Checks an expression designating a writable System V argument list.
 * @param a Analyzer receiving diagnostics.
 * @param expression Argument-list expression.
 * @return True when the expression has the supported argument-list type.
 */
static bool valid_va_list(Analyzer *a, Expr *expression)
{
    Type *type = check_expr(a, expression);
    Type *element = type && type->kind == TYPE_POINTER ? type->pointee : NULL;
    size_t size, alignment;
    return element && element->kind == TYPE_STRUCT && element->struct_name &&
           (!strcmp(element->struct_name, "__crunch_va_list_tag") ||
            !strcmp(element->struct_name, "$builtin_va_list")) &&
           !read_only(a, element, 0) &&
           type_layout_size(element, layout_find_struct, a, &size, &alignment) &&
           size == 24 && alignment == 8;
}

/**
 * @brief Checks an indirect call against its function-pointer signature.
 * @param a Semantic analyzer.
 * @param call Call expression.
 * @param pointer Type of the called expression.
 * @return Function return type, or NULL for an invalid target.
 */
static Type *check_indirect_call(Analyzer *a, Expr *call, Type *pointer)
{
    Decl *signature = pointer && pointer->kind == TYPE_POINTER && pointer->pointee &&
                      pointer->pointee->kind == TYPE_FUNCTION
                          ? pointer->pointee->function_signature : NULL;
    if (!signature) {
        report(a, call->location, "called expression is not a function pointer");
        for (size_t i = 0; i < call->as.call.arguments.count; ++i)
            check_expr(a, call->as.call.arguments.items[i]);
        return NULL;
    }
    size_t expected = signature->as.function.parameters.count;
    if (call->as.call.arguments.count < expected ||
        (!signature->as.function.variadic && expected != call->as.call.arguments.count))
        report(a, call->location, signature->as.function.variadic
                   ? "function pointer expects at least %zu arguments but received %zu"
                   : "function pointer expects %zu arguments but received %zu",
               expected, call->as.call.arguments.count);
    for (size_t i = 0; i < call->as.call.arguments.count; ++i) {
        Expr *value = call->as.call.arguments.items[i];
        Type *actual = check_expr(a, value);
        if (i < expected && actual) {
            Type *parameter = signature->as.function.parameters.items[i].type;
            check_constant_range(a, parameter, value, "function argument");
            if (!assignable_expr(parameter, actual, value))
                report(a, value->location, "argument %zu to function pointer has incompatible type",
                       i + 1);
        } else if (signature->as.function.variadic && i >= expected &&
                   actual && actual->kind == TYPE_VOID)
            report(a, value->location, "variadic argument cannot have void type");
    }
    return signature->as.function.return_type;
}

/**
 * @brief Validates a block-scope structure at its declaration point.
 * @param a Analyzer holding the structure and lexical scopes.
 * @param type Type that may refer to a local structure.
 * @param depth Current type recursion depth.
 * @return No value; records diagnostics for invalid fields.
 */
static void validate_local_structure(Analyzer *a, const Type *type, unsigned depth)
{
    if (!type || depth >= SEMA_MAX_RECURSION_DEPTH)
        return;
    if (type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY) {
        validate_local_structure(a, type->pointee, depth + 1);
        return;
    }
    if (type->kind != TYPE_STRUCT || !type->struct_name)
        return;
    Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
    if (symbol && symbol->declaration &&
        (symbol->declaration->as.structure.is_local ||
         symbol->declaration->as.structure.anonymous))
        validate_struct_fields(a, symbol->declaration);
}

/**
 * @brief Registers an enum definition embedded in a type name.
 * @param a Analyzer holding the current lexical scope.
 * @param d Enum declaration, or NULL when no definition is present.
 * @return No value; records diagnostics and any file-scope processing state.
 */
static void check_embedded_enum(Analyzer *a, Decl *d)
{
    if (!d)
        return;
    for (ProcessedEnum *p = a->model->processed_enums; p; p = p->next)
        if (p->declaration == d)
            return;
    if (a->at_file_scope || a->initializer_inference) {
        ProcessedEnum *p = analyzer_alloc(a, sizeof(*p));
        if (!p)
            return;
        p->declaration = d;
        p->next = a->model->processed_enums;
        a->model->processed_enums = p;
    }
    check_enum_decl(a, d, a->at_file_scope);
}

/**
 * @brief Registers file-scope enum definitions in an initializer in source order.
 * @param a Analyzer holding the current global declarations.
 * @param e Initializer expression to traverse.
 * @param depth Current recursion depth.
 * @return No value; reports recursion or semantic errors through the analyzer.
 */
static void scan_embedded_enums(Analyzer *a, Expr *e, unsigned depth)
{
    if (!e || depth >= SEMA_MAX_RECURSION_DEPTH)
        return;
    switch (e->kind) {
    case EXPR_CAST:
        check_embedded_enum(a, e->as.cast.enum_definition);
        scan_embedded_enums(a, e->as.cast.operand, depth + 1);
        break;
    case EXPR_OFFSETOF:
        check_embedded_enum(a, e->as.offset_of.enum_definition);
        for (size_t i = 0; i < e->as.offset_of.count; ++i)
            scan_embedded_enums(a, e->as.offset_of.items[i].index, depth + 1);
        break;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        check_embedded_enum(a, e->as.size_of.enum_definition);
        scan_embedded_enums(a, e->as.size_of.operand, depth + 1);
        break;
    case EXPR_UNARY:
        scan_embedded_enums(a, e->as.unary.operand, depth + 1);
        break;
    case EXPR_BINARY:
        scan_embedded_enums(a, e->as.binary.left, depth + 1);
        scan_embedded_enums(a, e->as.binary.right, depth + 1);
        break;
    case EXPR_ASSIGN:
        scan_embedded_enums(a, e->as.assign.target, depth + 1);
        scan_embedded_enums(a, e->as.assign.value, depth + 1);
        break;
    case EXPR_CONDITIONAL:
        scan_embedded_enums(a, e->as.conditional.condition, depth + 1);
        scan_embedded_enums(a, e->as.conditional.then_expr, depth + 1);
        scan_embedded_enums(a, e->as.conditional.else_expr, depth + 1);
        break;
    case EXPR_CALL:
        scan_embedded_enums(a, e->as.call.callee, depth + 1);
        for (size_t i = 0; i < e->as.call.arguments.count; ++i)
            scan_embedded_enums(a, e->as.call.arguments.items[i], depth + 1);
        break;
    case EXPR_ARRAY_INIT:
        for (size_t i = 0; i < e->as.array_init.count; ++i)
            scan_embedded_enums(a, e->as.array_init.items[i], depth + 1);
        break;
    case EXPR_DESIGNATED:
        for (size_t i = 0; i < e->as.designated.count; ++i)
            scan_embedded_enums(a, e->as.designated.items[i].index, depth + 1);
        scan_embedded_enums(a, e->as.designated.value, depth + 1);
        break;
    case EXPR_INDEX:
        scan_embedded_enums(a, e->as.index.base, depth + 1);
        scan_embedded_enums(a, e->as.index.index, depth + 1);
        break;
    case EXPR_MEMBER:
        scan_embedded_enums(a, e->as.member.base, depth + 1);
        break;
    default:
        break;
    }
}
/**
 * @brief Checks whether an expression can be assigned to.
 * @param e Expression node being analyzed or emitted.
 * @return True when the expression can be assigned to; otherwise false.
 */
static bool is_lvalue(Expr *e)
{
    while (e && e->kind == EXPR_MEMBER) {
        if (e->as.member.through_pointer)
            return true;
        e = e->as.member.base;
    }
    return e && ((e->kind == EXPR_NAME && !e->semantic_enum_constant) || e->kind == EXPR_INDEX ||
                 (e->kind == EXPR_CAST && e->as.cast.target_type &&
                  e->as.cast.target_type->kind != TYPE_VOID &&
                  e->as.cast.target_type->kind != TYPE_FUNCTION && e->as.cast.operand &&
                  e->as.cast.operand->kind == EXPR_ARRAY_INIT) ||
                 (e->kind == EXPR_UNARY && e->as.unary.op == UNARY_DEREFERENCE));
}

/**
 * @brief Finds a field, including fields promoted by anonymous members.
 * @param a Analyzer used to resolve nested structures.
 * @param structure Structure declaration to search.
 * @param name Field name.
 * @param length Field name length.
 * @param through_qualifiers Receives qualifiers of anonymous containing members.
 * @param depth Current nesting depth.
 * @return Matching field, or NULL if absent.
 */
static Decl *find_visible_field(Analyzer *a, Decl *structure, const char *name, size_t length,
                                unsigned *through_qualifiers, unsigned depth)
{
    if (!structure || depth >= SEMA_MAX_RECURSION_DEPTH)
        return NULL;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        Decl *field = structure->as.structure.fields[i];
        if (field->as.variable.anonymous_member) {
            Type *type = field->as.variable.type;
            Symbol *nested = type && type->struct_name
                                 ? lookup_struct(a, type->struct_name, strlen(type->struct_name))
                                 : NULL;
            unsigned nested_qualifiers = 0;
            Decl *found = nested ? find_visible_field(a, nested->declaration, name, length,
                                                       &nested_qualifiers, depth + 1) : NULL;
            if (found) {
                *through_qualifiers = nested_qualifiers | (type->is_const ? 1u : 0u) |
                                      (type->is_volatile ? 2u : 0u);
                return found;
            }
        } else if (field->as.variable.name &&
                   same_name(field->as.variable.name, field->as.variable.name_length,
                             name, length)) {
            *through_qualifiers = 0;
            return field;
        }
    }
    return NULL;
}

/**
 * @brief Finds a member and its byte offset, including anonymous aggregate members.
 * @param a Analyzer resolving aggregate tags.
 * @param type Aggregate type to search.
 * @param name Member name.
 * @param length Member name length.
 * @param offset Receives the accumulated offset.
 * @param depth Anonymous-member nesting depth.
 * @return The matching field, or NULL if missing or layout is unsupported.
 */
static Decl *offsetof_field(Analyzer *a, Type *type, const char *name, size_t length,
                            size_t *offset, unsigned depth)
{
    if (!type || type->kind != TYPE_STRUCT || !type->struct_name ||
        depth >= SEMA_MAX_RECURSION_DEPTH)
        return NULL;
    Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
    Decl *structure = symbol ? symbol->declaration : NULL;
    FieldLayout fields[256];
    size_t size, alignment;
    if (!structure || structure->as.structure.field_count > 256 ||
        !type_layout_struct_fields(structure, layout_find_struct, a, fields, &size, &alignment))
        return NULL;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        Decl *field = structure->as.structure.fields[i];
        size_t nested = 0;
        Decl *found = NULL;
        if (field->as.variable.anonymous_member)
            found = offsetof_field(a, field->as.variable.type, name, length, &nested, depth + 1);
        else if (field->as.variable.name &&
                 same_name(field->as.variable.name, field->as.variable.name_length, name, length))
            found = field;
        if (found) {
            if (nested > SIZE_MAX - fields[i].offset)
                return NULL;
            *offset = fields[i].offset + nested;
            return found;
        }
    }
    return NULL;
}

static bool enum_constant_form(const Expr *e, unsigned depth);
static bool enum_arithmetic_valid(const Expr *e, unsigned depth);

/**
 * @brief Type-checks an expression and returns its inferred type.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param e Expression node being analyzed or emitted.
 * @return Inferred type, or NULL when the expression cannot be typed.
 */
static Type *check_expr_impl(Analyzer *a, Expr *e)
{
    if (!e)
        return NULL;
    switch (e->kind) {
    case EXPR_OFFSETOF: {
        e->as.offset_of.resolved = false;
        e->as.offset_of.constant = true;
        e->as.offset_of.base_offset = 0;
        e->as.offset_of.value = 0;
        check_embedded_enum(a, e->as.offset_of.enum_definition);
        if (!a->program->contains_unsigned) {
            a->program->contains_unsigned = true;
            a->program->unsigned_location = e->location;
        }
        Type *type = e->as.offset_of.type;
        if (e->as.offset_of.count)
            a->source_offset = e->as.offset_of.items[0].location.offset;
        validate_local_structure(a, type, 0);
        if (type && type->kind == TYPE_STRUCT && type->struct_name) {
            Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
            if (symbol && symbol->declaration->location.offset > e->location.offset)
                validate_struct_fields(a, symbol->declaration);
        }
        validate_type(a, type, e->location);
        size_t size, alignment;
        if (!type || type->kind != TYPE_STRUCT ||
            !type_layout_size(type, layout_find_struct, a, &size, &alignment) ||
            (a->current_structure && type->struct_name &&
             !strcmp(type->struct_name, a->current_structure->as.structure.name))) {
            report(a, e->location, "__builtin_offsetof requires a complete aggregate type");
            return &t_unsigned_long;
        }
        for (size_t i = 0; i < e->as.offset_of.count; ++i) {
            InitializerDesignator *item = &e->as.offset_of.items[i];
            if (item->field) {
                size_t offset = 0;
                Decl *field = offsetof_field(a, type, item->field, item->field_length, &offset, 0);
                if (!field) {
                    report(a, item->location, "invalid __builtin_offsetof member '%s'", item->field);
                    return &t_unsigned_long;
                }
                if (field->as.variable.is_bit_field) {
                    report(a, item->location, "__builtin_offsetof cannot select a bit-field");
                    return &t_unsigned_long;
                }
                e->as.offset_of.base_offset += offset;
                e->as.offset_of.value += offset;
                type = field->as.variable.type;
            } else {
                Type *index_type = check_expr(a, item->index);
                if (!type || type->kind != TYPE_ARRAY ||
                    !type_layout_size(type->pointee, layout_find_struct, a, &size, &alignment)) {
                    report(a, item->location, "__builtin_offsetof index requires an array member");
                    return &t_unsigned_long;
                }
                if (!index_type || !is_numeric(index_type) || is_floating(index_type->kind)) {
                    report(a, item->location, "__builtin_offsetof index requires an integer");
                    return &t_unsigned_long;
                }
                item->semantic_stride = size;
                IntegerValue index_value;
                if (enum_constant_form(item->index, 0) &&
                    enum_arithmetic_valid(item->index, 0) &&
                    integer_constant(item->index, &index_value, 0))
                    e->as.offset_of.value += index_value.bits * (uint64_t)size;
                else
                    e->as.offset_of.constant = false;
                type = type->pointee;
            }
        }
        e->as.offset_of.resolved = true;
        return &t_unsigned_long;
    }
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF: {
        bool is_alignof = e->kind == EXPR_ALIGNOF;
        e->as.size_of.resolved = false;
        e->as.size_of.semantic_vla_decl = NULL;
        e->as.size_of.semantic_vla_parameter = NULL;
        check_embedded_enum(a, e->as.size_of.enum_definition);
        if (!a->program->contains_unsigned) {
            a->program->contains_unsigned = true;
            a->program->unsigned_location = e->location;
        }
        Type *type = e->as.size_of.type;
        ++a->unevaluated_depth;
        if (type) {
            if (e->as.size_of.type_end_offset > a->source_offset)
                a->source_offset = e->as.size_of.type_end_offset;
            validate_local_structure(a, type, 0);
            if (type->kind == TYPE_STRUCT && type->struct_name) {
                Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
                if (symbol && symbol->declaration->location.offset > e->location.offset)
                    validate_struct_fields(a, symbol->declaration);
            }
            validate_type(a, type, e->location);
        } else {
            bool saved = a->skip_evaluation_checks;
            if (a->program->c11_numeric)
                a->skip_evaluation_checks = true;
            type = check_expr_raw(a, e->as.size_of.operand);
            a->skip_evaluation_checks = saved;
        }
        --a->unevaluated_depth;
        if (!is_alignof && !e->as.size_of.type && e->as.size_of.operand)
            e->as.size_of.semantic_vla_decl =
                find_vla_dimension(a, type, &e->as.size_of.semantic_vla_dimension,
                                   &e->as.size_of.semantic_vla_parameter);
        if (!e->as.size_of.type && e->as.size_of.operand &&
            e->as.size_of.operand->semantic_bit_field) {
            e->as.size_of.resolved = false;
            report(a, e->location, "sizeof cannot be applied to a bit-field");
        } else if (!e->as.size_of.type && e->as.size_of.operand &&
            e->as.size_of.operand->kind == EXPR_STRING) {
            size_t size = string_array_count(e->as.size_of.operand);
            size_t width = e->as.size_of.operand->string_literal_wide ? 4 : 1;
            if (!size || size > INT64_MAX / width)
                report(a, e->location, "sizeof requires a complete object type with supported layout");
            else {
                e->as.size_of.size = (int64_t)(size * width);
                e->as.size_of.resolved = true;
            }
        } else if (e->as.size_of.semantic_vla_decl || e->as.size_of.semantic_vla_parameter) {
            if (e->as.size_of.operand->kind != EXPR_NAME)
                check_expr_raw(a, e->as.size_of.operand);
        } else if (!type) {
            report(a, e->location, "%s operand has no object type",
                   is_alignof ? "_Alignof" : "sizeof");
        } else {
            size_t size, alignment;
            const Type *object = type;
            while (object && object->kind == TYPE_ARRAY)
                object = object->pointee;
            if ((a->current_structure && object && object->kind == TYPE_STRUCT &&
                 object->struct_name &&
                 !strcmp(object->struct_name, a->current_structure->as.structure.name)) ||
                !type_layout_size(type, layout_find_struct, a, &size, &alignment) ||
                size > INT64_MAX) {
                report(a, e->location, "%s requires a complete object type with supported layout",
                       is_alignof ? "_Alignof" : "sizeof");
            } else {
                e->as.size_of.size = (int64_t)(is_alignof ? alignment : size);
                e->as.size_of.resolved = true;
            }
        }
        return &t_unsigned_long;
    }
    case EXPR_INTEGER:
        if ((e->integer_suffix != INTEGER_SUFFIX_NONE &&
             e->integer_suffix != INTEGER_SUFFIX_L &&
             e->integer_suffix != INTEGER_SUFFIX_LL) || e->integer_literal_unsigned) {
            if (!a->program->contains_unsigned) {
                a->program->contains_unsigned = true;
                a->program->unsigned_location = e->location;
            }
            if (e->integer_literal_out_of_range) {
                report(a, e->location, "unsigned integer literal exceeds the range of unsigned long");
                return NULL;
            }
            if (e->integer_suffix == INTEGER_SUFFIX_ULL ||
                e->integer_suffix == INTEGER_SUFFIX_LL)
                return &t_unsigned_long_long;
            return e->integer_suffix == INTEGER_SUFFIX_UL ||
                           e->integer_suffix == INTEGER_SUFFIX_L ||
                           e->unsigned_integer_value > UINT32_MAX
                       ? &t_unsigned_long : &t_unsigned_int;
        }
        if (e->integer_literal_out_of_range)
            report(a, e->location, "integer literal exceeds the range of long");
        if (e->integer_suffix == INTEGER_SUFFIX_LL)
            return &t_long_long;
        if (e->integer_suffix == INTEGER_SUFFIX_L)
            return &t_long;
        return (!e->integer_literal_out_of_range &&
                (e->as.integer_value < -2147483648LL || e->as.integer_value > 2147483647LL))
                   ? &t_long
                   : &t_int;
    case EXPR_REAL:
        if (e->real_literal_extended_text) {
            a->program->contains_long_double = true;
            if (!a->unevaluated_depth)
                a->program->requires_long_double_ir = true;
            return &t_long_double;
        }
        if (e->real_literal_float) {
            a->program->contains_float_ir = true;
            if (!a->program->contains_float) {
                a->program->contains_float = true;
                a->program->float_location = e->location;
            }
            return &t_float;
        }
        return &t_real;
    case EXPR_CHAR:
        return e->char_literal_wide || a->program->c11_numeric ? &t_int : &t_byte;
    case EXPR_STRING:
        return e->string_literal_wide ? &t_int_pointer : &t_byte_pointer;
    case EXPR_NAME: {
        Symbol *s = lookup_scope(a->scope, e->as.name.name, e->as.name.length);
        if (!s)
            s = lookup_global(a, e->as.name.name, e->as.name.length);
        if (!s) {
            report(a, e->location, "undefined identifier '%s'", e->as.name.name);
            return NULL;
        }
        if (a->current_function && a->current_function->as.function.semantic_inline_only &&
            s->internal)
            report(a, e->location, "inline definition references internal linkage identifier '%s'",
                   s->name);
        s->used = true;
        e->semantic_function_name = s->function_name;
        if (s->kind == SEMANTIC_SYMBOL_ENUMERATOR) {
            if (s->location.offset > e->location.offset) {
                report(a, e->location, "undefined identifier '%s'", e->as.name.name);
                return NULL;
            }
            e->semantic_enum_constant = true;
            e->semantic_enum_value = s->enum_value;
            return &t_int;
        }
        if (s->kind == SEMANTIC_SYMBOL_FUNCTION) {
            a->program->contains_function_types = true;
            Decl *d = s->declaration;
            if (crunch_is_runtime_name(s->name))
                mark_runtime_referenced(a, s->name);
            if (crunch_is_runtime_declaration(runtime_effective_declaration(a, d)))
                report(a, e->location, "taking the address of runtime function '%s' is unsupported",
                       s->name);
            return owned_function_type(a, s->declaration);
        }
        if (s->kind == SEMANTIC_SYMBOL_TYPE_ALIAS) {
            report(a, e->location, "type alias '%s' used as a value", e->as.name.name);
            return NULL;
        }
        return s->type;
    }
    case EXPR_UNARY: {
        bool raw = e->as.unary.op == UNARY_ADDRESS ||
                   e->as.unary.op == UNARY_PRE_INCREMENT ||
                   e->as.unary.op == UNARY_PRE_DECREMENT ||
                   e->as.unary.op == UNARY_POST_INCREMENT ||
                   e->as.unary.op == UNARY_POST_DECREMENT;
        Type *t = raw ? check_expr_raw(a, e->as.unary.operand)
                      : check_expr(a, e->as.unary.operand);
        if (!t)
            return NULL;
        switch (e->as.unary.op) {
        case UNARY_PLUS:
        case UNARY_MINUS:
            if (!is_numeric(t))
                report(a, e->location, "unary arithmetic operator requires a numeric operand");
            if (t->kind == TYPE_ENUM)
                return type_effective_kind(t) == TYPE_UNSIGNED_INT ? &t_unsigned_int : &t_int;
            return (is_promoted_small_kind(t->kind) ||
                    (a->program->c11_numeric && t->kind == TYPE_BYTE)) ? &t_int : t;
        case UNARY_LOGICAL_NOT:
            if (!is_numeric(t) && t->kind != TYPE_POINTER)
                report(a, e->location, "logical not requires a scalar operand");
            return &t_int;
        case UNARY_BITWISE_NOT:
            if (!is_numeric(t) || is_floating(t->kind))
                report(a, e->location, "bitwise not requires an integer operand");
            if (t->kind == TYPE_ENUM)
                return type_effective_kind(t) == TYPE_UNSIGNED_INT ? &t_unsigned_int : &t_int;
            return (is_promoted_small_kind(t->kind) ||
                    (a->program->c11_numeric && t->kind == TYPE_BYTE)) ? &t_int : t;
        case UNARY_ADDRESS:
            if (e->as.unary.operand->semantic_bit_field)
                report(a, e->location, "cannot take the address of a bit-field");
            else if (t->kind != TYPE_FUNCTION && !is_lvalue(e->as.unary.operand))
                report(a, e->location, "address-of requires an lvalue");
            return owned_pointer_type(a, t);
        case UNARY_DEREFERENCE:
            if (t->kind != TYPE_POINTER) {
                report(a, e->location, "cannot dereference a non-pointer expression");
                return NULL;
            }
            if (t->pointee && t->pointee->kind == TYPE_VOID) {
                report(a, e->location, "cannot dereference a void pointer");
                return NULL;
            }
            return t->pointee;
        case UNARY_PRE_INCREMENT:
        case UNARY_PRE_DECREMENT:
        case UNARY_POST_INCREMENT:
        case UNARY_POST_DECREMENT:
            if (!is_lvalue(e->as.unary.operand) || read_only(a, t, 0) ||
                (!is_numeric(t) && t->kind != TYPE_POINTER) ||
                (t->kind == TYPE_POINTER && t->pointee &&
                 (t->pointee->kind == TYPE_VOID || t->pointee->kind == TYPE_FUNCTION)) ||
                incomplete_structure_pointer(a, t) || incomplete_array_pointer(t))
                report(a, e->location,
                       "increment/decrement requires a modifiable numeric or pointer lvalue");
            e->semantic_bit_field_width = e->as.unary.operand->semantic_bit_field_width;
            return t;
        }
        return NULL;
    }

    case EXPR_BINARY: {
        Type *l = check_expr(a, e->as.binary.left);
        BinaryOp op = e->as.binary.op;
        bool saved = a->skip_evaluation_checks, truth;
        if (a->program->c11_numeric && l &&
            (op == BINARY_LOGICAL_AND || op == BINARY_LOGICAL_OR) &&
            constant_truth(e->as.binary.left, &truth, 0, false) &&
            ((op == BINARY_LOGICAL_AND && !truth) || (op == BINARY_LOGICAL_OR && truth)))
            a->skip_evaluation_checks = true;
        Type *r = check_expr(a, e->as.binary.right);
        a->skip_evaluation_checks = saved;
        if (!l || !r)
            return NULL;
        if (op == BINARY_COMMA)
            return r;
        if (op == BINARY_POWER) {
            if (!is_numeric(l) || !is_numeric(r)) {
                report(a, e->location, "power operator requires numeric operands");
                return NULL;
            }
            return &t_real;
        }
        if (op == BINARY_LOGICAL_AND || op == BINARY_LOGICAL_OR) {
            if ((!is_numeric(l) && l->kind != TYPE_POINTER) ||
                (!is_numeric(r) && r->kind != TYPE_POINTER))
                report(a, e->location, "logical operator requires scalar operands");
            return &t_int;
        }
        if (op == BINARY_EQUAL || op == BINARY_NOT_EQUAL) {
            if (!(is_numeric(l) && is_numeric(r)) && !type_equal(l, r) &&
                !pointer_assignable(l, r) && !pointer_assignable(r, l) &&
                !(l->kind == TYPE_POINTER && is_null_literal(e->as.binary.right)) &&
                !(r->kind == TYPE_POINTER && is_null_literal(e->as.binary.left)))
                report(a, e->location, "equality operands have incompatible types");
            return &t_int;
        }
        if (op == BINARY_LESS || op == BINARY_LESS_EQUAL || op == BINARY_GREATER ||
            op == BINARY_GREATER_EQUAL) {
            if (!(is_numeric(l) && is_numeric(r)) &&
                !(l->kind == TYPE_POINTER && l->pointee &&
                  l->pointee->kind != TYPE_FUNCTION && type_equal(l, r)))
                report(a, e->location,
                       "relational operator requires compatible numeric or pointer operands");
            return &t_int;
        }
        if (op == BINARY_BITWISE_AND || op == BINARY_BITWISE_OR || op == BINARY_BITWISE_XOR ||
            op == BINARY_SHIFT_LEFT || op == BINARY_SHIFT_RIGHT || op == BINARY_REMAINDER) {
            if (!is_numeric(l) || !is_numeric(r) || is_floating(l->kind) || is_floating(r->kind))
                report(a, e->location, "integer operator requires integer operands");
            if ((op == BINARY_SHIFT_LEFT || op == BINARY_SHIFT_RIGHT) &&
                (a->program->c11_numeric || is_unsigned_kind(l->kind) || is_unsigned_kind(r->kind) ||
                 is_promoted_small_kind(l->kind) || is_promoted_small_kind(r->kind))) {
                Type *result = l->kind == TYPE_ENUM
                                   ? (type_effective_kind(l) == TYPE_UNSIGNED_INT
                                          ? &t_unsigned_int : &t_int)
                                   : l->kind == TYPE_BYTE || is_promoted_small_kind(l->kind)
                                         ? &t_int : l;
                check_unsigned_shift_count(a, e->location, result, e->as.binary.right);
                return result;
            }
            if (op == BINARY_REMAINDER &&
                (is_unsigned_kind(type_effective_kind(l)) ||
                 is_unsigned_kind(type_effective_kind(r))))
                check_unsigned_divisor(a, e->location, e->as.binary.right, true);
            return promote(l, r, a->program->c11_numeric);
        }
        if (op == BINARY_ADD || op == BINARY_SUBTRACT) {
            if (incomplete_structure_pointer(a, l) || incomplete_structure_pointer(a, r) ||
                incomplete_array_pointer(l) || incomplete_array_pointer(r)) {
                report(a, e->location, incomplete_array_pointer(l) || incomplete_array_pointer(r)
                       ? "arithmetic on a pointer to an incomplete array"
                       : "arithmetic on a pointer to an incomplete structure");
                return NULL;
            }
            if ((l->kind == TYPE_POINTER && l->pointee && l->pointee->kind == TYPE_FUNCTION) ||
                (r->kind == TYPE_POINTER && r->pointee && r->pointee->kind == TYPE_FUNCTION)) {
                report(a, e->location, "arithmetic on a function pointer is invalid");
                return NULL;
            }
            if ((l->kind == TYPE_POINTER && l->pointee && l->pointee->kind == TYPE_VOID) ||
                (r->kind == TYPE_POINTER && r->pointee && r->pointee->kind == TYPE_VOID)) {
                report(a, e->location, "arithmetic on a void pointer is not supported");
                return NULL;
            }
            if (op == BINARY_SUBTRACT && l->kind == TYPE_POINTER && r->kind == TYPE_POINTER) {
                if (!type_equal(l, r) ||
                    (l->pointee && l->pointee->kind == TYPE_POINTER &&
                     !qualified_equal(l->pointee->pointee, r->pointee->pointee))) {
                    report(a, e->location, "pointer subtraction requires compatible object types");
                    return NULL;
                }
                a->program->contains_pointer_difference = true;
                return &t_long;
            }
            if (l->kind == TYPE_POINTER && is_numeric(r) && !is_floating(r->kind))
                return l;
            if (op == BINARY_ADD && r->kind == TYPE_POINTER && is_numeric(l) &&
                !is_floating(l->kind))
                return r;
        }
        if (op == BINARY_ADD || op == BINARY_SUBTRACT || op == BINARY_MULTIPLY ||
            op == BINARY_DIVIDE) {
            if (!is_numeric(l) || !is_numeric(r)) {
                report(
                    a, e->location,
                    "arithmetic operator requires numeric operands or pointer plus/minus integer");
                return NULL;
            }
            if (op == BINARY_DIVIDE &&
                !is_floating(l->kind) && !is_floating(r->kind) &&
                (is_unsigned_kind(type_effective_kind(l)) ||
                 is_unsigned_kind(type_effective_kind(r))))
                check_unsigned_divisor(a, e->location, e->as.binary.right, false);
            return promote(l, r, a->program->c11_numeric);
        }
        return NULL;
    }
    case EXPR_ASSIGN: {
        Type *l = check_expr_raw(a, e->as.assign.target), *r = check_expr(a, e->as.assign.value);
        e->semantic_bit_field_width = e->as.assign.target->semantic_bit_field_width;
        if (!is_lvalue(e->as.assign.target) || (l && l->kind == TYPE_ARRAY))
            report(a, e->location, "assignment target is not a modifiable lvalue");
        if (read_only(a, l, 0))
            report(a, e->location, "cannot assign to a const object");
        if (l && r) {
            if (e->as.assign.op == ASSIGN_SIMPLE) {
                if (!assignable_expr(l, r, e->as.assign.value))
                    report(a, e->location, "incompatible assignment types");
                else
                    check_constant_range(a, l, e->as.assign.value, "assignment");
            } else if (l->kind == TYPE_POINTER) {
                if ((l->pointee && (l->pointee->kind == TYPE_VOID ||
                                    l->pointee->kind == TYPE_FUNCTION)) ||
                    incomplete_structure_pointer(a, l) ||
                    incomplete_array_pointer(l) ||
                    (e->as.assign.op != ASSIGN_ADD && e->as.assign.op != ASSIGN_SUBTRACT) ||
                    !is_numeric(r) || is_floating(r->kind))
                    report(a, e->location,
                           "pointer compound assignment requires += or -= with an integer");
            } else if (!is_numeric(l) || !is_numeric(r) ||
                       (e->as.assign.op >= ASSIGN_REMAINDER &&
                        (is_floating(l->kind) || is_floating(r->kind))))
                report(a, e->location, "compound assignment requires compatible numeric operands");
            else if (a->program->c11_numeric || is_unsigned_kind(l->kind) || is_unsigned_kind(r->kind) ||
                     is_promoted_small_kind(l->kind) || is_promoted_small_kind(r->kind)) {
                if (e->as.assign.op == ASSIGN_SHIFT_LEFT || e->as.assign.op == ASSIGN_SHIFT_RIGHT)
                    check_unsigned_shift_count(a, e->location, l, e->as.assign.value);
                else if ((e->as.assign.op == ASSIGN_DIVIDE || e->as.assign.op == ASSIGN_REMAINDER) &&
                         !is_floating(l->kind) && !is_floating(r->kind))
                    check_unsigned_divisor(a, e->location, e->as.assign.value,
                                           e->as.assign.op == ASSIGN_REMAINDER);
            }
        }
        return l;
    }
    case EXPR_CALL: {
        Expr *c = e->as.call.callee;
        Symbol *s = NULL;
        e->semantic_builtin_alloca = false;
        if (c && c->kind == EXPR_NAME) {
            s = lookup_scope(a->scope, c->as.name.name, c->as.name.length);
            if (!s)
                s = lookup_global(a, c->as.name.name, c->as.name.length);
            if (s && s->internal && a->current_function &&
                a->current_function->as.function.semantic_inline_only)
                report(a, c->location, "inline definition references internal linkage identifier '%s'",
                       s->name);
            if ((!s || (s->kind == SEMANTIC_SYMBOL_FUNCTION && !s->defined)) &&
                same_name(c->as.name.name, c->as.name.length, "__builtin_alloca", 16)) {
                e->semantic_builtin_alloca = true;
                if (!a->current_function)
                    report(a, e->location, "__builtin_alloca requires a function body");
                else {
                    a->current_function->as.function.semantic_has_alloca = true;
                    a->program->contains_alloca = true;
                }
                if (e->as.call.arguments.count != 1)
                    report(a, e->location, "__builtin_alloca expects one integer argument");
                for (size_t i = 0; i < e->as.call.arguments.count; ++i) {
                    Type *type = check_expr(a, e->as.call.arguments.items[i]);
                    if (type && (!is_numeric(type) || is_floating(type->kind)))
                        report(a, e->as.call.arguments.items[i]->location,
                               "__builtin_alloca size must have integer type");
                }
                return &t_void_pointer;
            }
            if (!s && (same_name(c->as.name.name, c->as.name.length,
                                 "__crunch_va_start", 17) ||
                       same_name(c->as.name.name, c->as.name.length,
                                 "__builtin_va_start", 18))) {
                a->program->contains_va_start = true;
                if (!a->current_function || !a->current_function->as.function.variadic)
                    report(a, e->location, "va_start requires a variadic function definition");
                if (e->as.call.arguments.count != 2) {
                    report(a, e->location, "va_start expects an argument list and final parameter");
                    for (size_t i = 0; i < e->as.call.arguments.count; ++i)
                        check_expr(a, e->as.call.arguments.items[i]);
                    return &t_void;
                }
                Expr *list = e->as.call.arguments.items[0];
                Expr *last = e->as.call.arguments.items[1];
                if (!valid_va_list(a, list))
                    report(a, list->location, "va_start requires a writable System V argument list");
                if (!last || last->kind != EXPR_NAME ||
                    !a->last_named_parameter ||
                    lookup_scope(a->scope, last->as.name.name, last->as.name.length) !=
                        a->last_named_parameter)
                    report(a, last ? last->location : e->location,
                           "va_start requires the final named parameter");
                check_expr(a, last);
                return &t_void;
            }
            if (!s && (same_name(c->as.name.name, c->as.name.length,
                                 "__crunch_va_arg", 15) ||
                       same_name(c->as.name.name, c->as.name.length,
                                 "__builtin_va_arg", 16))) {
                a->program->contains_va_start = true;
                if (e->as.call.arguments.count != 2) {
                    report(a, e->location, "va_arg expects an argument list and type");
                    for (size_t i = 0; i < e->as.call.arguments.count; ++i)
                        check_expr(a, e->as.call.arguments.items[i]);
                    return NULL;
                }
                Expr *list = e->as.call.arguments.items[0];
                Expr *marker = e->as.call.arguments.items[1];
                if (!valid_va_list(a, list))
                    report(a, list->location, "va_arg requires a writable System V argument list");
                Type *pointer = check_expr(a, marker);
                Type *type = pointer && pointer->kind == TYPE_POINTER ? pointer->pointee : NULL;
                if (type && type->kind == TYPE_STRUCT)
                    a->program->contains_va_aggregate = true;
                if (!type || !(type->kind == TYPE_INT || type->kind == TYPE_UNSIGNED_INT ||
                    type->kind == TYPE_LONG || type->kind == TYPE_UNSIGNED_LONG ||
                    type->kind == TYPE_LONG_LONG || type->kind == TYPE_UNSIGNED_LONG_LONG ||
                    type->kind == TYPE_REAL || type->kind == TYPE_LONG_DOUBLE ||
                    type->kind == TYPE_POINTER ||
                    type->kind == TYPE_ENUM || type->kind == TYPE_STRUCT))
                    report(a, marker->location,
                           "va_arg supports promoted integer, double, pointer, and aggregate types only");
                return pointer;
            }
            if (!s && (same_name(c->as.name.name, c->as.name.length, "__crunch_va_copy", 16) ||
                       same_name(c->as.name.name, c->as.name.length, "__crunch_va_end", 15) ||
                       same_name(c->as.name.name, c->as.name.length, "__builtin_va_copy", 17) ||
                       same_name(c->as.name.name, c->as.name.length, "__builtin_va_end", 16))) {
                bool copy_list = same_name(c->as.name.name, c->as.name.length,
                                           "__crunch_va_copy", 16) ||
                                 same_name(c->as.name.name, c->as.name.length,
                                           "__builtin_va_copy", 17);
                a->program->contains_va_start = true;
                size_t expected = copy_list ? 2 : 1;
                if (e->as.call.arguments.count != expected)
                    report(a, e->location, "%s expects %zu argument%s",
                           copy_list ? "va_copy" : "va_end", expected, expected == 1 ? "" : "s");
                for (size_t i = 0; i < e->as.call.arguments.count; ++i)
                    if (!valid_va_list(a, e->as.call.arguments.items[i]))
                        report(a, e->as.call.arguments.items[i]->location,
                               "%s requires a writable System V argument list",
                               copy_list ? "va_copy" : "va_end");
                return &t_void;
            }
            /* Core built-ins; library functions such as exit() and strcmp() need a declaration. */
            if (!s && (same_name(c->as.name.name, c->as.name.length, "copy", 4) ||
                       same_name(c->as.name.name, c->as.name.length, "fill", 4) ||
                       same_name(c->as.name.name, c->as.name.length, "compare", 7))) {
                const char *builtin =
                    same_name(c->as.name.name, c->as.name.length, "copy", 4)   ? "copy"
                    : same_name(c->as.name.name, c->as.name.length, "fill", 4)   ? "fill"
                                                                                 : "compare";
                size_t expected = 3;
                if (e->as.call.arguments.count != expected) {
                    report(a, e->location, "built-in '%s' expects %zu argument%s", builtin,
                           expected, expected == 1 ? "" : "s");
                    for (size_t i = 0; i < e->as.call.arguments.count; i++)
                        check_expr(a, e->as.call.arguments.items[i]);
                    if (!strcmp(builtin, "copy") || !strcmp(builtin, "fill"))
                        return e->as.call.arguments.count
                                   ? check_expr(a, e->as.call.arguments.items[0])
                                   : NULL;
                    return &t_int;
                }
                Type *argtypes[3] = {NULL, NULL, NULL};
                for (size_t i = 0; i < expected; i++)
                    argtypes[i] = check_expr(a, e->as.call.arguments.items[i]);
                if (strcmp(builtin, "compare") && argtypes[0] &&
                    argtypes[0]->kind == TYPE_POINTER &&
                    read_only(a, argtypes[0]->pointee, 0))
                    report(a, e->location, "cannot write through a pointer to const");
                if (argtypes[0] && argtypes[0]->kind != TYPE_POINTER &&
                    !is_null_literal(e->as.call.arguments.items[0]))
                    report(a, e->as.call.arguments.items[0]->location,
                           "first argument to built-in '%s' must be a pointer", builtin);
                if (!strcmp(builtin, "copy") || !strcmp(builtin, "compare")) {
                    if (argtypes[1] && argtypes[1]->kind != TYPE_POINTER &&
                        !is_null_literal(e->as.call.arguments.items[1]))
                        report(a, e->as.call.arguments.items[1]->location,
                               "second argument to built-in '%s' must be a pointer", builtin);
                } else if (argtypes[1] &&
                           (!is_numeric(argtypes[1]) || is_floating(argtypes[1]->kind)))
                    report(a, e->as.call.arguments.items[1]->location,
                           "fill value must be an integer type");
                if (argtypes[2] && (!is_numeric(argtypes[2]) || is_floating(argtypes[2]->kind)))
                    report(a, e->as.call.arguments.items[2]->location,
                           "byte count to built-in '%s' must be an integer type", builtin);
                if (!strcmp(builtin, "copy") || !strcmp(builtin, "fill"))
                    return is_null_literal(e->as.call.arguments.items[0]) ? &t_byte_pointer
                                                                          : argtypes[0];
                return &t_int;
            }
            if (!s) {
                report(a, c->location, "call to undefined function '%s'", c->as.name.name);
                for (size_t i = 0; i < e->as.call.arguments.count; i++)
                    check_expr(a, e->as.call.arguments.items[i]);
                return NULL;
            }
            if (s->kind != SEMANTIC_SYMBOL_FUNCTION) {
                return check_indirect_call(a, e, check_expr(a, c));
            }
        } else {
            return check_indirect_call(a, e, check_expr(a, c));
        }
        Decl *d = s->declaration;
        const Decl *runtime_declaration = runtime_effective_declaration(a, d);
        if (crunch_is_runtime_name(s->name))
            mark_runtime_referenced(a, s->name);
        if (crunch_is_runtime_declaration(runtime_declaration) &&
            !crunch_is_linkable_runtime_declaration(runtime_declaration)) {
            a->program->contains_runtime_dispatch = true;
        }
        if (d && !d->is_extern && !d->is_static && !d->as.function.body &&
            crunch_is_runtime_name(s->name)) {
            validate_type(a, d->as.function.return_type, d->location);
            for (size_t i = 0; i < d->as.function.parameters.count; ++i)
                validate_type(a, d->as.function.parameters.items[i].type,
                              d->as.function.parameters.items[i].location);
        }
        if (crunch_is_runtime_declaration(runtime_declaration) &&
            (!strcmp(s->name, "scanf") || !strcmp(s->name, "fscanf"))) {
            size_t fixed = !strcmp(s->name, "fscanf") ? 2 : 1;
            if (e->as.call.arguments.count < fixed || e->as.call.arguments.count > fixed + 127)
                report(a, e->location, "%s requires its fixed arguments and at most 127 destinations", s->name);
            for (size_t i = 0; i < e->as.call.arguments.count; ++i) {
                Type *arg = check_expr(a, e->as.call.arguments.items[i]);
                if (!arg) continue;
                if (i < fixed) {
                    if (!assignable_expr(d->as.function.parameters.items[i].type, arg,
                                         e->as.call.arguments.items[i]))
                        report(a, e->as.call.arguments.items[i]->location,
                               "argument %zu to '%s' has incompatible type", i + 1, s->name);
                } else if (arg->kind != TYPE_POINTER && !is_null_literal(e->as.call.arguments.items[i])) {
                    report(a, e->as.call.arguments.items[i]->location,
                           "%s destinations must be pointers", s->name);
                }
            }
            return &t_int;
        }
        if (crunch_is_runtime_declaration(runtime_declaration) &&
            (!strcmp(s->name, "printf") || !strcmp(s->name, "fprintf") || !strcmp(s->name, "sprintf") ||
             !strcmp(s->name, "snprintf"))) {
            bool is_snprintf = !strcmp(s->name, "snprintf");
            bool is_sprintf = !strcmp(s->name, "sprintf");
            size_t format_index = is_snprintf ? 2 : (!strcmp(s->name, "fprintf") || is_sprintf) ? 1 : 0;
            size_t fixed = format_index + 1;
            Type *first = format_index && d->as.function.parameters.count == fixed
                               ? d->as.function.parameters.items[0].type : NULL;
            if (e->as.call.arguments.count < fixed || e->as.call.arguments.count > fixed + 127)
                report(a, e->location, "%s expects %s and at most 127 values", s->name,
                       is_snprintf ? "a buffer, capacity and format" :
                       is_sprintf ? "a buffer and format" : format_index ? "a stream and format" : "a format");
            for (size_t i = 0; i < e->as.call.arguments.count; ++i) {
                Type *arg = check_expr(a, e->as.call.arguments.items[i]);
                if (!arg)
                    continue;
                if (i >= fixed && is_floating(arg->kind))
                    a->program->contains_formatted_output = true;
                if (i >= fixed && is_numeric(arg) && !is_floating(arg->kind))
                    a->program->contains_c_format_integer = true;
                if (format_index && i == 0) {
                    if (first && !assignable_expr(first, arg, e->as.call.arguments.items[i]))
                        report(a, e->as.call.arguments.items[i]->location,
                               "argument 1 to '%s' has incompatible %s type", s->name,
                               (is_snprintf || is_sprintf) ? "buffer" : "stream");
                    continue;
                }
                if (is_snprintf && i == 1) {
                    if (!is_numeric(arg) || is_floating(arg->kind))
                        report(a, e->as.call.arguments.items[i]->location,
                               "argument 2 to 'snprintf' must be an integer capacity");
                    continue;
                }
                bool string = arg->kind == TYPE_POINTER && arg->pointee &&
                              arg->pointee->kind == TYPE_BYTE;
                if ((i == format_index && !string) ||
                    (i > format_index && !string && !is_numeric(arg)))
                    report(a, e->as.call.arguments.items[i]->location,
                           "%s arguments must be numeric values or byte pointers", s->name);
            }
            check_printf_format_count(a, e, format_index, s->name);
            return &t_int;
        }
        size_t expected = d ? d->as.function.parameters.count : 0;
        if (d && (e->as.call.arguments.count < expected ||
                  (!d->as.function.variadic && expected != e->as.call.arguments.count)))
            report(a, e->location, d->as.function.variadic
                       ? "function '%s' expects at least %zu arguments but received %zu"
                       : "function '%s' expects %zu arguments but received %zu", s->name,
                   expected, e->as.call.arguments.count);
        for (size_t i = 0; i < e->as.call.arguments.count; i++) {
            Type *arg = check_expr(a, e->as.call.arguments.items[i]);
            if (d && i < expected && arg) {
                check_constant_range(a, d->as.function.parameters.items[i].type,
                                     e->as.call.arguments.items[i], "function argument");
                if (!assignable_expr(d->as.function.parameters.items[i].type, arg,
                                     e->as.call.arguments.items[i]))
                    report(a, e->as.call.arguments.items[i]->location,
                           "argument %zu to '%s' has incompatible type", i + 1, s->name);
            } else if (d && d->as.function.variadic && i >= expected &&
                       arg && arg->kind == TYPE_VOID)
                report(a, e->as.call.arguments.items[i]->location,
                       "variadic argument cannot have void type");
        }
        return d ? d->as.function.return_type : s->type;
    }
    case EXPR_INDEX: {
        Type *b = check_expr(a, e->as.index.base), *i = check_expr(a, e->as.index.index);
        if (b && b->kind != TYPE_POINTER)
            report(a, e->location, "indexing requires an array or pointer");
        if (b && b->kind == TYPE_POINTER && b->pointee && b->pointee->kind == TYPE_VOID) {
            report(a, e->location, "cannot index a void pointer");
            return NULL;
        }
        if (b && b->kind == TYPE_POINTER && b->pointee && b->pointee->kind == TYPE_FUNCTION) {
            report(a, e->location, "cannot index a function pointer");
            return NULL;
        }
        if (incomplete_structure_pointer(a, b) || incomplete_array_pointer(b)) {
            report(a, e->location, incomplete_array_pointer(b)
                   ? "cannot index a pointer to an incomplete array"
                   : "cannot index a pointer to an incomplete structure");
            return NULL;
        }
        if (i && (!is_numeric(i) || is_floating(i->kind)))
            report(a, e->as.index.index->location, "array index must be an integer");
        return b && b->kind == TYPE_POINTER ? b->pointee : NULL;
    }
    case EXPR_MEMBER: {
        Type *b = check_expr(a, e->as.member.base);
        if (!b)
            return NULL;
        if (e->as.member.through_pointer) {
            if (b->kind != TYPE_POINTER) {
                report(a, e->location, "pointer member access requires a pointer");
                return NULL;
            }
            b = b->pointee;
        }
        if (b && b->kind == TYPE_STRUCT && b->struct_name && a->current_structure &&
            !strcmp(b->struct_name, a->current_structure->as.structure.name)) {
            report(a, e->location, "member access on an incomplete structure");
            return NULL;
        }
        if (b->kind != TYPE_STRUCT || !b->struct_name) {
            report(a, e->location, "member access requires a structure value");
            return NULL;
        }
        Symbol *st = lookup_struct(a, b->struct_name, strlen(b->struct_name));
        if (!st || !st->declaration) {
            report(a, e->location, "member access on an incomplete structure");
            return NULL;
        }
        unsigned through_qualifiers = 0;
        Decl *f = find_visible_field(a, st->declaration, e->as.member.name,
                                     e->as.member.name_length, &through_qualifiers, 0);
        if (f) {
            e->semantic_bit_field = f->as.variable.is_bit_field;
            if (f->as.variable.is_bit_field && f->as.variable.bit_field_width_resolved)
                e->semantic_bit_field_width = f->as.variable.resolved_bit_field_width;
            return qualified_type(a, f->as.variable.type, b->is_const || (through_qualifiers & 1),
                                  b->is_volatile || (through_qualifiers & 2));
        }
        report(a, e->location, "structure '%s' has no member '%.*s'", b->struct_name,
               (int)e->as.member.name_length, e->as.member.name);
        return NULL;
    }
    case EXPR_CONDITIONAL: {
        Type *c = check_expr(a, e->as.conditional.condition);
        bool saved = a->skip_evaluation_checks, truth = false;
        bool known = a->program->c11_numeric && c &&
                     constant_truth(e->as.conditional.condition, &truth, 0, false);
        a->skip_evaluation_checks = saved || (known && !truth);
        Type *l = check_expr(a, e->as.conditional.then_expr);
        a->skip_evaluation_checks = saved || (known && truth);
        Type *r = check_expr(a, e->as.conditional.else_expr);
        a->skip_evaluation_checks = saved;
        if (c && !is_numeric(c) && c->kind != TYPE_POINTER)
            report(a, e->location, "conditional expression requires a scalar condition");
        if (l && r) {
            if (is_numeric(l) && is_numeric(r))
                return promote(l, r, a->program->c11_numeric);
            if (l->kind == TYPE_POINTER && r->kind == TYPE_POINTER &&
                l->pointee && r->pointee &&
                l->pointee->kind != TYPE_FUNCTION && r->pointee->kind != TYPE_FUNCTION &&
                (l->pointee->kind == TYPE_VOID || r->pointee->kind == TYPE_VOID)) {
                Type *pointee = l->pointee->kind == TYPE_VOID ? l->pointee : r->pointee;
                pointee = qualified_type(a, pointee,
                                         l->pointee->is_const || r->pointee->is_const,
                                         l->pointee->is_volatile || r->pointee->is_volatile);
                return owned_pointer_type(a, pointee);
            }
            if (l->kind == TYPE_POINTER && r->kind == TYPE_POINTER && type_equal(l, r)) {
                if (l->pointee && r->pointee &&
                    ((l->pointee->kind != TYPE_POINTER && l->pointee->kind != TYPE_ARRAY) ||
                     qualified_equal(l->pointee->pointee, r->pointee->pointee))) {
                    Type *pointee = l->pointee;
                    pointee = qualified_type(a, pointee, r->pointee->is_const,
                                             r->pointee->is_volatile);
                    return owned_pointer_type(a, pointee);
                }
                report(a, e->location, "conditional branches have incompatible pointer qualifiers");
                return l;
            }
            if (value_type_equal(l, r))
                return l;
            if (l->kind == TYPE_POINTER && is_null_literal(e->as.conditional.else_expr))
                return l;
            if (r->kind == TYPE_POINTER && is_null_literal(e->as.conditional.then_expr))
                return r;
            report(a, e->location, "conditional branches have incompatible types");
        }
        return l ? l : r;
    }
    case EXPR_CAST: {
        check_embedded_enum(a, e->as.cast.enum_definition);
        Type *to = e->as.cast.target_type;
        if (e->as.cast.operand && e->as.cast.operand->kind == EXPR_ARRAY_INIT) {
            validate_type(a, to, e->location);
            if (!to || to->kind == TYPE_VOID || to->kind == TYPE_FUNCTION) {
                report(a, e->location, "compound literals require an object type");
                return to;
            }
            e->as.cast.semantic_file_scope = a->at_file_scope;
            if (a->at_file_scope)
                a->program->contains_file_compound_literal = true;
            if (to->kind != TYPE_ARRAY && to->kind != TYPE_STRUCT) {
                check_scalar_compound_initializer(a, to, e->as.cast.operand);
                if (a->at_file_scope)
                    check_file_compound_constants(a, e->as.cast.operand, 0);
                return to;
            }
            Decl declaration = {0};
            declaration.kind = DECL_VARIABLE;
            declaration.as.variable.type = to;
            declaration.as.variable.initializer.kind = INIT_EXPRESSION;
            declaration.as.variable.initializer.expression = e->as.cast.operand;
            if (to->kind == TYPE_ARRAY &&
                e->as.cast.operand->as.array_init.count == 1 &&
                string_element_matches(to->pointee, e->as.cast.operand->as.array_init.items[0]))
                declaration.as.variable.initializer.expression =
                    e->as.cast.operand->as.array_init.items[0];
            declaration.location = e->location;
            if (to->kind == TYPE_ARRAY)
                resolve_fixed_array(a, &declaration, false, false);
            check_initializer(a, &declaration);
            e->as.cast.semantic_plan = declaration.as.variable.initializer.semantic_plan;
            if (a->at_file_scope)
                check_file_compound_constants(a, e->as.cast.operand, 0);
            return to;
        }
        Type *from = check_expr(a, e->as.cast.operand);
        validate_type(a, to, e->location);
        if (!to || !from)
            return to;
        if (to->kind == TYPE_VOID)
            return to;
        if (to->kind == TYPE_BOOL && from->kind == TYPE_POINTER)
            return to;
        if (from->kind == TYPE_POINTER && is_numeric(to) && !is_floating(to->kind)) {
            a->program->contains_pointer_to_integer = true;
            return to;
        }
        if (is_numeric(to) && is_numeric(from)) {
            if (!a->program->c11_numeric &&
                ((to->kind == TYPE_BYTE && from->kind == TYPE_REAL) ||
                 (to->kind == TYPE_REAL && from->kind == TYPE_BYTE)))
                report(a, e->location, "casts between char and double are not allowed");
            else
                check_constant_range(a, to, e->as.cast.operand, "explicit cast");
            return to;
        }
        if (to->kind == TYPE_POINTER && is_null_literal(e->as.cast.operand))
            return to;
        if (to->kind == TYPE_POINTER && from->kind == TYPE_POINTER)
            return to;
        if (to->kind == TYPE_POINTER && is_numeric(from) && !is_floating(from->kind))
            return to;
        if (!type_equal(to, from))
            report(a, e->location, "cast between these types is not supported");
        return to;
    }
    case EXPR_ARRAY_INIT:
        for (size_t i = 0; i < e->as.array_init.count; i++)
            check_expr(a, e->as.array_init.items[i]);
        return NULL;
    case EXPR_DESIGNATED:
        report(a, e->location, "designator requires an aggregate initializer");
        return NULL;
    }
    return NULL;
}

/**
 * @brief Type-checks an expression while bounding recursive AST traversal.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param e Expression node being analyzed or emitted.
 * @return Inferred type, or NULL when the expression is invalid or too deeply nested.
 */
static Type *check_expr_raw(Analyzer *a, Expr *e)
{
    Type *type;
    if (!e || a->out_of_memory)
        return NULL;
    e->semantic_c11_numeric = a->program->c11_numeric;
    e->semantic_bit_field = false;
    e->semantic_bit_field_width = 0;
    if (a->expression_depth >= SEMA_MAX_RECURSION_DEPTH) {
        if (!a->recursion_limit_reported) {
            report(a, e->location, "maximum semantic expression nesting depth exceeded");
            a->recursion_limit_reported = true;
        }
        return NULL;
    }
    size_t unsigned_before = a->unsigned_uses;
    size_t float_before = a->float_uses;
    size_t saved_offset = a->source_offset;
    a->source_offset = e->location.offset;
    ++a->expression_depth;
    type = check_expr_impl(a, e);
    --a->expression_depth;
    a->source_offset = saved_offset;
    e->semantic_type = type ? type_effective_kind(type) : TYPE_VOID;
    e->semantic_volatile_lvalue = type && type->is_volatile && is_lvalue(e);
    if (type && is_unsigned_kind(type_effective_kind(type)))
        ++a->unsigned_uses;
    if (type && (type->kind == TYPE_FLOAT || type->kind == TYPE_LONG_DOUBLE))
        ++a->float_uses;
    e->semantic_unsigned = a->unsigned_uses != unsigned_before;
    e->semantic_float = a->float_uses != float_before;
    return type;
}

/**
 * @brief Checks an expression and applies fixed-array-to-pointer conversion.
 * @param a Analyzer receiving diagnostics and owning converted types.
 * @param e Expression to check.
 * @return The expression's value type, or NULL on failure.
 */
static Type *check_expr(Analyzer *a, Expr *e)
{
    Type *type = check_expr_raw(a, e);
    if (type && type->kind == TYPE_UNSIGNED_INT && e->semantic_bit_field_width &&
        e->semantic_bit_field_width < 32) {
        type = &t_int;
        e->semantic_type = TYPE_INT;
    }
    if (type && (type->kind == TYPE_ARRAY || type->kind == TYPE_FUNCTION)) {
        Type *element = type->kind == TYPE_FUNCTION ? type :
                        qualified_type(a, type->pointee, type->is_const, type->is_volatile);
        type = owned_pointer_type(a, element);
        if (e)
            e->semantic_type = TYPE_POINTER;
    }
    return type;
}

/**
 * @brief Resolves and validates the bound of a fixed array declaration.
 * @param a Analyzer receiving diagnostics.
 * @param d Array declaration to resolve.
 * @param allow_incomplete Whether a file-scope incomplete array is permitted.
 * @param allow_zero_member Whether a zero bound is allowed for this structure field.
 * @return No value; records the resolved bound on the type.
 */
static void resolve_fixed_array(Analyzer *a, Decl *d, bool allow_incomplete,
                                bool allow_zero_member)
{
    Type *type = d->as.variable.type;
    if (!type || type->kind != TYPE_ARRAY)
        return;
    Expr *bound = d->as.variable.array_size;
    Expr *init = d->as.variable.initializer.expression;
    if (init && init->kind == EXPR_ARRAY_INIT && init->as.array_init.count == 1 &&
        string_element_matches(type->pointee, init->as.array_init.items[0]))
        init = init->as.array_init.items[0];
    if (bound) {
        Type *count_type = check_expr(a, bound);
        IntegerValue value;
        if (!count_type || !is_numeric(count_type) || is_floating(count_type->kind) ||
            !integer_constant(bound, &value, 0) || (!value.bits && !allow_zero_member) ||
            (!is_unsigned_kind(value.kind) && signed_bits(value.bits) < 0) ||
            value.bits > SIZE_MAX) {
            report(a, bound->location, "fixed array bound must be a positive integer constant");
            return;
        }
        type->array_length = (size_t)value.bits;
        type->is_zero_length_array = !value.bits;
    } else if (type->array_length) {
        /* An array typedef already supplies the bound. */
    } else if (init && init->kind == EXPR_ARRAY_INIT) {
        type->array_length = (has_designator(init, 0) ||
                              contains_bit_field(a, type->pointee, 0))
                                 ? process_designated_initializer(a, d, false)
                             : type->pointee && (type->pointee->kind == TYPE_STRUCT ||
                                                 type->pointee->kind == TYPE_ARRAY)
                                 ? infer_structure_array_count(a, type->pointee, init)
                                 : init->as.array_init.count;
    } else if (string_element_matches(type->pointee, init)) {
        type->array_length = string_array_count(init);
    } else if (allow_incomplete && !init) {
        size_t size, alignment;
        if (!type_layout_size(type->pointee, layout_find_struct, a, &size, &alignment))
            report(a, d->location, "fixed array has an incomplete element type or unsupported size");
        return;
    } else {
        report(a, d->location, "cannot infer fixed array size from this initializer");
        return;
    }
    if (!type->array_length && !type->is_zero_length_array) {
        report(a, d->location, "fixed array must have at least one element");
        return;
    }
    size_t size, alignment;
    if (!type_layout_size(type, layout_find_struct, a, &size, &alignment) ||
        size > INT64_MAX)
        report(a, d->location, "fixed array has an incomplete element type or unsupported size");
}

/**
 * @brief Creates and enters a nested lexical scope.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool push_scope(Analyzer *a)
{
    Scope *s = analyzer_alloc(a, sizeof(*s));
    if (!s)
        return false;
    s->parent = a->scope;
    a->scope = s;
    return true;
}
/**
 * @brief Warns about unused symbols and frees a symbol list.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param p Head of the owned symbol list to release; NULL is allowed.
 * @return No value; releases symbol records, not their borrowed names or AST declarations.
 */
static void release_symbols(Analyzer *a, Symbol *p)
{
    while (p) {
        Symbol *n = p->next;
        if (a->warn_unused_variable && p->warn_unused && !p->used)
            warn(a, p->location, "unused variable '%.*s'", (int)p->length, p->name);
        free(p);
        p = n;
    }
}
/**
 * @brief Leaves the current lexical scope and releases its symbols.
 * @param a Analyzer whose current scope is removed.
 * @return No value; emits unused-variable warnings before releasing the scope.
 */
static void pop_scope(Analyzer *a)
{
    if (!a->scope)
        return;
    Scope *s = a->scope;
    a->scope = s->parent;
    release_symbols(a, s->symbols);
    free(s);
}

/**
 * @brief Records the variable-length arrays visible at a control-flow point.
 * @param a Analyzer holding the current lexical scope.
 * @param bits Output bitset indexed by the function's VLA identifiers.
 * @return No value.
 */
static void active_vlas(Analyzer *a, uint64_t bits[8])
{
    memset(bits, 0, 8 * sizeof(*bits));
    for (Scope *scope = a->scope; scope; scope = scope->parent)
        for (Symbol *symbol = scope->symbols; symbol; symbol = symbol->next) {
            Decl *d = symbol->declaration;
            if (d && d->kind == DECL_VARIABLE &&
                (d->as.variable.semantic_vla || d->as.variable.semantic_vla_pointer) &&
                d->as.variable.semantic_vla_id < 512) {
                unsigned id = d->as.variable.semantic_vla_id;
                bits[id / 64] |= UINT64_C(1) << (id % 64);
            }
        }
}
/**
 * @brief Determines whether a statement contains a named entry label.
 * @param s Statement to inspect.
 * @param depth Current nesting depth.
 * @return True when a label may be present, including at the recursion limit.
 */
static bool contains_entry_label(const Stmt *s, unsigned depth)
{
    if (!s)
        return false;
    if (depth >= SEMA_MAX_RECURSION_DEPTH)
        return true;
    switch (s->kind) {
    case STMT_LABEL:
        return true;
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.statements.count; ++i)
            if (contains_entry_label(s->as.block.statements.items[i], depth + 1))
                return true;
        return false;
    case STMT_IF:
        return contains_entry_label(s->as.if_stmt.then_branch, depth + 1) ||
               contains_entry_label(s->as.if_stmt.else_branch, depth + 1);
    case STMT_SWITCH:
        for (size_t i = 0; i < s->as.switch_stmt.prefix.count; ++i)
            if (contains_entry_label(s->as.switch_stmt.prefix.items[i], depth + 1))
                return true;
        for (size_t i = 0; i < s->as.switch_stmt.cases.count; ++i) {
            const StmtArray *statements = &s->as.switch_stmt.cases.items[i].statements;
            for (size_t j = 0; j < statements->count; ++j)
                if (contains_entry_label(statements->items[j], depth + 1))
                    return true;
        }
        return false;
    case STMT_FOR:
        return contains_entry_label(s->as.for_stmt.initializer, depth + 1) ||
               contains_entry_label(s->as.for_stmt.body, depth + 1);
    case STMT_LOOP:
    case STMT_DO:
        return contains_entry_label(s->as.loop_stmt.body, depth + 1);
    default:
        return false;
    }
}
/**
 * @brief Semantically checks a statement list and warns about unreachable code.
 * @param a Analyzer used for checks and diagnostics.
 * @param items Statements to check in source order.
 * @param count Number of entries in items.
 * @return No value; updates analyzer state and records diagnostics.
 */
static void check_stmt_list(Analyzer *a, Stmt **items, size_t count)
{
    bool reported = false;
    for (size_t i = 0; i < count && !a->out_of_memory; i++) {
        bool entry_label = contains_entry_label(items[i], 0);
        if (entry_label)
            reported = false;
        if (i > 0 && !reported) {
            Stmt *prev = items[i - 1];
            if (prev && (prev->kind == STMT_RETURN || prev->kind == STMT_BREAK ||
                         prev->kind == STMT_CONTINUE) &&
                items[i] && !entry_label) {
                warn(a, items[i]->location, "unreachable code");
                reported = true;
            }
        }
        check_stmt(a, items[i]);
    }
}

typedef struct {
    Expr *expr;
    Type *type;
} InitializerExprCache;

/**
 * @brief Identifies expressions that can have structure type.
 * @param value Expression to inspect.
 * @return True when the expression can denote a structure value.
 */
static bool may_be_structure_value(const Expr *value)
{
    return value && (value->kind == EXPR_NAME || value->kind == EXPR_INDEX ||
                     value->kind == EXPR_MEMBER || value->kind == EXPR_CALL ||
                     value->kind == EXPR_ASSIGN || value->kind == EXPR_CONDITIONAL ||
                     value->kind == EXPR_CAST ||
                     (value->kind == EXPR_BINARY && value->as.binary.op == BINARY_COMMA) ||
                     (value->kind == EXPR_UNARY &&
                      value->as.unary.op == UNARY_DEREFERENCE));
}

/**
 * @brief Looks up an initializer expression's type without evaluating it.
 * @param a Analyzer holding the declarations visible at this point.
 * @param value Expression to inspect.
 * @param depth Current expression nesting depth.
 * @return Borrowed type when it can be determined, otherwise NULL.
 */
static Type *initializer_shape_type(Analyzer *a, const Expr *value, unsigned depth)
{
    if (!value || depth >= SEMA_MAX_RECURSION_DEPTH)
        return NULL;
    switch (value->kind) {
    case EXPR_NAME: {
        Symbol *symbol = lookup_scope(a->scope, value->as.name.name, value->as.name.length);
        if (!symbol)
            symbol = lookup_global(a, value->as.name.name, value->as.name.length);
        return symbol ? symbol->type : NULL;
    }
    case EXPR_INDEX: {
        const Expr *base = value->as.index.base;
        if (base && base->kind == EXPR_UNARY && base->as.unary.op == UNARY_ADDRESS)
            return initializer_shape_type(a, base->as.unary.operand, depth + 1);
        Type *type = initializer_shape_type(a, base, depth + 1);
        return type && (type->kind == TYPE_ARRAY || type->kind == TYPE_POINTER)
                   ? type->pointee : NULL;
    }
    case EXPR_UNARY: {
        if (value->as.unary.op == UNARY_ADDRESS) {
            Type *type = initializer_shape_type(a, value->as.unary.operand, depth + 1);
            return type ? owned_pointer_type(a, type) : NULL;
        }
        if (value->as.unary.op != UNARY_DEREFERENCE)
            return NULL;
        const Expr *operand = value->as.unary.operand;
        if (operand && operand->kind == EXPR_UNARY &&
            operand->as.unary.op == UNARY_ADDRESS)
            return initializer_shape_type(a, operand->as.unary.operand, depth + 1);
        Type *type = initializer_shape_type(a, operand, depth + 1);
        return type && (type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY)
                   ? type->pointee : NULL;
    }
    case EXPR_BINARY: {
        if (value->as.binary.op == BINARY_COMMA)
            return initializer_shape_type(a, value->as.binary.right, depth + 1);
        if (value->as.binary.op != BINARY_ADD && value->as.binary.op != BINARY_SUBTRACT)
            return NULL;
        Type *left = initializer_shape_type(a, value->as.binary.left, depth + 1);
        if (left && (left->kind == TYPE_POINTER || left->kind == TYPE_ARRAY))
            return left;
        Type *right = initializer_shape_type(a, value->as.binary.right, depth + 1);
        return right && (right->kind == TYPE_POINTER || right->kind == TYPE_ARRAY) ? right : NULL;
    }
    case EXPR_CAST:
        return value->as.cast.target_type;
    case EXPR_MEMBER: {
        Type *type = initializer_shape_type(a, value->as.member.base, depth + 1);
        if (type && value->as.member.through_pointer && type->kind == TYPE_POINTER)
            type = type->pointee;
        if (!type || type->kind != TYPE_STRUCT || !type->struct_name)
            return NULL;
        Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
        unsigned through_qualifiers = 0;
        Decl *field = symbol ? find_visible_field(a, symbol->declaration, value->as.member.name,
                                                   value->as.member.name_length,
                                                   &through_qualifiers, 0) : NULL;
        return field ? field->as.variable.type : NULL;
    }
    case EXPR_ASSIGN:
        return initializer_shape_type(a, value->as.assign.target, depth + 1);
    case EXPR_CONDITIONAL:
        return initializer_shape_type(a, value->as.conditional.then_expr, depth + 1);
    case EXPR_CALL: {
        const Expr *callee = value->as.call.callee;
        if (!callee || callee->kind != EXPR_NAME)
            return NULL;
        Symbol *symbol = lookup_scope(a->scope, callee->as.name.name, callee->as.name.length);
        if (!symbol)
            symbol = lookup_global(a, callee->as.name.name, callee->as.name.length);
        return symbol && symbol->kind == SEMANTIC_SYMBOL_FUNCTION && symbol->declaration
                   ? symbol->declaration->as.function.return_type : NULL;
    }
    default:
        return NULL;
    }
}

/**
 * @brief Consumes one subobject from a positional initializer list.
 * @param a Analyzer receiving diagnostics.
 * @param type Destination subobject type.
 * @param items Current brace-level initializer list.
 * @param index Next item in the list, updated on consumption.
 * @param depth Aggregate nesting depth.
 * @param validate Whether to check conversions and list constraints.
 * @param cache Previously checked expression at the current cursor.
 * @return No value; advances the list cursor.
 */
static void consume_initializer(Analyzer *a, Type *type, ExprArray *items, size_t *index,
                                unsigned depth, bool validate, InitializerExprCache *cache)
{
    if (!type || *index >= items->count || depth >= SEMA_MAX_RECURSION_DEPTH) {
        if (type && *index < items->count && validate)
            report(a, items->items[*index]->location, "initializer nesting is too deep");
        return;
    }
    Expr *value = items->items[*index];
    bool aggregate = type->kind == TYPE_STRUCT || type->kind == TYPE_ARRAY;
    if (type->kind == TYPE_ARRAY && string_element_matches(type->pointee, value)) {
        ++*index;
        if (validate && string_array_count(value) - 1 > type->array_length)
            report(a, value->location, "string is too long for fixed char array");
        return;
    }
    if (type->kind == TYPE_STRUCT && may_be_structure_value(value)) {
        if (cache->expr != value) {
            cache->expr = value;
            cache->type = validate ? check_expr(a, value)
                                   : initializer_shape_type(a, value, 0);
        }
        if (cache->type && cache->type->kind == TYPE_STRUCT &&
            type_equal(type, cache->type)) {
            ++*index;
            cache->expr = NULL;
            return;
        }
    }
    if (value->kind == EXPR_ARRAY_INIT) {
        ++*index;
        ExprArray *nested = &value->as.array_init;
        size_t cursor = 0;
        if (!nested->count) {
            if (validate)
                report(a, value->location, "empty initializer is not supported");
            return;
        }
        if (!aggregate) {
            consume_initializer(a, type, nested, &cursor, depth + 1, validate, cache);
        } else if (type->kind == TYPE_ARRAY &&
                   string_element_matches(type->pointee, nested->items[0])) {
            consume_initializer(a, type, nested, &cursor, depth + 1, validate, cache);
        } else {
            Decl *structure = NULL;
            size_t count = type->array_length;
            if (type->kind == TYPE_STRUCT) {
                Symbol *symbol = type->struct_name
                                     ? lookup_struct(a, type->struct_name, strlen(type->struct_name))
                                     : NULL;
                structure = symbol ? symbol->declaration : NULL;
                if (!structure)
                    return;
                count = structure->as.structure.is_union ? 1 : structure->as.structure.field_count;
                if (!structure->as.structure.is_union && count &&
                    field_is_zero_storage(structure->as.structure.fields[count - 1]))
                    --count;
            }
            for (size_t i = 0; i < count && cursor < nested->count; ++i) {
                Type *element = structure ? structure->as.structure.fields[i]->as.variable.type
                                          : type->pointee;
                size_t before = cursor;
                consume_initializer(a, element, nested, &cursor, depth + 1, validate, cache);
                if (cursor == before)
                    break;
            }
        }
        if (validate && cursor < nested->count)
            report(a, nested->items[cursor]->location, "too many aggregate initializers");
        return;
    }
    if (aggregate) {
        Decl *structure = NULL;
        size_t count = type->array_length;
        if (type->kind == TYPE_STRUCT) {
            Symbol *symbol = type->struct_name
                                 ? lookup_struct(a, type->struct_name, strlen(type->struct_name))
                                 : NULL;
            structure = symbol ? symbol->declaration : NULL;
            if (!structure)
                return;
            count = structure->as.structure.is_union ? 1 : structure->as.structure.field_count;
            if (!structure->as.structure.is_union && count &&
                field_is_zero_storage(structure->as.structure.fields[count - 1]))
                --count;
        }
        for (size_t i = 0; i < count && *index < items->count; ++i) {
            Type *element = structure ? structure->as.structure.fields[i]->as.variable.type
                                      : type->pointee;
            size_t before = *index;
            consume_initializer(a, element, items, index, depth + 1, validate, cache);
            if (*index == before)
                break;
        }
        return;
    }
    ++*index;
    if (validate) {
        Type *actual = cache->expr == value ? cache->type : check_expr(a, value);
        if (actual) {
            check_constant_range(a, type, value, "initializer");
            if (!assignable_expr(type, actual, value))
                report(a, value->location, "initializer element has incompatible type");
        }
    }
    cache->expr = NULL;
}

/**
 * @brief Infers the number of structure elements consumed by a flat list.
 * @param a Semantic analyzer.
 * @param element Array element type.
 * @param init Array initializer list.
 * @return Number of initialized elements.
 */
static size_t infer_structure_array_count(Analyzer *a, Type *element, Expr *init)
{
    size_t index = 0, count = 0;
    InitializerExprCache cache = {0};
    while (index < init->as.array_init.count) {
        size_t before = index;
        consume_initializer(a, element, &init->as.array_init, &index, 0, false, &cache);
        if (index == before)
            break;
        ++count;
    }
    return count;
}

typedef struct {
    Type *type;
    size_t index;
    size_t offset;
    bool bit_field;
    unsigned bit_offset;
    unsigned bit_width;
} InitializerPosition;

typedef struct {
    InitializerPosition path[SEMA_MAX_RECURSION_DEPTH];
    size_t depth;
    bool valid;
} InitializerCursor;

typedef struct {
    const char *name;
    size_t offset;
    size_t index;
} ActiveInitializerUnion;

typedef struct {
    Analyzer *analyzer;
    InitializerPlan *plan;
    Type *root;
    size_t inferred_count;
    bool validate;
    ActiveInitializerUnion *unions;
    size_t union_count;
} DesignatedContext;

/**
 * @brief Identifies bit-fields in an aggregate or its by-value subobjects.
 * @param a Analyzer holding structure declarations.
 * @param type Type to inspect.
 * @param depth Current nesting depth.
 * @return True when the type contains a bit-field.
 */
static bool contains_bit_field(Analyzer *a, const Type *type, unsigned depth)
{
    if (!type || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    if (type->kind == TYPE_ARRAY)
        return contains_bit_field(a, type->pointee, depth + 1);
    if (type->kind != TYPE_STRUCT || !type->struct_name)
        return false;
    Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
    Decl *structure = symbol ? symbol->declaration : NULL;
    if (!structure)
        return false;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        Decl *field = structure->as.structure.fields[i];
        if (field->as.variable.is_bit_field ||
            contains_bit_field(a, field->as.variable.type, depth + 1))
            return true;
    }
    return false;
}

/**
 * @brief Detects designated elements in an initializer tree.
 * @param value Initializer expression.
 * @param depth Current nesting depth.
 * @return True when a designated element occurs.
 */
static bool has_designator(const Expr *value, unsigned depth)
{
    if (!value || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    if (value->kind == EXPR_DESIGNATED)
        return true;
    if (value->kind == EXPR_ARRAY_INIT)
        for (size_t i = 0; i < value->as.array_init.count; ++i)
            if (has_designator(value->as.array_init.items[i], depth + 1))
                return true;
    return false;
}

/**
 * @brief Selects one direct subobject and records its layout position.
 * @param context Designated initializer state.
 * @param cursor Current path into the initialized aggregate.
 * @param index Direct child index to select.
 * @return True when the child exists and its layout is supported.
 */
static bool initializer_child(DesignatedContext *context, InitializerCursor *cursor, size_t index)
{
    Analyzer *a = context->analyzer;
    InitializerPosition *parent = &cursor->path[cursor->depth];
    Type *type = parent->type, *child = NULL;
    size_t offset = 0, size, alignment;
    bool bit_field = false;
    unsigned bit_offset = 0, bit_width = 0;
    if (cursor->depth + 1 >= SEMA_MAX_RECURSION_DEPTH || !type)
        return false;
    if (type->kind == TYPE_ARRAY) {
        if ((type->array_length && index >= type->array_length) ||
            (!type->array_length && (type != context->root || context->validate)) ||
            !type_layout_size(type->pointee, layout_find_struct, a, &size, &alignment) ||
            !size || index > SIZE_MAX / size)
            return false;
        child = type->pointee;
        offset = index * size;
    } else if (type->kind == TYPE_STRUCT && type->struct_name) {
        Symbol *symbol = lookup_struct(a, type->struct_name, strlen(type->struct_name));
        Decl *structure = symbol ? symbol->declaration : NULL;
        FieldLayout fields[256];
        if (!structure || structure->as.structure.field_count > 256 ||
            index >= structure->as.structure.field_count ||
            field_is_zero_storage(structure->as.structure.fields[index]) ||
            !type_layout_struct_fields(structure, layout_find_struct, a, fields, &size,
                                       &alignment))
            return false;
        child = structure->as.structure.fields[index]->as.variable.type;
        offset = fields[index].offset;
        bit_field = structure->as.structure.fields[index]->as.variable.is_bit_field;
        bit_offset = fields[index].bit_offset;
        bit_width = fields[index].bit_width;
    } else
        return false;
    if (parent->offset > SIZE_MAX - offset)
        return false;
    cursor->path[++cursor->depth] = (InitializerPosition){child, index, parent->offset + offset,
                                                          bit_field, bit_offset, bit_width};
    cursor->valid = true;
    return true;
}

/**
 * @brief Selects the next initializable child, skipping unnamed bit-fields.
 * @param context Designated initializer state.
 * @param cursor Current path into the initialized aggregate.
 * @param index First child index to consider.
 * @return True when a child was selected.
 */
static bool initializer_next_child(DesignatedContext *context, InitializerCursor *cursor,
                                   size_t index)
{
    Type *type = cursor->path[cursor->depth].type;
    if (type && type->kind == TYPE_STRUCT && type->struct_name) {
        Symbol *symbol = lookup_struct(context->analyzer, type->struct_name,
                                       strlen(type->struct_name));
        Decl *structure = symbol ? symbol->declaration : NULL;
        if (!structure)
            return false;
        while (index < structure->as.structure.field_count) {
            Decl *field = structure->as.structure.fields[index];
            if (!field->as.variable.is_bit_field || field->as.variable.name)
                break;
            ++index;
        }
    }
    return initializer_child(context, cursor, index);
}

/**
 * @brief Advances to the next subobject within a brace-enclosed aggregate.
 * @param context Designated initializer state.
 * @param cursor Current subobject path.
 * @return No value; marks the cursor invalid after the final subobject.
 */
static void initializer_advance(DesignatedContext *context, InitializerCursor *cursor)
{
    while (cursor->depth) {
        size_t index = cursor->path[cursor->depth].index;
        --cursor->depth;
        if (cursor->path[cursor->depth].type->kind == TYPE_STRUCT &&
            cursor->path[cursor->depth].type->is_union)
            continue;
        if (index != SIZE_MAX && initializer_next_child(context, cursor, index + 1))
            return;
    }
    cursor->valid = false;
}

/**
 * @brief Selects a named field, following anonymous structure members.
 * @param context Designated initializer state.
 * @param cursor Current structure position.
 * @param name Field name.
 * @param length Field name length.
 * @param depth Anonymous-member nesting depth.
 * @return True when the field is found.
 */
static bool initializer_field(DesignatedContext *context, InitializerCursor *cursor,
                              const char *name, size_t length, unsigned depth)
{
    Type *type = cursor->path[cursor->depth].type;
    if (!type || type->kind != TYPE_STRUCT || !type->struct_name ||
        depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    Symbol *symbol = lookup_struct(context->analyzer, type->struct_name, strlen(type->struct_name));
    Decl *structure = symbol ? symbol->declaration : NULL;
    if (!structure)
        return false;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        Decl *field = structure->as.structure.fields[i];
        if (field->as.variable.name && !field->as.variable.anonymous_member &&
            same_name(field->as.variable.name, field->as.variable.name_length, name, length))
            return initializer_child(context, cursor, i);
        if (field->as.variable.anonymous_member) {
            size_t saved = cursor->depth;
            if (initializer_child(context, cursor, i) &&
                initializer_field(context, cursor, name, length, depth + 1))
                return true;
            cursor->depth = saved;
        }
    }
    return false;
}

/**
 * @brief Appends an operation to a checked initializer plan.
 * @param context Designated initializer state.
 * @param position Destination subobject.
 * @param value Initializer expression, or NULL for a clear operation.
 * @return True when the operation was recorded.
 */
static bool append_initializer_operation(DesignatedContext *context, InitializerPosition *position,
                                         Expr *value)
{
    if (!context->validate)
        return true;
    InitializerPlan *plan = context->plan;
    size_t size, alignment;
    if (position->bit_field)
        size = (position->bit_offset + position->bit_width + 7) / 8;
    else if (!type_layout_size(position->type, layout_find_struct, context->analyzer,
                               &size, &alignment)) {
        report(context->analyzer, value ? value->location : (SourceLocation){0},
               "unsupported initializer subobject layout");
        return false;
    }
    if (plan->count == SIZE_MAX || plan->count + 1 > SIZE_MAX / sizeof(*plan->items)) {
        context->analyzer->out_of_memory = true;
        return false;
    }
    InitializerOperation *items = realloc(plan->items, (plan->count + 1) * sizeof(*items));
    if (!items) {
        context->analyzer->out_of_memory = true;
        return false;
    }
    plan->items = items;
    items[plan->count++] = (InitializerOperation){position->type, value, position->offset,
                                                   size, value == NULL, position->bit_field,
                                                   position->bit_offset, position->bit_width};
    return true;
}

/**
 * @brief Records an operation, clearing overridden aggregates and switched unions.
 * @param context Designated initializer state.
 * @param cursor Path to the destination.
 * @param position Destination subobject.
 * @param value Initializer expression, or NULL for a clear operation.
 * @return True when the operations were recorded.
 */
static bool initializer_operation(DesignatedContext *context, InitializerCursor *cursor,
                                  InitializerPosition *position, Expr *value)
{
    if (!context->validate)
        return true;
    /* A later subobject initializer overrides a prior whole-aggregate copy. */
    for (size_t i = 0; i < cursor->depth; ++i) {
        InitializerPosition *parent = &cursor->path[i];
        size_t size, alignment;
        if (!parent->type ||
            (parent->type->kind != TYPE_STRUCT && parent->type->kind != TYPE_ARRAY) ||
            !type_layout_size(parent->type, layout_find_struct, context->analyzer,
                              &size, &alignment))
            continue;
        for (size_t j = context->plan->count; j > 0; --j) {
            InitializerOperation *previous = &context->plan->items[j - 1];
            if (previous->bit_field || !previous->type ||
                (previous->type->kind != TYPE_STRUCT && previous->type->kind != TYPE_ARRAY) ||
                (!previous->clear && previous->type->kind != TYPE_STRUCT) ||
                previous->offset > parent->offset ||
                parent->offset - previous->offset > previous->size ||
                size > previous->size - (parent->offset - previous->offset))
                continue;
            if (!previous->clear && !append_initializer_operation(context, parent, NULL))
                return false;
            break;
        }
    }
    for (size_t i = 0; i < cursor->depth; ++i) {
        InitializerPosition *parent = &cursor->path[i];
        Type *type = parent->type;
        if (!type || type->kind != TYPE_STRUCT || !type->is_union || !type->struct_name)
            continue;
        size_t index = cursor->path[i + 1].index, j;
        for (j = 0; j < context->union_count; ++j)
            if (context->unions[j].offset == parent->offset &&
                !strcmp(context->unions[j].name, type->struct_name))
                break;
        if (j == context->union_count) {
            if (j == SIZE_MAX || j + 1 > SIZE_MAX / sizeof(*context->unions)) {
                context->analyzer->out_of_memory = true;
                return false;
            }
            ActiveInitializerUnion *unions =
                realloc(context->unions, (j + 1) * sizeof(*unions));
            if (!unions) {
                context->analyzer->out_of_memory = true;
                return false;
            }
            context->unions = unions;
            context->unions[j] = (ActiveInitializerUnion){type->struct_name, parent->offset, index};
            ++context->union_count;
        } else if (context->unions[j].index != index) {
            if (!append_initializer_operation(context, parent, NULL))
                return false;
            context->unions[j].index = index;
        }
    }
    return append_initializer_operation(context, position, value);
}

/**
 * @brief Resolves a designator chain from a brace-level root.
 * @param context Designated initializer state.
 * @param cursor Root path, replaced by the selected subobject path.
 * @param value Designated initializer node.
 * @return True when all designators are valid.
 */
static bool initializer_designate(DesignatedContext *context, InitializerCursor *cursor,
                                  Expr *value)
{
    Analyzer *a = context->analyzer;
    cursor->depth = 0;
    cursor->valid = true;
    for (size_t i = 0; i < value->as.designated.count; ++i) {
        InitializerDesignator *d = &value->as.designated.items[i];
        if (d->field) {
            if (!initializer_field(context, cursor, d->field, d->field_length, 0)) {
                report(a, d->location, "unknown or invalid field designator '%s'", d->field);
                return false;
            }
        } else {
            Type *type = d->index ? check_expr(a, d->index) : NULL;
            IntegerValue number;
            if (!type || !is_numeric(type) || is_floating(type->kind) ||
                !integer_constant(d->index, &number, 0) ||
                (!is_unsigned_kind(number.kind) && signed_bits(number.bits) < 0) ||
                number.bits >= SIZE_MAX) {
                report(a, d->location, "array designator requires a nonnegative integer constant");
                return false;
            }
            d->resolved_index = (size_t)number.bits;
            Type *current = cursor->path[cursor->depth].type;
            if (!current || current->kind != TYPE_ARRAY ||
                !initializer_child(context, cursor, d->resolved_index)) {
                report(a, d->location, "array designator is out of bounds or not an array");
                return false;
            }
        }
    }
    return true;
}

static void designated_list(DesignatedContext *, Type *, size_t, Expr *, unsigned);

/**
 * @brief Processes one brace-enclosed designated initializer list.
 * @param context Designated initializer state.
 * @param type Brace-level destination type.
 * @param offset Byte offset of the destination.
 * @param list Brace-enclosed initializer expression.
 * @param depth Current brace nesting depth.
 * @return No value; records diagnostics and initialization operations.
 */
static void designated_list(DesignatedContext *context, Type *type, size_t offset, Expr *list,
                            unsigned depth)
{
    Analyzer *a = context->analyzer;
    if (depth >= SEMA_MAX_RECURSION_DEPTH) {
        report(a, list->location, "initializer nesting is too deep");
        return;
    }
    if (!list->as.array_init.count) {
        report(a, list->location, "empty initializer is not supported");
        return;
    }
    InitializerCursor cursor = {0};
    cursor.path[0] = (InitializerPosition){type, 0, offset, false, 0, 0};
    cursor.valid = initializer_next_child(context, &cursor, 0);
    for (size_t i = 0; i < list->as.array_init.count && !a->out_of_memory; ++i) {
        Expr *value = list->as.array_init.items[i];
        if (value->kind == EXPR_DESIGNATED) {
            if (!initializer_designate(context, &cursor, value))
                continue;
            value = value->as.designated.value;
        }
        if (!cursor.valid || !value) {
            report(a, list->as.array_init.items[i]->location, "too many aggregate initializers");
            continue;
        }
        if (!context->validate && a->at_file_scope && value->kind != EXPR_ARRAY_INIT)
            scan_embedded_enums(a, value, 0);
        if (type == context->root && type->kind == TYPE_ARRAY && cursor.depth &&
            cursor.path[1].index >= context->inferred_count)
            context->inferred_count = cursor.path[1].index + 1;
        Type *actual = NULL;
        Expr *checked = NULL;
        for (;;) {
            InitializerPosition *position = &cursor.path[cursor.depth];
            Type *target = position->type;
            bool aggregate = target->kind == TYPE_STRUCT || target->kind == TYPE_ARRAY;
            if (value->kind == EXPR_ARRAY_INIT) {
                if (!aggregate) {
                    if (value->as.array_init.count != 1) {
                        report(a, value->location, "scalar initializer requires exactly one value");
                        break;
                    }
                    value = value->as.array_init.items[0];
                    continue;
                }
                if (!initializer_operation(context, &cursor, position, NULL))
                    break;
                if (target->kind == TYPE_ARRAY &&
                    value->as.array_init.count == 1 &&
                    string_element_matches(target->pointee, value->as.array_init.items[0])) {
                    value = value->as.array_init.items[0];
                    continue;
                }
                designated_list(context, target, position->offset, value, depth + 1);
                break;
            }
            if (target->kind == TYPE_ARRAY && string_element_matches(target->pointee, value)) {
                if (string_array_count(value) - 1 > target->array_length)
                    report(a, value->location, "string is too long for fixed char array");
                initializer_operation(context, &cursor, position, NULL);
                initializer_operation(context, &cursor, position, value);
                break;
            }
            if (aggregate && target->kind == TYPE_STRUCT && may_be_structure_value(value)) {
                if (checked != value) {
                    checked = value;
                    actual = context->validate ? check_expr(a, value)
                                               : initializer_shape_type(a, value, 0);
                }
                if (actual && actual->kind == TYPE_STRUCT && type_equal(target, actual)) {
                    if (!context->validate && !a->at_file_scope)
                        check_expr(a, value);
                    initializer_operation(context, &cursor, position, value);
                    break;
                }
            }
            if (aggregate) {
                if (!initializer_next_child(context, &cursor, 0)) {
                    report(a, value->location, "unsupported aggregate initializer");
                    break;
                }
                continue;
            }
            if (context->validate || !a->at_file_scope) {
                if (!context->validate || checked != value)
                    actual = check_expr(a, value);
                if (context->validate && actual) {
                    check_constant_range(a, target, value, "initializer");
                    if (!assignable_expr(target, actual, value))
                        report(a, value->location, "initializer element has incompatible type");
                }
            }
            initializer_operation(context, &cursor, position, value);
            break;
        }
        initializer_advance(context, &cursor);
    }
}

/**
 * @brief Infers array bounds or builds the checked designated initializer plan.
 * @param a Semantic analyzer.
 * @param declaration Variable whose initializer is being processed.
 * @param validate Whether to validate and retain a plan.
 * @return Inferred number of top-level array elements.
 */
static size_t process_designated_initializer(Analyzer *a, Decl *declaration, bool validate)
{
    Initializer *initializer = &declaration->as.variable.initializer;
    DesignatedContext context = {0};
    context.analyzer = a;
    context.root = declaration->as.variable.type;
    context.validate = validate;
    if (validate) {
        OwnedInitializerPlan *owned = analyzer_alloc(a, sizeof(*owned));
        if (!owned)
            return 0;
        owned->next = a->model->initializer_plans;
        a->model->initializer_plans = owned;
        initializer->semantic_plan = &owned->plan;
        context.plan = &owned->plan;
    }
    bool previous_inference = a->initializer_inference;
    a->initializer_inference = !validate;
    designated_list(&context, context.root, 0, initializer->expression, 0);
    a->initializer_inference = previous_inference;
    free(context.unions);
    return context.inferred_count;
}

/**
 * @brief Type-checks a declaration initializer.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param d Declaration node being analyzed or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void check_initializer(Analyzer *a, Decl *d)
{
    Expr *e = d->as.variable.initializer.expression;
    if (!e)
        return;
    Type *array = d->as.variable.type;
    if (array && array->kind == TYPE_ARRAY && e->kind == EXPR_ARRAY_INIT &&
        e->as.array_init.count == 1 &&
        string_element_matches(array->pointee, e->as.array_init.items[0]))
        e = e->as.array_init.items[0];
    d->as.variable.initializer.semantic_plan = NULL;
    if (e->kind == EXPR_ARRAY_INIT &&
        (has_designator(e, 0) || contains_bit_field(a, d->as.variable.type, 0))) {
        if (!d->as.variable.type ||
            (d->as.variable.type->kind != TYPE_ARRAY && d->as.variable.type->kind != TYPE_STRUCT))
            report(a, e->location, "designators require an array or structure");
        else
            process_designated_initializer(a, d, true);
        return;
    }

    if (d->as.variable.type && e->kind == EXPR_ARRAY_INIT &&
        (d->as.variable.type->kind == TYPE_STRUCT ||
         (d->as.variable.type->kind == TYPE_ARRAY && d->as.variable.type->pointee &&
          (d->as.variable.type->pointee->kind == TYPE_STRUCT ||
           d->as.variable.type->pointee->kind == TYPE_ARRAY)))) {
        Expr *items[] = {e};
        ExprArray root = {items, 1};
        size_t index = 0;
        InitializerExprCache cache = {0};
        consume_initializer(a, d->as.variable.type, &root, &index, 0, true, &cache);
        return;
    }
    if (e->kind == EXPR_ARRAY_INIT) {
        Type *t = d->as.variable.type;
        Type *element = t && (t->kind == TYPE_POINTER || t->kind == TYPE_ARRAY)
                            ? t->pointee : NULL;
        if (t && t->kind == TYPE_ARRAY && t->array_length &&
            e->as.array_init.count > t->array_length)
            report(a, e->location, "too many fixed array initializers");
        for (size_t i = 0; i < e->as.array_init.count; i++) {
            Type *it = check_expr(a, e->as.array_init.items[i]);
            if (element && it)
                check_constant_range(a, element, e->as.array_init.items[i], "array initializer");
            if (element && it && !assignable_expr(element, it, e->as.array_init.items[i]))
                report(a, e->as.array_init.items[i]->location,
                       "array initializer element has incompatible type");
        }
        if (!element)
            report(a, e->location, "array initializer requires an array declaration");
        return;
    }
    Type *to = d->as.variable.type;
    if (to && to->kind == TYPE_ARRAY) {
        if (!string_element_matches(to->pointee, e))
            report(a, e->location, "fixed array initializer requires a list or char string");
        else if (to->array_length && string_array_count(e) - 1 > to->array_length)
            report(a, e->location, "string is too long for fixed char array");
        return;
    }
    Type *it = check_expr(a, e);
    if (it && to) {
        check_constant_range(a, to, e, "initializer");
        if (!assignable_expr(to, it, e))
            report(a, e->location, "initializer for '%s' has incompatible type",
                   d->as.variable.name);
    }
}
/**
 * @brief Validates the braced initializer of a scalar compound literal.
 * @param a Semantic analyzer.
 * @param type Scalar object type.
 * @param initializer Braced initializer.
 * @return No value; reports invalid initializers.
 */
static void check_scalar_compound_initializer(Analyzer *a, Type *type, Expr *initializer)
{
    Expr *items[] = {initializer};
    ExprArray root = {items, 1};
    size_t index = 0;
    InitializerExprCache cache = {0};
    consume_initializer(a, type, &root, &index, 0, true, &cache);
}

static bool file_constant_address(Analyzer *, const Expr *, bool, unsigned);

/**
 * @brief Recognizes static addresses usable in file-scope literal initializers.
 * @param a Analyzer holding visible file-scope declarations.
 * @param e Expression to inspect.
 * @param object Whether the expression denotes an object whose address is taken.
 * @param depth Current recursion depth.
 * @return True for a supported constant address.
 */
static bool file_constant_address(Analyzer *a, const Expr *e, bool object, unsigned depth)
{
    if (!e || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    switch (e->kind) {
    case EXPR_STRING:
        return true;
    case EXPR_NAME: {
        Symbol *s = lookup_scope(a->scope, e->as.name.name, e->as.name.length);
        return s && (s->kind == SEMANTIC_SYMBOL_FUNCTION ||
                     (s->kind == SEMANTIC_SYMBOL_VARIABLE &&
                      (object || (s->type && s->type->kind == TYPE_ARRAY))));
    }
    case EXPR_CAST:
        if (e->as.cast.operand && e->as.cast.operand->kind == EXPR_ARRAY_INIT)
            return e->as.cast.semantic_file_scope &&
                   (object || (e->as.cast.target_type &&
                               e->as.cast.target_type->kind == TYPE_ARRAY));
        if (!object && e->as.cast.target_type && e->as.cast.target_type->kind == TYPE_POINTER)
            return file_constant_address(a, e->as.cast.operand, false, depth + 1);
        return false;
    case EXPR_UNARY:
        if (!object && e->as.unary.op == UNARY_ADDRESS)
            return file_constant_address(a, e->as.unary.operand, true, depth + 1);
        if (object && e->as.unary.op == UNARY_DEREFERENCE)
            return file_constant_address(a, e->as.unary.operand, false, depth + 1);
        return false;
    case EXPR_INDEX: {
        long double index;
        return object && sema_numeric_constant(e->as.index.index, &index) &&
               file_constant_address(a, e->as.index.base, false, depth + 1);
    }
    case EXPR_MEMBER:
        return (object || e->semantic_type == TYPE_ARRAY) &&
               file_constant_address(a, e->as.member.base,
                                     !e->as.member.through_pointer, depth + 1);
    case EXPR_BINARY: {
        long double number;
        if (object || (e->as.binary.op != BINARY_ADD &&
                       e->as.binary.op != BINARY_SUBTRACT))
            return false;
        return (sema_numeric_constant(e->as.binary.right, &number) &&
                file_constant_address(a, e->as.binary.left, false, depth + 1)) ||
               (e->as.binary.op == BINARY_ADD &&
                sema_numeric_constant(e->as.binary.left, &number) &&
                file_constant_address(a, e->as.binary.right, false, depth + 1));
    }
    case EXPR_CONDITIONAL: {
        long double condition;
        return !object && sema_numeric_constant(e->as.conditional.condition, &condition) &&
               (file_constant_address(a, condition ? e->as.conditional.then_expr
                                                    : e->as.conditional.else_expr,
                                      false, depth + 1) ||
                sema_numeric_constant(condition ? e->as.conditional.then_expr
                                                : e->as.conditional.else_expr, &condition));
    }
    default:
        return false;
    }
}

/**
 * @brief Checks static initialization of a file-scope compound literal.
 * @param a Analyzer receiving diagnostics.
 * @param e Initializer or nested initializer item.
 * @param depth Current recursion depth.
 * @return No value; reports nonconstant initializer expressions.
 */
static void check_file_compound_constants(Analyzer *a, const Expr *e, unsigned depth)
{
    if (!e || depth >= SEMA_MAX_RECURSION_DEPTH)
        return;
    if (e->kind == EXPR_ARRAY_INIT) {
        for (size_t i = 0; i < e->as.array_init.count; ++i)
            check_file_compound_constants(a, e->as.array_init.items[i], depth + 1);
    } else if (e->kind == EXPR_DESIGNATED) {
        check_file_compound_constants(a, e->as.designated.value, depth + 1);
    } else {
        long double value;
        if (!sema_numeric_constant(e, &value) && e->kind != EXPR_STRING &&
            !file_constant_address(a, e, false, 0))
            report(a, e->location, "file-scope compound literal initializer must be constant");
    }
}
/**
 * @brief Checks the syntax permitted in an enum integer constant expression.
 * @param e Semantically checked expression.
 * @param depth Current recursion depth.
 * @return True for supported C integer constant-expression forms.
 */
static bool enum_constant_form(const Expr *e, unsigned depth)
{
    if (!e || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    switch (e->kind) {
    case EXPR_INTEGER:
        return !e->integer_literal_out_of_range;
    case EXPR_CHAR:
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        return true;
    case EXPR_OFFSETOF:
        return e->as.offset_of.constant;
    case EXPR_NAME:
        return e->semantic_enum_constant;
    case EXPR_UNARY:
        return (e->as.unary.op == UNARY_PLUS || e->as.unary.op == UNARY_MINUS ||
                e->as.unary.op == UNARY_LOGICAL_NOT || e->as.unary.op == UNARY_BITWISE_NOT) &&
               enum_constant_form(e->as.unary.operand, depth + 1);
    case EXPR_BINARY:
        return e->as.binary.op != BINARY_POWER && e->as.binary.op != BINARY_COMMA &&
               enum_constant_form(e->as.binary.left, depth + 1) &&
               enum_constant_form(e->as.binary.right, depth + 1);
    case EXPR_CONDITIONAL:
        return enum_constant_form(e->as.conditional.condition, depth + 1) &&
               enum_constant_form(e->as.conditional.then_expr, depth + 1) &&
               enum_constant_form(e->as.conditional.else_expr, depth + 1);
    case EXPR_CAST:
        return is_numeric(e->as.cast.target_type) &&
               !is_floating(e->as.cast.target_type->kind) && e->as.cast.operand &&
               (e->as.cast.operand->kind == EXPR_REAL ||
                enum_constant_form(e->as.cast.operand, depth + 1));
    default:
        return false;
    }
}

/**
 * @brief Checks evaluated signed arithmetic in an enum constant for overflow.
 * @param e Semantically checked expression.
 * @param depth Current recursion depth.
 * @return True when evaluated integer operations are representable.
 */
static bool enum_arithmetic_valid(const Expr *e, unsigned depth)
{
    if (!e || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    if (e->kind == EXPR_CAST)
        return enum_arithmetic_valid(e->as.cast.operand, depth + 1);
    if (e->kind == EXPR_CONDITIONAL) {
        bool truth;
        return enum_arithmetic_valid(e->as.conditional.condition, depth + 1) &&
               constant_truth(e->as.conditional.condition, &truth, depth + 1, false) &&
               enum_arithmetic_valid(truth ? e->as.conditional.then_expr
                                           : e->as.conditional.else_expr, depth + 1);
    }
    if (e->kind == EXPR_UNARY) {
        if (!enum_arithmetic_valid(e->as.unary.operand, depth + 1))
            return false;
        if (e->as.unary.op == UNARY_MINUS && !is_unsigned_kind(e->semantic_type) &&
            !is_floating(e->semantic_type)) {
            IntegerValue operand;
            if (integer_constant(e->as.unary.operand, &operand, 0)) {
                int64_t minimum = e->semantic_type == TYPE_LONG ||
                                  e->semantic_type == TYPE_LONG_LONG ? INT64_MIN : INT32_MIN;
                if (signed_bits(operand.bits) == minimum)
                    return false;
            }
        }
        return true;
    }
    if (e->kind != EXPR_BINARY)
        return true;
    const Expr *left = e->as.binary.left, *right = e->as.binary.right;
    BinaryOp op = e->as.binary.op;
    if (!enum_arithmetic_valid(left, depth + 1))
        return false;
    if (op == BINARY_LOGICAL_AND || op == BINARY_LOGICAL_OR) {
        bool truth;
        if (!constant_truth(left, &truth, depth + 1, false))
            return false;
        if ((op == BINARY_LOGICAL_AND && !truth) || (op == BINARY_LOGICAL_OR && truth))
            return true;
    }
    if (!enum_arithmetic_valid(right, depth + 1))
        return false;
    if (is_floating(left->semantic_type) || is_floating(right->semantic_type))
        return true;
    IntegerValue l, r;
    if (!integer_constant(left, &l, 0) || !integer_constant(right, &r, 0))
        return false;
    Type lt = {l.kind, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false}, rt = {r.kind, NULL, NULL, false, 0, NULL, false, NULL, false, NULL, false, false};
    Type *common = promote(&lt, &rt, true);
    if (!common)
        return false;
    if (op == BINARY_SHIFT_LEFT || op == BINARY_SHIFT_RIGHT) {
        TypeKind kind = is_promoted_small_kind(l.kind) || l.kind == TYPE_BYTE ? TYPE_INT : l.kind;
        if (is_unsigned_kind(kind) || op == BINARY_SHIFT_RIGHT)
            return true;
        int64_t x = signed_bits(integer_bits(l.bits, kind));
        unsigned width = kind == TYPE_LONG || kind == TYPE_LONG_LONG ? 64 : 32;
        int64_t maximum = width == 64 ? INT64_MAX : INT32_MAX;
        return x >= 0 && r.bits < width && x <= (maximum >> (unsigned)r.bits);
    }
    if (is_unsigned_kind(common->kind) || common->kind == TYPE_BYTE)
        return true;
    int64_t x = signed_bits(integer_bits(l.bits, common->kind));
    int64_t y = signed_bits(integer_bits(r.bits, common->kind));
    int64_t minimum = common->kind == TYPE_LONG || common->kind == TYPE_LONG_LONG
                          ? INT64_MIN : INT32_MIN;
    int64_t maximum = common->kind == TYPE_LONG || common->kind == TYPE_LONG_LONG
                          ? INT64_MAX : INT32_MAX;
    if (op == BINARY_ADD)
        return !((y > 0 && x > maximum - y) || (y < 0 && x < minimum - y));
    if (op == BINARY_SUBTRACT)
        return !((y < 0 && x > maximum + y) || (y > 0 && x < minimum + y));
    if (op == BINARY_MULTIPLY) {
        if (x > 0)
            return !((y > 0 && x > maximum / y) || (y < 0 && y < minimum / x));
        if (x < 0)
            return !((y > 0 && x < minimum / y) || (y < 0 && x < maximum / y));
    }
    return true;
}

/**
 * @brief Validates and resolves alignment specifiers on one declaration.
 * @param a Analyzer holding the visible declarations.
 * @param d Declaration whose alignment is resolved.
 * @param member Whether this is a structure or union member.
 * @return No value; records diagnostics and resolved alignment.
 */
static void check_alignment_specifiers(Analyzer *a, Decl *d, bool member)
{
    d->semantic_alignment = 0;
    if (!d->alignment_specifiers)
        return;
    if (d->kind != DECL_VARIABLE) {
        report(a, d->location, "alignment specifiers require an object declaration");
        return;
    }
    if (d->alignment_specifiers->register_storage)
        report(a, d->location, "alignment specifiers cannot apply to register objects");
    if (member && d->as.variable.is_bit_field)
        report(a, d->location, "alignment specifiers cannot apply to bit-fields");
    size_t requested = 0;
    for (size_t i = 0; i < d->alignment_specifiers->expressions.count; ++i) {
        Expr *expression = d->alignment_specifiers->expressions.items[i];
        Type *type = check_expr(a, expression);
        IntegerValue value;
        if (!type || !is_numeric(type) || is_floating(type->kind) ||
            !enum_constant_form(expression, 0) || !integer_constant(expression, &value, 0) ||
            !enum_arithmetic_valid(expression, 0) ||
            (!is_unsigned_kind(value.kind) && signed_bits(value.bits) < 0)) {
            report(a, expression->location, "alignment must be an integer constant expression");
            continue;
        }
        if (value.bits > 16 || (value.bits && (value.bits & (value.bits - 1)))) {
            report(a, expression->location, "unsupported alignment; expected 0, 1, 2, 4, 8, or 16");
            continue;
        }
        if (value.bits > requested)
            requested = (size_t)value.bits;
    }
    Type *object = d->as.variable.type;
    while (object && object->kind == TYPE_ARRAY)
        object = object->pointee;
    size_t size, natural;
    if (requested && object && type_layout_size(object, layout_find_struct, a,
                                                &size, &natural) && requested < natural)
        report(a, d->location, "alignment cannot be weaker than the object type's alignment");
    d->semantic_alignment = requested;
}

/**
 * @brief Evaluates and registers enumerators in their declaration scope.
 * @param a Analyzer holding the current scope.
 * @param d Enum declaration whose enumerators are processed in order.
 * @param global Whether enumerators belong to the file scope.
 * @return No value; records diagnostics for invalid or duplicate values.
 */
static void check_enum_decl(Analyzer *a, Decl *d, bool global)
{
    if (global && d->as.enumeration.tag &&
        lookup_struct(a, d->as.enumeration.tag, strlen(d->as.enumeration.tag)))
        report(a, d->location, "enum tag '%s' conflicts with a structure tag",
               d->as.enumeration.tag);
    int64_t next = 0;
    d->as.enumeration.is_unsigned = true;
    for (size_t i = 0; i < d->as.enumeration.count && !a->out_of_memory; ++i) {
        Enumerator *item = &d->as.enumeration.items[i];
        int64_t value = next;
        bool valid = true;
        bool diagnosed = false;
        if (item->value) {
            Type *type = check_expr(a, item->value);
            IntegerValue constant;
            if (!type || !is_numeric(type) || is_floating(type->kind) ||
                !enum_constant_form(item->value, 0) ||
                !integer_constant(item->value, &constant, 0) ||
                !enum_arithmetic_valid(item->value, 0)) {
                report(a, item->location, "enumerator '%s' requires an integer constant expression",
                       item->name);
                valid = false;
                diagnosed = true;
            } else if (is_unsigned_kind(constant.kind) || constant.kind == TYPE_BYTE) {
                if (constant.bits > INT32_MAX)
                    valid = false;
                else
                    value = (int64_t)constant.bits;
            } else
                value = signed_bits(constant.bits);
        }
        if (valid && (value < INT32_MIN || value > INT32_MAX))
            valid = false;
        if (!valid) {
            if (!diagnosed)
                report(a, item->location, "enumerator '%s' value is outside the int range",
                       item->name);
            value = 0;
        }
        next = value + 1;
        if (value < 0)
            d->as.enumeration.is_unsigned = false;
        Symbol *old = global ? lookup_global(a, item->name, item->name_length)
                             : find_local(a->scope->symbols, item->name, item->name_length);
        if (old) {
            report(a, item->location, "duplicate declaration of '%s'", item->name);
            continue;
        }
        if (d->as.enumeration.is_unsigned && !a->program->contains_unsigned) {
            a->program->contains_unsigned = true;
            a->program->unsigned_location = d->location;
        }
        Symbol *symbol = new_symbol(a, item->name, item->name_length,
                                    SEMANTIC_SYMBOL_ENUMERATOR, item->location, d, &t_int, true);
        if (!symbol)
            return;
        symbol->enum_value = (int32_t)value;
        if (global) {
            symbol->next = a->model->globals;
            a->model->globals = symbol;
            ++a->model->global_count;
        } else {
            symbol->next = a->scope->symbols;
            a->scope->symbols = symbol;
        }
    }
}

/**
 * @brief Evaluates a static assertion in its declaration scope.
 * @param a Semantic analyzer.
 * @param d Assertion declaration.
 * @return No value; reports invalid or false assertions.
 */
static void check_static_assertion(Analyzer *a, Decl *d)
{
    Expr *condition = d->as.static_assertion.condition;
    Expr *message = d->as.static_assertion.message;
    Type *type = check_expr(a, condition);
    IntegerValue value;
    if (!type || !is_numeric(type) || is_floating(type->kind) ||
        !enum_constant_form(condition, 0) || !integer_constant(condition, &value, 0) ||
        !enum_arithmetic_valid(condition, 0)) {
        report(a, d->location, "static assertion requires an integer constant expression");
    } else if (!value.bits) {
        char text[480];
        size_t offset = 1, length = 0;
        int c;
        if (message && message->kind == EXPR_STRING && message->as.string.bytes)
            while (length + 1 < sizeof(text) && (c = format_byte(message, &offset)) >= 0)
                text[length++] = (char)c;
        text[length] = '\0';
        if (message)
            report(a, d->location, "static assertion failed: %s", text);
        else
            report(a, d->location, "static assertion failed");
    }
}

/**
 * @brief Checks whether two linked object or function declarations agree.
 * @param previous Earlier declaration.
 * @param d Declaration being checked.
 * @return True when the types and thread-local storage agree.
 */
static bool linked_declarations_compatible(const Decl *previous, const Decl *d)
{
    if (previous->kind != d->kind)
        return false;
    if (d->kind == DECL_FUNCTION)
        return function_compatible(previous, d);
    if (previous->is_thread_local != d->is_thread_local)
        return false;
    Type *old = previous->as.variable.type;
    Type *type = d->as.variable.type;
    return qualified_equal(old, type) ||
           (old && type && old->kind == TYPE_ARRAY && type->kind == TYPE_ARRAY &&
            (!old->array_length || !type->array_length) &&
            old->is_const == type->is_const && old->is_volatile == type->is_volatile &&
            qualified_equal(old->pointee, type->pointee));
}

/**
 * @brief Validates and registers a local declaration.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param d Declaration node being analyzed or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void check_local_decl(Analyzer *a, Decl *d)
{
    if (!d)
        return;
    if (a->current_function && a->current_function->as.function.semantic_inline_only &&
        d->kind == DECL_VARIABLE && d->is_static) {
        const Type *type = d->as.variable.type;
        while (type && type->kind == TYPE_ARRAY)
            type = type->pointee;
        if (!type || !type->is_const)
            report(a, d->location, "inline definition cannot define a modifiable static object");
    }
    if (d->kind == DECL_TYPE_ALIAS || d->kind == DECL_VARIABLE || d->kind == DECL_FUNCTION)
        a->source_offset = d->declarator_offset ? d->declarator_offset : d->location.offset;
    check_alignment_specifiers(a, d, false);
    if (d->kind == DECL_STATIC_ASSERT) {
        check_static_assertion(a, d);
        return;
    }
    if (d->kind == DECL_STRUCT_SCOPE) {
        Symbol *structure = find_local(a->model->structures, d->as.structure.name,
                                       d->as.structure.name_length);
        if (structure)
            validate_struct_fields(a, structure->declaration);
        else
            report(a, d->location, "unknown local structure");
        return;
    }
    if (d->kind == DECL_ENUM) {
        check_enum_decl(a, d, false);
        return;
    }
    if (d->kind == DECL_TYPE_ALIAS) {
        validate_local_structure(a, d->as.type_alias.type, 0);
        bool saved = a->suppress_unsigned_types;
        bool requires_long_double_ir = a->program->requires_long_double_ir;
        a->suppress_unsigned_types = true;
        if (!d->as.type_alias.type || d->as.type_alias.type->kind != TYPE_STRUCT)
            validate_type(a, d->as.type_alias.type, d->location);
        a->suppress_unsigned_types = saved;
        a->program->requires_long_double_ir = requires_long_double_ir;
        Symbol *old = find_local(a->scope->symbols, d->as.type_alias.name,
                                 d->as.type_alias.name_length);
        if (old && old->kind == SEMANTIC_SYMBOL_TYPE_ALIAS) {
            if (!identical_type(old->type, d->as.type_alias.type))
                report(a, d->location, "conflicting declaration of type alias '%s'",
                       d->as.type_alias.name);
        } else
            add_scope_symbol(a, a->scope, d->as.type_alias.name, d->as.type_alias.name_length,
                             SEMANTIC_SYMBOL_TYPE_ALIAS, d->location, d, d->as.type_alias.type);
        return;
    }
    if (d->kind == DECL_FUNCTION || (d->kind == DECL_VARIABLE && d->is_extern)) {
        bool function = d->kind == DECL_FUNCTION;
        const char *name = function ? d->as.function.name : d->as.variable.name;
        size_t length = function ? d->as.function.name_length : d->as.variable.name_length;
        Type *type = function ? d->as.function.return_type : d->as.variable.type;
        if (function && (d->is_static || d->as.function.body))
            report(a, d->location, "block-scope function must be a non-static declaration");
        if (function)
            d->is_extern = true;
        if (!function && d->as.variable.initializer.kind != INIT_NONE)
            report(a, d->location, "external variable cannot have an initializer");
        if ((!function || !incomplete_structure(a, type)) &&
            (function || !incomplete_external_structure(a, d)))
            validate_type(a, type, d->location);
        if (!function) {
            validate_local_structure(a, type, 0);
            resolve_fixed_array(a, d, true, false);
            if (!type || type->kind == TYPE_VOID || type->kind == TYPE_FUNCTION)
                report(a, d->location, "external variables must have an object type");
        } else {
            if (type && (type->kind == TYPE_ARRAY || type->kind == TYPE_FUNCTION))
                report(a, d->location, "functions cannot return arrays or functions");
            Scope prototype = {0};
            prototype.parent = a->scope;
            a->scope = &prototype;
            for (size_t i = 0; i < d->as.function.parameters.count; ++i) {
                Parameter *param = &d->as.function.parameters.items[i];
                check_embedded_enum(a, param->enum_definition);
                validate_parameter_type(a, param, true);
                if (param->type && param->type->kind == TYPE_VOID)
                    report(a, param->location, "parameter cannot have void type");
                if (param->name &&
                    add_scope_symbol(a, &prototype, param->name, param->name_length,
                                     SEMANTIC_SYMBOL_VARIABLE, param->location, NULL, param->type))
                    prototype.symbols->parameter = param;
            }
            release_symbols(a, prototype.symbols);
            a->scope = prototype.parent;
        }
        Symbol *same = find_local(a->scope->symbols, name, length);
        if (same && (!same->declaration ||
                     (same->kind != (function ? SEMANTIC_SYMBOL_FUNCTION :
                                               SEMANTIC_SYMBOL_VARIABLE)) ||
                     (!function && !same->declaration->is_extern))) {
            report(a, d->location, "duplicate declaration of '%s' in the same scope", name);
            return;
        }
        Symbol *global = lookup_global(a, name, length);
        Symbol *visible = lookup_scope(a->scope, name, length);
        d->semantic_internal_linkage =
            visible ? (visible->declaration && visible->declaration->is_extern &&
                       visible->declaration->semantic_internal_linkage) :
                      (global && global->internal);
        if (global && (global->kind == SEMANTIC_SYMBOL_VARIABLE ||
                       global->kind == SEMANTIC_SYMBOL_FUNCTION) && global->declaration &&
            !linked_declarations_compatible(global->declaration, d))
            report(a, d->location, "conflicting declaration of '%s'", name);
        for (size_t i = 0; i < a->program->block_externals.count; ++i) {
            Decl *candidate = a->program->block_externals.items[i];
            size_t candidate_length;
            const char *candidate_name = decl_name(candidate, &candidate_length);
            if (same_name(name, length, candidate_name, candidate_length) &&
                !linked_declarations_compatible(candidate, d)) {
                report(a, d->location, "conflicting declaration of '%s'", name);
                break;
            }
        }
        Symbol *linked = visible ? (visible->declaration && visible->declaration->is_extern
                                        ? visible : NULL) : global;
        if (!function && type && type->kind == TYPE_ARRAY && linked &&
            linked->kind == SEMANTIC_SYMBOL_VARIABLE && linked->type &&
            linked->type->kind == TYPE_ARRAY) {
            if (!type->array_length)
                type->array_length = linked->type->array_length;
            else if (!linked->type->array_length && same)
                same->type = type;
        }
        if (!same && !add_scope_symbol(a, a->scope, name, length,
                                       function ? SEMANTIC_SYMBOL_FUNCTION :
                                                  SEMANTIC_SYMBOL_VARIABLE,
                                       d->location, d, type))
            return;
        DeclArray *declarations = &a->program->block_externals;
        if (declarations->count == SIZE_MAX ||
            declarations->count + 1 > SIZE_MAX / sizeof(*declarations->items)) {
            a->out_of_memory = true;
            return;
        }
        Decl **items = realloc(declarations->items,
                               (declarations->count + 1) * sizeof(*items));
        if (!items) {
            a->out_of_memory = true;
            return;
        }
        declarations->items = items;
        items[declarations->count++] = d;
        return;
    }
    if (d->kind != DECL_VARIABLE)
        return;
    validate_local_structure(a, d->as.variable.type, 0);
    Type *type = d->as.variable.type;
    bool saved_allow_variable = a->allow_variable_array_bounds;
    bool pointer_to_array = type && type->kind == TYPE_POINTER && type->pointee &&
                            type->pointee->kind == TYPE_ARRAY;
    a->allow_variable_array_bounds = type && (type->kind == TYPE_ARRAY || pointer_to_array);
    validate_type(a, pointer_to_array ? type->pointee : type, d->location);
    a->allow_variable_array_bounds = saved_allow_variable;
    d->as.variable.semantic_vla = false;
    d->as.variable.semantic_vla_pointer =
        pointer_to_array && variable_array_type(type->pointee);
    if (d->as.variable.semantic_vla_pointer) {
        size_t size, alignment;
        const Type *element = type->pointee;
        while (element && element->kind == TYPE_ARRAY)
            element = element->pointee;
        if (!type_layout_size(element, layout_find_struct, a, &size, &alignment))
            report(a, d->location, "pointer to variable-length array requires a complete element type");
        d->as.variable.semantic_vla_id = a->vla_count++;
        if (d->as.variable.semantic_vla_id >= 512)
            report(a, d->location, "too many variable-length arrays in one function");
        if (d->is_static || d->is_extern)
            report(a, d->location, "pointer to variable-length array must have automatic storage");
        a->program->contains_vla = true;
        a->current_function->as.function.semantic_has_vla = true;
    }
    bool inner_variable = variable_array_type(type);
    if (type && type->kind == TYPE_ARRAY && (d->as.variable.array_size || inner_variable)) {
        Expr *bound = d->as.variable.array_size;
        Type *count_type = bound ? check_expr(a, bound) : NULL;
        IntegerValue value;
        bool constant = bound && integer_constant(bound, &value, 0);
        bool outer_variable = count_type && is_numeric(count_type) &&
            !is_floating(count_type->kind) && !constant && !constant_expression_form(bound, 0);
        if (outer_variable || inner_variable) {
            size_t size, alignment;
            d->as.variable.semantic_vla = true;
            d->as.variable.semantic_vla_id = a->vla_count++;
            if (d->as.variable.semantic_vla_id >= 512)
                report(a, d->location, "too many variable-length arrays in one function");
            a->program->contains_vla = true;
            a->current_function->as.function.semantic_has_vla = true;
            if (d->is_static || d->is_extern)
                report(a, d->location, "variable-length array must have automatic storage");
            if (d->as.variable.initializer.kind != INIT_NONE)
                report(a, d->location, "variable-length array cannot have an initializer");
            if (inner_variable && !outer_variable) {
                if (!bound || !count_type || !is_numeric(count_type) ||
                    is_floating(count_type->kind) || !constant || !value.bits ||
                    (!is_unsigned_kind(value.kind) && signed_bits(value.bits) <= 0) ||
                    value.bits > SIZE_MAX)
                    report(a, bound ? bound->location : d->location,
                           "array bound must be a positive integer");
                else
                    type->array_length = (size_t)value.bits;
            }
            const Type *element = type;
            while (element && element->kind == TYPE_ARRAY)
                element = element->pointee;
            if (!type_layout_size(element, layout_find_struct, a, &size, &alignment))
                report(a, d->location, "variable-length array requires a complete element type");
        }
    }
    bool infer_structure_array = type && type->kind == TYPE_ARRAY && type->pointee &&
                                 !type->array_length &&
                                 !d->as.variable.array_size &&
                                 d->as.variable.initializer.expression &&
                                 d->as.variable.initializer.expression->kind == EXPR_ARRAY_INIT &&
                                 (type->pointee->kind == TYPE_STRUCT ||
                                  type->pointee->kind == TYPE_ARRAY ||
                                  has_designator(d->as.variable.initializer.expression, 0));
    if (!infer_structure_array && !d->as.variable.semantic_vla)
        resolve_fixed_array(a, d, false, false);
    if (!add_scope_symbol(a, a->scope, d->as.variable.name, d->as.variable.name_length,
                          SEMANTIC_SYMBOL_VARIABLE, d->location, d, d->as.variable.type))
        return;
    if (infer_structure_array && !d->as.variable.semantic_vla)
        resolve_fixed_array(a, d, false, false);
    a->scope->symbols->warn_unused = true;
    if (!d->as.variable.semantic_vla)
        check_initializer(a, d);
}
/**
 * @brief Type-checks a statement and its children.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param s Statement node being analyzed or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void check_stmt_impl(Analyzer *a, Stmt *s)
{
    if (!s)
        return;
    switch (s->kind) {
    case STMT_BLOCK:
        if (!push_scope(a))
            return;
        check_stmt_list(a, s->as.block.statements.items, s->as.block.statements.count);
        pop_scope(a);
        break;
    case STMT_DECLARATION:
        check_local_decl(a, s->as.declaration.declaration);
        break;
    case STMT_EXPRESSION:
        check_expr(a, s->as.expression.expression);
        break;
    case STMT_IF: {
        do {
            Type *t = check_expr(a, s->as.if_stmt.condition);
            if (t && !is_numeric(t) && t->kind != TYPE_POINTER)
                report(a, s->location, "if condition must be scalar");
            check_stmt(a, s->as.if_stmt.then_branch);
            s = s->as.if_stmt.else_branch;
        } while (s && s->kind == STMT_IF && !a->out_of_memory);
        check_stmt(a, s);
        break;
    }
    case STMT_SWITCH: {
        Type *control = check_expr(a, s->as.switch_stmt.expression);
        if (control && control->kind == TYPE_ENUM)
            control = type_effective_kind(control) == TYPE_UNSIGNED_INT ? &t_unsigned_int : &t_int;
        if (control && (is_promoted_small_kind(control->kind) ||
                        (a->program->c11_numeric && control->kind == TYPE_BYTE)))
            control = &t_int;
        if (control && (!is_numeric(control) || is_floating(control->kind)))
            report(a, s->location, "switch expression must have integer type");
        if (!push_scope(a))
            return;
        a->break_depth++;
        check_stmt_list(a, s->as.switch_stmt.prefix.items, s->as.switch_stmt.prefix.count);
        int have_default = 0;
        for (size_t i = 0; i < s->as.switch_stmt.cases.count; i++) {
            SwitchCase *c = &s->as.switch_stmt.cases.items[i];
            for (Symbol *symbol = a->scope->symbols; symbol; symbol = symbol->next)
                if (symbol->declaration && symbol->declaration->kind == DECL_VARIABLE &&
                    (symbol->declaration->as.variable.semantic_vla ||
                     symbol->declaration->as.variable.semantic_vla_pointer)) {
                    report(a, c->location,
                           "switch cannot jump into the scope of a variable-length array");
                    break;
                }
            if (!c->value) {
                if (have_default)
                    report(a, c->location, "duplicate default label");
                have_default = 1;
            } else {
                Type *case_type = check_expr(a, c->value);
                long double value;
                IntegerValue case_integer;
                bool converted_case = control && case_type &&
                    (is_unsigned_kind(control->kind) || is_unsigned_kind(case_type->kind)) &&
                    integer_constant(c->value, &case_integer, 0);
                bool constant = converted_case || constant_number(c->value, &value);
                if (converted_case)
                    value = is_unsigned_kind(case_integer.kind) || case_integer.kind == TYPE_BYTE
                                ? (long double)case_integer.bits
                                : (long double)signed_bits(case_integer.bits);
                if (case_type && (!is_numeric(case_type) || is_floating(case_type->kind)))
                    report(a, c->location, "case label must have integer type");
                if (!constant || !isfinite(value) || truncl(value) != value)
                    report(a, c->location, "case label is not an integer constant expression");
                else if (!converted_case && control && is_numeric(control) && !is_floating(control->kind) &&
                         !numeric_value_fits(control, value))
                    report(a, c->location, "case value is out of range for switch expression");
                else {
                    for (size_t j = 0; j < i; j++) {
                        long double prior;
                        Expr *previous = s->as.switch_stmt.cases.items[j].value;
                        IntegerValue previous_integer;
                        bool same = false;
                        if (previous && control && case_type &&
                            (converted_case || is_unsigned_kind(previous->semantic_type)) &&
                            integer_constant(c->value, &case_integer, 0) &&
                            integer_constant(previous, &previous_integer, 0))
                            same = integer_bits(case_integer.bits, control->kind) ==
                                   integer_bits(previous_integer.bits, control->kind);
                        else if (previous && constant_number(previous, &prior))
                            same = prior == value;
                        if (same) {
                            report(a, c->location, "duplicate case label");
                            break;
                        }
                    }
                }
            }
            check_stmt_list(a, c->statements.items, c->statements.count);
            if (a->warn_case_fall_through && c->statements.count &&
                i + 1 < s->as.switch_stmt.cases.count) {
                Stmt *last = c->statements.items[c->statements.count - 1];
                while (last && last->kind == STMT_LABEL)
                    last = last->as.label.statement;
                if (last && last->kind != STMT_BREAK && last->kind != STMT_RETURN &&
                    last->kind != STMT_CONTINUE && last->kind != STMT_GOTO)
                    warn(a, s->as.switch_stmt.cases.items[i + 1].location,
                         "case falls through from the previous label");
            }
        }
        a->break_depth--;
        pop_scope(a);
        break;
    }
    case STMT_FOR:
        if (!push_scope(a))
            return;
        if (s->as.for_stmt.initializer) {
            Stmt *init = s->as.for_stmt.initializer;
            if (init->kind == STMT_BLOCK) {
                for (size_t i = 0; i < init->as.block.statements.count; i++)
                    check_stmt(a, init->as.block.statements.items[i]);
            } else
                check_stmt(a, init);
        }
        {
            Type *t = check_expr(a, s->as.for_stmt.condition);
            if (t && !is_numeric(t) && t->kind != TYPE_POINTER)
                report(a, s->location, "for condition must be scalar");
        }
        check_expr(a, s->as.for_stmt.update);
        a->break_depth++;
        a->loop_depth++;
        check_stmt(a, s->as.for_stmt.body);
        a->loop_depth--;
        a->break_depth--;
        pop_scope(a);
        break;
    case STMT_LOOP:
    case STMT_DO: {
        Type *t = check_expr(a, s->as.loop_stmt.condition);
        if (t && !is_numeric(t) && t->kind != TYPE_POINTER)
            report(a, s->location, "loop condition must be scalar");
        a->break_depth++;
        a->loop_depth++;
        check_stmt(a, s->as.loop_stmt.body);
        a->loop_depth--;
        a->break_depth--;
        break;
    }
    case STMT_CONTINUE:
        if (!a->loop_depth)
            report(a, s->location, "continue used outside a loop");
        break;
    case STMT_BREAK:
        if (!a->break_depth)
            report(a, s->location, "break used outside a loop or switch");
        break;
    case STMT_GOTO:
    case STMT_LABEL: {
        FunctionLabel *label = a->labels;
        while (label && strcmp(label->name, s->as.label.name))
            label = label->next;
        if (!label) {
            label = analyzer_alloc(a, sizeof(*label));
            if (!label)
                return;
            label->name = s->as.label.name;
            label->id = a->label_count++;
            label->next = a->labels;
            a->labels = label;
        }
        s->as.label.id = label->id;
        if (s->kind == STMT_LABEL) {
            if (label->defined)
                report(a, s->location, "duplicate label '%s'", label->name);
            label->defined = true;
            active_vlas(a, label->active_vlas);
            check_stmt(a, s->as.label.statement);
        } else {
            if (!label->referenced)
                label->reference_location = s->location;
            label->referenced = true;
            VlaJump *jump = analyzer_alloc(a, sizeof(*jump));
            if (jump) {
                active_vlas(a, jump->active);
                jump->location = s->location;
                jump->next = label->jumps;
                label->jumps = jump;
            }
        }
        break;
    }
    case STMT_RETURN: {
        Expr *e = s->as.return_stmt.expression;
        Type *t = check_expr(a, e);
        if (a->return_type) {
            if (a->return_type->kind == TYPE_VOID && e)
                report(a, s->location, "void function must not return a value");
            else if (a->return_type->kind != TYPE_VOID && !e)
                report(a, s->location, "non-void function must return a value");
            else if (e && t) {
                check_constant_range(a, a->return_type, e, "return value");
                if (!assignable_expr(a->return_type, t, e))
                    report(a, s->location, "return expression has incompatible type");
            }
        }
        break;
    }
    }
}

/**
 * @brief Type-checks a statement while bounding recursive AST traversal.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param s Statement node being analyzed or emitted.
 * @return No value; reports a diagnostic if statement nesting exceeds the limit.
 */
static void check_stmt(Analyzer *a, Stmt *s)
{
    if (!s || a->out_of_memory)
        return;
    if (a->statement_depth >= SEMA_MAX_RECURSION_DEPTH) {
        if (!a->recursion_limit_reported) {
            report(a, s->location, "maximum semantic statement nesting depth exceeded");
            a->recursion_limit_reported = true;
        }
        return;
    }
    size_t saved_offset = a->source_offset;
    a->source_offset = s->location.offset;
    ++a->statement_depth;
    check_stmt_impl(a, s);
    --a->statement_depth;
    a->source_offset = saved_offset;
}
/**
 * @brief Counts matching fields including promoted anonymous members.
 * @param a Analyzer used to resolve nested structures.
 * @param structure Structure to search.
 * @param name Field name.
 * @param length Field name length.
 * @param depth Current nesting depth.
 * @return Number of visible matches.
 */
static size_t count_visible_fields(Analyzer *a, Decl *structure, const char *name, size_t length,
                                   unsigned depth)
{
    if (!structure || depth >= SEMA_MAX_RECURSION_DEPTH)
        return 0;
    size_t count = 0;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        Decl *field = structure->as.structure.fields[i];
        if (field->as.variable.anonymous_member) {
            Type *type = field->as.variable.type;
            Symbol *nested = type && type->struct_name
                                 ? lookup_struct(a, type->struct_name, strlen(type->struct_name))
                                 : NULL;
            if (nested)
                count += count_visible_fields(a, nested->declaration, name, length, depth + 1);
        } else if (field->as.variable.name &&
                   same_name(field->as.variable.name, field->as.variable.name_length,
                             name, length)) {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Checks for duplicate names promoted into a structure.
 * @param a Analyzer used to resolve nested structures.
 * @param root Structure whose visible names are checked.
 * @param current Structure containing fields being visited.
 * @param depth Current nesting depth.
 * @return No value; reports duplicate visible field names.
 */
static void check_visible_field_duplicates(Analyzer *a, Decl *root, Decl *current, unsigned depth)
{
    if (!current || depth >= SEMA_MAX_RECURSION_DEPTH)
        return;
    for (size_t i = 0; i < current->as.structure.field_count; ++i) {
        Decl *field = current->as.structure.fields[i];
        if (field->as.variable.anonymous_member) {
            Type *type = field->as.variable.type;
            Symbol *nested = type && type->struct_name
                                 ? lookup_struct(a, type->struct_name, strlen(type->struct_name))
                                 : NULL;
            if (nested)
                check_visible_field_duplicates(a, root, nested->declaration, depth + 1);
        } else if (field->as.variable.name &&
                   count_visible_fields(a, root, field->as.variable.name,
                                        field->as.variable.name_length, 0) > 1) {
            report(a, field->location, "duplicate field '%s' in structure",
                   field->as.variable.name);
        }
    }
}

/**
 * @brief Checks whether a structure exposes a named member.
 * @param a Analyzer holding structure declarations.
 * @param structure Structure to inspect.
 * @param depth Current anonymous-member nesting depth.
 * @return True when a named member is present.
 */
static bool has_named_field(Analyzer *a, const Decl *structure, unsigned depth)
{
    if (!structure || depth >= SEMA_MAX_RECURSION_DEPTH)
        return false;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        Decl *field = structure->as.structure.fields[i];
        if (field->as.variable.name)
            return true;
        if (!field->as.variable.anonymous_member)
            continue;
        const Type *type = field->as.variable.type;
        Symbol *nested = type && type->struct_name
                             ? lookup_struct(a, type->struct_name, strlen(type->struct_name))
                             : NULL;
        if (nested && has_named_field(a, nested->declaration, depth + 1))
            return true;
    }
    return false;
}

/**
 * @brief Validates field declarations for a structure.
 * @param a Semantic analyzer state containing the symbol tables, scopes, and diagnostics.
 * @param d Structure declaration being checked.
 * @return No value; reports invalid fields.
 */
static void validate_struct_fields(Analyzer *a, Decl *d)
{
    Symbol *symbol = find_local(a->model->structures, d->as.structure.name,
                                d->as.structure.name_length);
    if (symbol && symbol->defined)
        return;
    if (symbol)
        symbol->defined = true;
    size_t saved_offset = a->source_offset;
    Decl *previous_structure = a->current_structure;
    a->current_structure = d;
    size_t assertion = 0;
    bool named = false;
    for (size_t i = 0; i < d->as.structure.field_count; i++) {
        while (assertion < d->as.structure.assertion_count &&
               d->as.structure.assertions[assertion]->as.static_assertion.preceding_fields == i)
            check_static_assertion(a, d->as.structure.assertions[assertion++]);
        Decl *f = d->as.structure.fields[i];
        a->source_offset = f->location.offset;
        if (f->as.variable.is_bit_field) {
            f->as.variable.bit_field_width_resolved = false;
            Type *type = f->as.variable.type;
            unsigned maximum = type && type->kind == TYPE_BOOL ? 1 : 32;
            if (!type || (type->kind != TYPE_BOOL && type->kind != TYPE_INT &&
                          type->kind != TYPE_UNSIGNED_INT))
                report(a, f->location,
                       "bit-field type must be _Bool, int, or unsigned int");
            Expr *width = f->as.variable.bit_field_width;
            Type *width_type = check_expr(a, width);
            IntegerValue value;
            if (!width_type || !is_numeric(width_type) || is_floating(width_type->kind) ||
                !enum_constant_form(width, 0) || !integer_constant(width, &value, 0) ||
                !enum_arithmetic_valid(width, 0)) {
                report(a, f->location, "bit-field width must be an integer constant expression");
            } else if ((!is_unsigned_kind(value.kind) && signed_bits(value.bits) < 0) ||
                       value.bits > maximum || (!value.bits && f->as.variable.name)) {
                report(a, f->location, "bit-field width is outside the permitted range");
            } else if (type && (type->kind == TYPE_BOOL || type->kind == TYPE_INT ||
                                type->kind == TYPE_UNSIGNED_INT)) {
                f->as.variable.resolved_bit_field_width = (unsigned)value.bits;
                f->as.variable.bit_field_width_resolved = true;
            }
        }
        validate_local_structure(a, f->as.variable.type, 0);
        validate_type(a, f->as.variable.type, f->location);
        if (field_is_flexible_array(f)) {
            a->program->contains_flexible_array = true;
            size_t size, alignment;
            if (d->as.structure.is_union || i + 1 != d->as.structure.field_count || !named)
                report(a, f->location,
                       "flexible array must be the last member of a structure with another named member");
            if (!type_layout_size(f->as.variable.type->pointee, layout_find_struct, a,
                                  &size, &alignment))
                report(a, f->location, "flexible array requires a complete element type");
        } else {
            if (!d->as.structure.is_union && f->as.variable.type &&
                f->as.variable.type->kind == TYPE_STRUCT &&
                contains_flexible_array(a, f->as.variable.type, 0))
                report(a, f->location, "structure member cannot contain a flexible array member");
            resolve_fixed_array(a, f, false, true);
            if (f->as.variable.type && f->as.variable.type->kind == TYPE_ARRAY &&
                f->as.variable.type->is_zero_length_array) {
                a->program->contains_flexible_array = true;
                if (d->as.structure.is_union || i + 1 != d->as.structure.field_count || !named)
                    report(a, f->location,
                           "zero-length array must be the last member of a structure with another named member");
            }
        }
        check_alignment_specifiers(a, f, true);
        if (f->as.variable.name) {
            named = true;
        } else if (f->as.variable.anonymous_member) {
            const Type *type = f->as.variable.type;
            Symbol *nested = type && type->struct_name
                                 ? lookup_struct(a, type->struct_name, strlen(type->struct_name))
                                 : NULL;
            if (nested && has_named_field(a, nested->declaration, 0))
                named = true;
        }
    }
    while (assertion < d->as.structure.assertion_count)
        check_static_assertion(a, d->as.structure.assertions[assertion++]);
    a->current_structure = previous_structure;
    a->source_offset = d->as.structure.completion_offset;
    check_visible_field_duplicates(a, d, d, 0);
    a->source_offset = saved_offset;
}

/**
 * @brief Checks a file-scope initializer at its declaration.
 * @param a Analyzer holding declarations visible at this point.
 * @param d Variable declaration to check.
 * @return No value; records diagnostics and initializer metadata.
 */
static void check_global_initializer(Analyzer *a, Decl *d)
{
    if (d->as.variable.type &&
        (d->as.variable.type->kind == TYPE_ARRAY ||
         (d->as.variable.type->kind == TYPE_STRUCT &&
          d->as.variable.initializer.expression &&
          d->as.variable.initializer.expression->kind == EXPR_ARRAY_INIT))) {
        check_initializer(a, d);
    } else if (d->as.variable.initializer.expression) {
        if (d->as.variable.initializer.expression->kind == EXPR_ARRAY_INIT) {
            Expr *e = d->as.variable.initializer.expression;
            Type *t = d->as.variable.type;
            Type *element = t && t->kind == TYPE_POINTER ? t->pointee : NULL;
            for (size_t j = 0; j < e->as.array_init.count; j++) {
                Type *it = check_expr(a, e->as.array_init.items[j]);
                if (element && it) {
                    check_constant_range(a, element, e->as.array_init.items[j],
                                         "array initializer");
                    if (!assignable_expr(element, it, e->as.array_init.items[j]))
                        report(a, e->as.array_init.items[j]->location,
                               "global array initializer element has incompatible type");
                }
            }
        } else {
            Type *it = check_expr(a, d->as.variable.initializer.expression);
            if (it) {
                check_constant_range(a, d->as.variable.type,
                                     d->as.variable.initializer.expression,
                                     "global initializer");
                Type *to = d->as.variable.type;
                if (!assignable_expr(to, it, d->as.variable.initializer.expression))
                    report(a, d->location, "global initializer has incompatible type");
            }
        }
    }
}

/**
 * @brief Checks a function body with its file-scope declarations visible.
 * @param a Semantic analyzer.
 * @param d Function definition to check.
 * @return No value; records diagnostics and function metadata.
 */
static void check_function_body(Analyzer *a, Decl *d)
{
    Scope fs = {0};
    Scope *saved_scope = a->scope;
    a->scope = &fs;
    a->at_file_scope = false;
    a->return_type = d->as.function.return_type;
    a->current_function = d;
    a->last_named_parameter = NULL;
    a->break_depth = 0;
    a->loop_depth = 0;
    a->labels = NULL;
    a->label_count = 0;
    a->vla_count = 0;
    d->as.function.semantic_has_vla = false;
    d->as.function.semantic_has_alloca = false;
    OwnedType *function_name_type = analyzer_alloc(a, sizeof(*function_name_type));
    if (!function_name_type)
        goto done;
    function_name_type->value.kind = TYPE_ARRAY;
    function_name_type->value.pointee = &t_const_byte;
    function_name_type->value.array_length = d->as.function.name_length + 1;
    function_name_type->next = a->model->owned_types;
    a->model->owned_types = function_name_type;
    if (!add_scope_symbol(a, a->scope, "__func__", 8, SEMANTIC_SYMBOL_VARIABLE,
                          d->location, NULL, &function_name_type->value))
        goto done;
    fs.symbols->function_name = true;
    for (size_t j = 0; j < d->as.function.parameters.count; j++) {
        Parameter *p = &d->as.function.parameters.items[j];
        check_embedded_enum(a, p->enum_definition);
        if (!add_scope_symbol(a, a->scope, p->name, p->name_length, SEMANTIC_SYMBOL_VARIABLE,
                              p->location, NULL, p->type))
            goto done;
        fs.symbols->parameter = p;
        if (p->type && p->type->kind == TYPE_POINTER &&
            ((p->type->array_bound && p->type->array_bound->variable) ||
             variable_array_type(p->type->pointee)))
            a->program->contains_vla = true;
        if (j + 1 == d->as.function.parameters.count)
            a->last_named_parameter = fs.symbols;
    }
    Stmt *body = d->as.function.body;
    if (body->kind == STMT_BLOCK)
        check_stmt_list(a, body->as.block.statements.items, body->as.block.statements.count);
    else
        check_stmt(a, body);
    d->as.function.label_count = a->label_count;
done:
    while (a->labels) {
        FunctionLabel *label = a->labels;
        a->labels = label->next;
        if (label->referenced && !label->defined)
            report(a, label->reference_location, "undefined label '%s'", label->name);
        while (label->jumps) {
            VlaJump *jump = label->jumps;
            label->jumps = jump->next;
            if (label->defined)
                for (size_t i = 0; i < 8; ++i)
                    if (label->active_vlas[i] & ~jump->active[i]) {
                        report(a, jump->location,
                               "goto cannot enter the scope of a variable-length array");
                        break;
                    }
            free(jump);
        }
        free(label);
    }
    release_symbols(a, fs.symbols);
    a->scope = saved_scope;
    a->at_file_scope = true;
    a->return_type = NULL;
    a->current_function = NULL;
    a->last_named_parameter = NULL;
}

/**
 * @brief Analyzes an AST with configurable optional warnings.
 * @param program AST program being analyzed, emitted, or destroyed.
 * @param warn_unused_variable Whether to report unused local variables.
 * @param warn_case_fall_through Whether to report switch case fall-through.
 * @param output Destination stream or output object.
 * @return COMPILER_OK on success, COMPILER_SEMA_ERROR when diagnostics exist, or another status for
 * invalid input or allocation failure.
 */
CompilerStatus sema_run_with_warning_options(AstProgram *program, bool warn_unused_variable,
                                            bool warn_case_fall_through, SemanticModel **output)
{
    if (!program || !output)
        return COMPILER_INVALID_INPUT;
    free(program->block_externals.items);
    program->block_externals.items = NULL;
    program->block_externals.count = 0;
    program->contains_file_compound_literal = false;
    program->c11_numeric = true;
    *output = NULL;
    SemanticModel *m = calloc(1, sizeof(*m));
    if (!m)
        return COMPILER_OUT_OF_MEMORY;
    Analyzer a = {0};
    a.model = m;
    a.program = program;
    a.warn_unused_variable = warn_unused_variable;
    a.warn_case_fall_through = warn_case_fall_through;
    a.at_file_scope = true;
    Scope global = {0};
    a.scope = &global;
    for (size_t i = 0; i < program->declarations.count; ++i) {
        Decl *d = program->declarations.items[i];
        if (d) {
            d->semantic_global_definition = false;
            d->semantic_internal_linkage = false;
            if (d->kind == DECL_VARIABLE && d->as.variable.type &&
                d->as.variable.type->kind == TYPE_ARRAY) {
                if (!d->as.variable.semantic_array_length_recorded) {
                    d->as.variable.semantic_initial_array_length =
                        d->as.variable.type->array_length;
                    d->as.variable.semantic_array_length_recorded = true;
                } else {
                    d->as.variable.type->array_length =
                        d->as.variable.semantic_initial_array_length;
                }
            }
        }
    }
    for (size_t i = 0; i < program->declarations.count; ++i) {
        Decl *d = program->declarations.items[i];
        if (!d || d->kind != DECL_FUNCTION)
            continue;
        d->as.function.semantic_inline_only =
            d->as.function.body && d->as.function.is_inline && !d->is_static;
        if (!d->as.function.semantic_inline_only)
            continue;
        for (size_t j = 0; j < program->declarations.count; ++j) {
            Decl *other = program->declarations.items[j];
            if (other && other->kind == DECL_FUNCTION &&
                other->as.function.name && d->as.function.name &&
                !strcmp(other->as.function.name, d->as.function.name) &&
                (other->is_static || other->is_extern || !other->as.function.is_inline)) {
                d->as.function.semantic_inline_only = false;
                break;
            }
        }
    }
    for (size_t i = 0; i < program->declarations.count && !a.out_of_memory; i++) {
        Decl *d = program->declarations.items[i];
        if (d && d->kind == DECL_STRUCT) {
            size_t len = d->as.structure.name_length;
            const char *n = d->as.structure.name;
            if (find_local(a.model->structures, n, len))
                report(&a, d->location, "duplicate structure declaration '%s'", n);
            else {
                Symbol *s =
                    new_symbol(&a, n, len, SEMANTIC_SYMBOL_STRUCTURE, d->location, d, NULL, false);
                if (!s) {
                    sema_destroy(m);
                    return COMPILER_OUT_OF_MEMORY;
                }
                s->next = m->structures;
                m->structures = s;
            }
        }
    }
    for (size_t i = 0; i < program->declarations.count && !a.out_of_memory; i++) {
        Decl *d = program->declarations.items[i];
        if (!d)
            continue;
        a.source_offset = d->declarator_offset ? d->declarator_offset : d->location.offset;
        if (d->kind == DECL_STATIC_ASSERT) {
            check_static_assertion(&a, d);
            continue;
        }
        if (d->kind == DECL_ENUM) {
            check_enum_decl(&a, d, true);
            continue;
        }
        if (d->kind == DECL_STRUCT) {
            if (!d->as.structure.is_local)
                validate_struct_fields(&a, d);
            continue;
        }
        check_alignment_specifiers(&a, d, false);
        if (d->kind == DECL_VARIABLE) {
            validate_local_structure(&a, d->as.variable.type, 0);
            if (d->is_thread_local &&
                (!d->is_static || d->is_extern || !d->as.variable.type ||
                 d->as.variable.type->kind == TYPE_ARRAY ||
                 d->as.variable.type->kind == TYPE_STRUCT ||
                 d->as.variable.type->kind == TYPE_LONG_DOUBLE))
                report(&a, d->location,
                       "thread-local storage currently requires a file-scope static scalar");
            bool tentative_incomplete = !d->is_extern && !d->is_static &&
                d->as.variable.initializer.kind == INIT_NONE &&
                incomplete_structure(&a, d->as.variable.type);
            if (!tentative_incomplete && !incomplete_external_structure(&a, d))
                validate_type(&a, d->as.variable.type, d->location);
            Symbol *prior = lookup_global(&a, d->as.variable.name, d->as.variable.name_length);
            if (prior && prior->kind == SEMANTIC_SYMBOL_VARIABLE && prior->type &&
                prior->type->kind == TYPE_ARRAY && d->as.variable.type &&
                d->as.variable.type->kind == TYPE_ARRAY && !d->as.variable.array_size &&
                !d->as.variable.type->array_length)
                d->as.variable.type->array_length = prior->type->array_length;
            resolve_fixed_array(&a, d, true, false);
            if (d->is_static && d->as.variable.type &&
                d->as.variable.type->kind == TYPE_ARRAY &&
                !d->as.variable.type->array_length) {
                if (prior && prior->type && prior->type->kind == TYPE_ARRAY)
                    d->as.variable.type->array_length = prior->type->array_length;
                if (!d->as.variable.type->array_length)
                    report(&a, d->location, "static array definition requires a complete type");
            }
            if (d->is_extern) {
                Type *t = d->as.variable.type;
                if (!t || t->kind == TYPE_VOID || t->kind == TYPE_FUNCTION)
                    report(&a, d->location,
                           "external variables must have an object type");
            }
        } else if (d->kind == DECL_TYPE_ALIAS) {
            bool requires_long_double_ir = a.program->requires_long_double_ir;
            a.suppress_unsigned_types = true;
            validate_local_structure(&a, d->as.type_alias.type, 0);
            if (!d->as.type_alias.type || d->as.type_alias.type->kind != TYPE_STRUCT)
                validate_type(&a, d->as.type_alias.type, d->location);
            a.suppress_unsigned_types = false;
            a.program->requires_long_double_ir = requires_long_double_ir;
        } else {
            a.suppress_unsigned_types = crunch_is_runtime_declaration(d);
            a.suppress_float_types = a.suppress_unsigned_types;
            if (d->as.function.return_type && d->as.function.return_type->kind == TYPE_ARRAY)
                report(&a, d->location, "functions cannot return arrays");
            if (d->as.function.return_type && d->as.function.return_type->kind == TYPE_FUNCTION)
                report(&a, d->location, "functions cannot return functions");
            if (d->as.function.body || !incomplete_structure(&a, d->as.function.return_type))
                validate_type(&a, d->as.function.return_type, d->location);
            Scope prototype = {0};
            prototype.parent = &global;
            a.scope = &prototype;
            a.at_file_scope = false;
            for (size_t j = 0; j < d->as.function.parameters.count; j++) {
                Parameter *param = &d->as.function.parameters.items[j];
                check_embedded_enum(&a, param->enum_definition);
                validate_parameter_type(&a, param, !d->as.function.body);
                if (param->name &&
                    add_scope_symbol(&a, &prototype, param->name, param->name_length,
                                     SEMANTIC_SYMBOL_VARIABLE, param->location, NULL, param->type))
                    prototype.symbols->parameter = param;
            }
            release_symbols(&a, prototype.symbols);
            a.scope = &global;
            a.at_file_scope = true;
            a.suppress_unsigned_types = false;
            a.suppress_float_types = false;
        }
        if (!add_global(&a, d)) {
            sema_destroy(m);
            return COMPILER_OUT_OF_MEMORY;
        }
        if (d->kind == DECL_VARIABLE) {
            scan_embedded_enums(&a, d->as.variable.initializer.expression, 0);
            check_global_initializer(&a, d);
        } else if (d->kind == DECL_FUNCTION && d->as.function.body) {
            check_function_body(&a, d);
        }
    }
    a.source_offset = SIZE_MAX;
    for (Symbol *symbol = m->globals; symbol; symbol = symbol->next) {
        if (symbol->kind == SEMANTIC_SYMBOL_VARIABLE && symbol->tentative &&
            incomplete_structure(&a, symbol->type))
            report(&a, symbol->location, "tentative definition requires a complete structure type");
        if (symbol->kind == SEMANTIC_SYMBOL_VARIABLE &&
            (symbol->defined || symbol->tentative) && symbol->type &&
            symbol->type->kind == TYPE_ARRAY && !symbol->type->array_length)
            symbol->type->array_length = 1;
        if (symbol->kind == SEMANTIC_SYMBOL_VARIABLE && symbol->declaration &&
            (symbol->defined || symbol->tentative)) {
            symbol->declaration->semantic_global_definition = true;
            symbol->declaration->semantic_internal_linkage = symbol->internal;
            symbol->declaration->semantic_alignment = symbol->alignment;
        }
    }
    for (size_t i = 0; i < program->declarations.count; i++) {
        Decl *d = program->declarations.items[i];
        if (!d || d->kind != DECL_VARIABLE || !d->as.variable.type ||
            d->as.variable.type->kind != TYPE_ARRAY || d->as.variable.type->array_length)
            continue;
        Symbol *symbol = lookup_global(&a, d->as.variable.name, d->as.variable.name_length);
        if (symbol && symbol->type && symbol->type->kind == TYPE_ARRAY)
            d->as.variable.type->array_length = symbol->type->array_length;
    }
    if (a.out_of_memory) {
        sema_destroy(m);
        return COMPILER_OUT_OF_MEMORY;
    }
    *output = m;
    for (size_t i = 0; i < m->diagnostic_count; i++)
        if (!m->diagnostics[i].warning)
            return COMPILER_SEMA_ERROR;
    return COMPILER_OK;
}

/**
 * @brief Analyzes an AST with optional unused-variable warnings.
 * @param program AST program to analyze.
 * @param warn_unused_variable Whether to report unused local variables.
 * @param output Receives the semantic model, including diagnostics on semantic errors.
 * @return COMPILER_OK on success, COMPILER_SEMA_ERROR for errors, or another failure status.
 */
CompilerStatus sema_run_with_warnings(AstProgram *program, bool warn_unused_variable,
                                     SemanticModel **output)
{
    return sema_run_with_warning_options(program, warn_unused_variable, false, output);
}

/**
 * @brief Analyzes an AST with default warning settings.
 * @param program AST program to analyze.
 * @param output Receives the semantic model, including diagnostics on semantic errors.
 * @return COMPILER_OK on success, COMPILER_SEMA_ERROR for errors, or another failure status.
 */
CompilerStatus sema_run(AstProgram *program, SemanticModel **output)
{
    return sema_run_with_warnings(program, false, output);
}

/**
 * @brief Releases a semantic model and its owned data.
 * @param m Semantic model being queried or released.
 * @return No value; the function performs the documented output or state update.
 */
void sema_destroy(SemanticModel *m)
{
    if (!m)
        return;
    for (size_t i = 0; i < m->diagnostic_count; i++)
        free((void *)m->diagnostics[i].message);
    free(m->diagnostics);
    Symbol *s = m->globals;
    while (s) {
        Symbol *n = s->next;
        free(s);
        s = n;
    }
    s = m->structures;
    while (s) {
        Symbol *n = s->next;
        free(s);
        s = n;
    }
    OwnedType *t = m->owned_types;
    while (t) {
        OwnedType *n = t->next;
        free(t);
        t = n;
    }
    ProcessedEnum *processed = m->processed_enums;
    while (processed) {
        ProcessedEnum *next = processed->next;
        free(processed);
        processed = next;
    }
    OwnedInitializerPlan *plan = m->initializer_plans;
    while (plan) {
        OwnedInitializerPlan *next = plan->next;
        free(plan->plan.items);
        free(plan);
        plan = next;
    }
    free(m);
}
/**
 * @brief Returns the number of diagnostics in a semantic model.
 * @param m Semantic model being queried.
 * @return Count or size computed by the operation.
 */
size_t sema_diagnostic_count(const SemanticModel *m)
{
    return m ? m->diagnostic_count : 0;
}
/**
 * @brief Returns a diagnostic by index or NULL when out of range.
 * @param m Semantic model being queried.
 * @param i Zero-based index into the collection.
 * @return Diagnostic at index, or NULL when the model is NULL or the index is out of range.
 */
const SemanticDiagnostic *sema_diagnostic_at(const SemanticModel *m, size_t i)
{
    return m && i < m->diagnostic_count ? &m->diagnostics[i] : NULL;
}
/**
 * @brief Returns the number of global symbols in a semantic model.
 * @param m Semantic model being queried.
 * @return Count or size computed by the operation.
 */
size_t sema_global_symbol_count(const SemanticModel *m)
{
    return m ? m->global_count : 0;
}
