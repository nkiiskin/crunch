#include "ir/ir.h"
#include "common/diagnostic.h"
#include "common/constant_expr.h"
#include "common/runtime_declaration.h"
#include "sema/sema.h"
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    unsigned slot;
    const Type *type;
    int alias;
} Local;
enum { LOCAL_FIELD_ALIAS = 1, LOCAL_STATIC_ARRAY = 2, LOCAL_STATIC_SCALAR = 3,
       LOCAL_STATIC_STRUCT = 4, LOCAL_EXTENDED = 5, LOCAL_EXTENDED_PARAM = 6,
       LOCAL_EXTENDED_STATIC = 7, LOCAL_EXTERN = 8, LOCAL_ALIGNED_SCALAR = 9,
       LOCAL_VLA = 10 };
typedef struct {
    const AstProgram *program;
    FILE *out;
    FILE *static_out;
    char *error;
    size_t error_size;
    Diagnostic *diagnostic;
    SourceLocation location;
    Local locals[CRUNCH_LOCAL_LIMIT];
    size_t count;
    char *owned_local_names[CRUNCH_LOCAL_LIMIT];
    size_t owned_local_scopes[CRUNCH_LOCAL_LIMIT];
    size_t owned_local_name_count;
    Type pointer_types[512];
    size_t pointer_count;
    unsigned next_slot, next_label, next_string, next_extended;
    unsigned vla_parameter_sizes[CRUNCH_LOCAL_LIMIT];
    unsigned goto_label_base;
    size_t goto_label_count;
    unsigned next_static_local;
    unsigned numeric_postfix_slots[TYPE_ENUM + 1];
    unsigned fixed_array_copy_slot;
    unsigned volatile_read_slot;
    unsigned next_field_alias;
    unsigned break_labels[128];
    size_t break_vla_counts[128];
    size_t break_depth;
    unsigned continue_labels[128];
    size_t continue_vla_counts[128];
    size_t continue_depth;
    const Decl *vla_definitions[512];
    size_t vla_definition_count;
    uint64_t vla_label_bits[512][8];
    unsigned active_vla_ids[512];
    unsigned active_vla_marks[512];
    size_t active_vla_count;
    unsigned expression_depth, type_depth, statement_depth;
    size_t fixed_array_bytes;
    int recursion_limit_exceeded;
    int out_of_memory;
    int module;
    int typed_ir;
    int function_has_vla;
    int function_has_alloca;
    const Type *return_type;
    const char *function_name;
    Type function_name_type;
    unsigned function_name_string;
} Emitter;
enum { IR_MAX_AST_RECURSION_DEPTH = 64, IR_MAX_STATEMENT_ANALYSIS_DEPTH = 128 };
static _Thread_local Diagnostic *active_ir_diagnostic;
/**
 * @brief Identifies runtime calls that bypass ordinary module linkage.
 * @param declaration Function declaration to inspect.
 * @return Nonzero for a runtime-only module call.
 */
static int module_runtime_declaration(const Decl *declaration) {
    /* link has a weak public runtime symbol so user modules can override it. */
    return strcmp(declaration->as.function.name, "link") &&
           crunch_is_runtime_declaration(declaration) &&
           (!crunch_is_linkable_runtime_declaration(declaration) ||
            !declaration->as.function.runtime_referenced);
}

/**
 * @brief Releases generated local names whose scope is being exited.
 * @param e IR emitter whose generated local names are tracked.
 * @param scope First local index belonging to the scope being released.
 * @return No value; frees matching names and retains names from outer scopes.
 */
static void release_local_names(Emitter *e, size_t scope) {
    size_t kept = 0;
    for (size_t i = 0; i < e->owned_local_name_count; ++i) {
        if (e->owned_local_scopes[i] >= scope) {
            free(e->owned_local_names[i]);
        } else {
            e->owned_local_names[kept] = e->owned_local_names[i];
            e->owned_local_scopes[kept++] = e->owned_local_scopes[i];
        }
    }
    e->owned_local_name_count = kept;
}
/**
 * @brief Tracks an allocated local-name string for scope-based cleanup.
 * @param e IR emitter whose generated local names are tracked.
 * @param name Allocated name string to retain until its scope ends.
 * @return True when tracked; false when the tracking table is full.
 */
static int track_local_name(Emitter *e, char *name) {
    if (e->owned_local_name_count >= CRUNCH_LOCAL_LIMIT)
        return 0;
    e->owned_local_names[e->owned_local_name_count] = name;
    e->owned_local_scopes[e->owned_local_name_count++] = e->count;
    return 1;
}
/**
 * @brief Records an IR-generation diagnostic at the current source location.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param fmt printf-style format string.
 * @return No value; the function performs the documented output or state update.
 */
static void fail(Emitter *e, const char *fmt, ...) {
    va_list ap;
    Diagnostic *d = e->diagnostic ? e->diagnostic : active_ir_diagnostic;
    if (!e->error || !e->error_size || e->error[0])
        return;
    va_start(ap, fmt);
    vsnprintf(e->error, e->error_size, fmt, ap);
    va_end(ap);
    if (d) {
        d->code = e->out_of_memory ? "E_OUT_OF_MEMORY" : "E_IR_GENERATION";
        d->message = e->error;
        d->severity = DIAGNOSTIC_ERROR;
        d->source_location = e->location;
    }
}
/**
 * @brief Records an allocation failure during IR generation.
 * @param e IR emitter whose failure status and diagnostic are updated.
 * @return No value.
 */
static void fail_oom(Emitter *e) {
    e->out_of_memory = 1;
    fail(e, "out of memory generating IR");
}
static const Type TYPE_BYTE_VALUE = {TYPE_BYTE, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false}, TYPE_INT_VALUE = {TYPE_INT, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false},
                  TYPE_LONG_VALUE = {TYPE_LONG, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false}, TYPE_REAL_VALUE = {TYPE_REAL, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false},
                  TYPE_FLOAT_VALUE = {TYPE_FLOAT, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false},
                  TYPE_LONG_DOUBLE_VALUE = {TYPE_LONG_DOUBLE, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false},
                  TYPE_UINT_VALUE = {TYPE_UNSIGNED_INT, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false},
                  TYPE_ULONG_VALUE = {TYPE_UNSIGNED_LONG, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false};
static const Type TYPE_LLONG_VALUE = {TYPE_LONG_LONG, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false},
                  TYPE_ULLONG_VALUE = {TYPE_UNSIGNED_LONG_LONG, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false};
static const Type TYPE_VOID_VALUE = {TYPE_VOID, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false};

extern double _crunch_ir_pow_helper(double base, double exponent);

static int numeric_initializer_expression(const Expr *x, unsigned depth);

/**
 * @brief Checks that every part of an integer initializer is constant.
 * @param x Expression to inspect.
 * @param depth Current recursion depth.
 * @return Nonzero for supported integer constant-expression forms.
 */
static int integer_initializer_expression(const Expr *x, unsigned depth) {
    if (!x || depth >= IR_MAX_AST_RECURSION_DEPTH)
        return 0;
    switch (x->kind) {
    case EXPR_INTEGER:
    case EXPR_CHAR:
        return 1;
    case EXPR_OFFSETOF:
        return x->as.offset_of.resolved && x->as.offset_of.constant;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        return x->as.size_of.semantic_vla_decl == NULL &&
               x->as.size_of.semantic_vla_parameter == NULL;
    case EXPR_NAME:
        return x->semantic_enum_constant;
    case EXPR_UNARY:
        switch (x->as.unary.op) {
        case UNARY_PLUS:
        case UNARY_MINUS:
        case UNARY_LOGICAL_NOT:
        case UNARY_BITWISE_NOT:
            return integer_initializer_expression(x->as.unary.operand, depth + 1);
        default:
            return 0;
        }
    case EXPR_BINARY:
        if (x->as.binary.op == BINARY_POWER || x->as.binary.op == BINARY_COMMA)
            return 0;
        if (x->as.binary.op >= BINARY_EQUAL && x->as.binary.op <= BINARY_GREATER_EQUAL)
            return numeric_initializer_expression(x->as.binary.left, depth + 1) &&
                   numeric_initializer_expression(x->as.binary.right, depth + 1);
        return integer_initializer_expression(x->as.binary.left, depth + 1) &&
               integer_initializer_expression(x->as.binary.right, depth + 1);
    case EXPR_CONDITIONAL:
        return numeric_initializer_expression(x->as.conditional.condition, depth + 1) &&
               integer_initializer_expression(x->as.conditional.then_expr, depth + 1) &&
               integer_initializer_expression(x->as.conditional.else_expr, depth + 1);
    case EXPR_CAST:
        return x->as.cast.target_type &&
               (x->as.cast.target_type->kind == TYPE_BYTE ||
               x->as.cast.target_type->kind == TYPE_BOOL ||
                x->as.cast.target_type->kind == TYPE_SIGNED_CHAR ||
                x->as.cast.target_type->kind == TYPE_UNSIGNED_CHAR ||
                x->as.cast.target_type->kind == TYPE_SHORT ||
                x->as.cast.target_type->kind == TYPE_UNSIGNED_SHORT ||
                x->as.cast.target_type->kind == TYPE_INT ||
                x->as.cast.target_type->kind == TYPE_ENUM ||
                x->as.cast.target_type->kind == TYPE_LONG ||
                x->as.cast.target_type->kind == TYPE_LONG_LONG ||
                x->as.cast.target_type->kind == TYPE_UNSIGNED_INT ||
                x->as.cast.target_type->kind == TYPE_UNSIGNED_LONG ||
                x->as.cast.target_type->kind == TYPE_UNSIGNED_LONG_LONG) &&
               integer_initializer_expression(x->as.cast.operand, depth + 1);
    default:
        return 0;
    }
}

/**
 * @brief Checks supported numeric constant-expression forms.
 * @param x Expression to inspect.
 * @param depth Current recursion depth.
 * @return Nonzero for supported numeric constant-expression forms.
 */
static int numeric_initializer_expression(const Expr *x, unsigned depth) {
    if (!x || depth >= IR_MAX_AST_RECURSION_DEPTH)
        return 0;
    if (x->kind == EXPR_REAL)
        return 1;
    if (x->kind == EXPR_CAST && x->as.cast.target_type &&
        x->as.cast.target_type->kind == TYPE_REAL)
        return numeric_initializer_expression(x->as.cast.operand, depth + 1);
    if (x->kind == EXPR_CONDITIONAL)
        return numeric_initializer_expression(x->as.conditional.condition, depth + 1) &&
               numeric_initializer_expression(x->as.conditional.then_expr, depth + 1) &&
               numeric_initializer_expression(x->as.conditional.else_expr, depth + 1);
    if (x->kind == EXPR_UNARY &&
        (x->as.unary.op == UNARY_PLUS || x->as.unary.op == UNARY_MINUS))
        return numeric_initializer_expression(x->as.unary.operand, depth + 1);
    if (x->kind == EXPR_BINARY &&
        (x->as.binary.op == BINARY_ADD || x->as.binary.op == BINARY_SUBTRACT ||
         x->as.binary.op == BINARY_MULTIPLY || x->as.binary.op == BINARY_DIVIDE ||
         x->as.binary.op == BINARY_POWER))
        return numeric_initializer_expression(x->as.binary.left, depth + 1) &&
               numeric_initializer_expression(x->as.binary.right, depth + 1);
    return integer_initializer_expression(x, depth);
}

/**
 * @brief Returns the textual IR spelling for a Crunch type.
 * @param t Crunch type node being inspected or emitted.
 * @return Static IR spelling for the type, or "unsupported" when the type is not representable.
 */
static const char *tn(const Type *t) {
    if (!t)
        return "int";
    switch (t->kind) {
    case TYPE_BYTE:
    case TYPE_BOOL:
        return "byte";
    case TYPE_UNSIGNED_CHAR:
        return "ubyte";
    case TYPE_SIGNED_CHAR:
        return "sbyte";
    case TYPE_SHORT:
        return "short";
    case TYPE_UNSIGNED_SHORT:
        return "ushort";
    case TYPE_INT:
        return "int";
    case TYPE_ENUM:
        return type_effective_kind(t) == TYPE_UNSIGNED_INT ? "uint" : "int";
    case TYPE_LONG:
    case TYPE_LONG_LONG:
        return "long";
    case TYPE_UNSIGNED_INT:
        return "uint";
    case TYPE_UNSIGNED_LONG:
    case TYPE_UNSIGNED_LONG_LONG:
        return "ulong";
    case TYPE_REAL:
        return "real";
    case TYPE_FLOAT:
        return "float";
    case TYPE_VOID:
        return "void";
    case TYPE_POINTER:
        return "ptr";
    default:
        return "unsupported";
    }
}
/**
 * @brief Checks whether a type is a supported numeric scalar.
 * @param t Crunch type node being inspected or emitted.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int isnum(const Type *t) {
    return t && (t->kind == TYPE_BYTE || t->kind == TYPE_BOOL || t->kind == TYPE_SIGNED_CHAR ||
                 t->kind == TYPE_UNSIGNED_CHAR || t->kind == TYPE_SHORT ||
                 t->kind == TYPE_UNSIGNED_SHORT || t->kind == TYPE_INT || t->kind == TYPE_ENUM || t->kind == TYPE_LONG ||
                 t->kind == TYPE_LONG_LONG || t->kind == TYPE_UNSIGNED_LONG_LONG ||
                 t->kind == TYPE_UNSIGNED_INT || t->kind == TYPE_UNSIGNED_LONG ||
                 t->kind == TYPE_REAL || t->kind == TYPE_FLOAT);
}
/**
 * @brief Checks whether a pointer can be stored or returned as a value.
 * @param t Pointer type to inspect.
 * @return One for pointers to supported value or structure types.
 */
static int is_value_pointer(const Type *t) {
    if (!t || t->kind != TYPE_POINTER)
        return 0;
    while (t && (t->kind == TYPE_POINTER || t->kind == TYPE_ARRAY))
        t = t->pointee;
    return isnum(t) || (t && (t->kind == TYPE_LONG_DOUBLE || t->kind == TYPE_VOID || t->kind == TYPE_STRUCT ||
                              t->kind == TYPE_FUNCTION));
}
/**
 * @brief Checks whether a function parameter type is representable in IR.
 * @param t Crunch type node being inspected or emitted.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int is_supported_parameter(const Type *t) {
    return isnum(t) || is_value_pointer(t) || (t && t->kind == TYPE_LONG_DOUBLE);
}
/**
 * @brief Recognizes the supported byte-string vector type.
 * @param t Crunch type node being inspected or emitted.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int is_byte_string_vector(const Type *t) {
    return t && t->kind == TYPE_POINTER && t->pointee && t->pointee->kind == TYPE_POINTER &&
           t->pointee->pointee && t->pointee->pointee->kind == TYPE_BYTE;
}
/**
 * @brief Returns an emitter-owned pointer type for the requested pointee.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param pointee Crunch type node being inspected or emitted.
 * @return Emitter-owned pointer type, or NULL if the type limit is reached.
 */
static const Type *pointer_to(Emitter *e, const Type *pointee) {
    for (size_t i = 0; i < e->pointer_count; i++)
        if (e->pointer_types[i].kind == TYPE_POINTER && e->pointer_types[i].pointee == pointee)
            return &e->pointer_types[i];
    if (e->pointer_count >= 512) {
        fail(e, "too many pointer types");
        return NULL;
    }
    Type *t = &e->pointer_types[e->pointer_count++];
    memset(t, 0, sizeof(*t));
    t->kind = TYPE_POINTER;
    t->pointee = (Type *)pointee;
    t->struct_name = NULL;
    return t;
}
/**
 * @brief Returns an emitter-owned function type for a declaration.
 * @param e IR emitter.
 * @param declaration Borrowed function declaration.
 * @return Function type, or NULL if the type limit is reached.
 */
static const Type *function_type(Emitter *e, Decl *declaration) {
    for (size_t i = 0; i < e->pointer_count; ++i)
        if (e->pointer_types[i].kind == TYPE_FUNCTION &&
            e->pointer_types[i].function_signature == declaration)
            return &e->pointer_types[i];
    if (e->pointer_count >= 512) {
        fail(e, "too many function types");
        return NULL;
    }
    Type *type = &e->pointer_types[e->pointer_count++];
    memset(type, 0, sizeof(*type));
    type->kind = TYPE_FUNCTION;
    type->function_signature = declaration;
    return type;
}
/**
 * @brief Returns the numeric widening rank of a type.
 * @param t Crunch type node being inspected or emitted.
 * @return Numeric widening rank (byte 0, int 1, long 2, real 3), or -1 for a nonnumeric type.
 */
static int rankt(const Type *t) {
    if (!t)
        return -1;
    switch (t->kind) {
    case TYPE_BYTE:
        return 0;
    case TYPE_INT:
        return 1;
    case TYPE_LONG:
        return 2;
    case TYPE_REAL:
        return 3;
    default:
        return -1;
    }
}
/**
 * @brief Identifies character and short types that promote to int.
 * @param kind Numeric type kind.
 * @return Nonzero for explicitly signed or unsigned character and short types.
 */
static int promoted_small_kind(TypeKind kind) {
    return kind == TYPE_BOOL || kind == TYPE_SIGNED_CHAR || kind == TYPE_UNSIGNED_CHAR ||
           kind == TYPE_SHORT || kind == TYPE_UNSIGNED_SHORT;
}
/**
 * @brief Identifies the three character types.
 * @param kind Numeric type kind.
 * @return Nonzero for plain, signed, or unsigned char.
 */
static int character_kind(TypeKind kind) {
    return kind == TYPE_BYTE || kind == TYPE_SIGNED_CHAR || kind == TYPE_UNSIGNED_CHAR;
}
/**
 * @brief Checks whether an array element can be initialized by a literal.
 * @param element Array element type.
 * @param literal String expression.
 * @return Nonzero for matching narrow or target wide character types.
 */
static int string_element_matches(const Type *element, const Expr *literal) {
    return element && literal && literal->kind == EXPR_STRING &&
           (literal->string_literal_wide ? element->kind == TYPE_INT
                                         : character_kind(element->kind));
}
/**
 * @brief Finds the common promoted type of two numeric operands.
 * @param a Crunch type node being inspected or emitted.
 * @param b Crunch type node being inspected or emitted.
 * @param c11_numeric Whether plain char undergoes integer promotion.
 * @return Common numeric type, or NULL when either input is not numeric.
 */
static const Type *promote(const Type *a, const Type *b, int c11_numeric) {
    if (a && b &&
        ((a->kind == TYPE_LONG_DOUBLE && (isnum(b) || b->kind == TYPE_LONG_DOUBLE)) ||
         (b->kind == TYPE_LONG_DOUBLE && isnum(a))))
        return &TYPE_LONG_DOUBLE_VALUE;
    if (!isnum(a) || !isnum(b))
        return NULL;
    if (a->kind == TYPE_ENUM)
        a = type_effective_kind(a) == TYPE_UNSIGNED_INT ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE;
    if (b->kind == TYPE_ENUM)
        b = type_effective_kind(b) == TYPE_UNSIGNED_INT ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE;
    if (promoted_small_kind(a->kind) || (c11_numeric && a->kind == TYPE_BYTE))
        a = &TYPE_INT_VALUE;
    if (promoted_small_kind(b->kind) || (c11_numeric && b->kind == TYPE_BYTE))
        b = &TYPE_INT_VALUE;
    if (a->kind == TYPE_REAL || b->kind == TYPE_REAL)
        return &TYPE_REAL_VALUE;
    if (a->kind == TYPE_FLOAT || b->kind == TYPE_FLOAT)
        return &TYPE_FLOAT_VALUE;
    if (a->kind == TYPE_UNSIGNED_LONG_LONG || b->kind == TYPE_UNSIGNED_LONG_LONG)
        return &TYPE_ULLONG_VALUE;
    if (a->kind == TYPE_LONG_LONG || b->kind == TYPE_LONG_LONG)
        return a->kind == TYPE_UNSIGNED_LONG || b->kind == TYPE_UNSIGNED_LONG
                   ? &TYPE_ULLONG_VALUE : &TYPE_LLONG_VALUE;
    if (a->kind == TYPE_UNSIGNED_LONG || b->kind == TYPE_UNSIGNED_LONG)
        return &TYPE_ULONG_VALUE;
    if (a->kind == TYPE_UNSIGNED_INT || b->kind == TYPE_UNSIGNED_INT)
        return a->kind == TYPE_LONG || b->kind == TYPE_LONG ? &TYPE_LONG_VALUE : &TYPE_UINT_VALUE;
    int x = rankt(a), y = rankt(b), r = x > y ? x : y;
    return r == 0   ? &TYPE_BYTE_VALUE
           : r == 1 ? &TYPE_INT_VALUE
           : r == 2 ? &TYPE_LONG_VALUE
           : r == 3 ? &TYPE_REAL_VALUE
                    : NULL;
}
/**
 * @brief Finds a local symbol and returns its slot and type.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param n Identifier name to search for or emit.
 * @param s Output receiving the local variable slot.
 * @param t Optional output receiving the local variable type.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int lookup(Emitter *e, const char *n, unsigned *s, const Type **t) {
    for (size_t i = e->count; i > 0; i--)
        if (!strcmp(e->locals[i - 1].name, n)) {
            if (e->locals[i - 1].alias == LOCAL_EXTERN)
                return 0;
            *s = e->locals[i - 1].slot;
            if (t)
                *t = e->locals[i - 1].type;
            return 1;
        }
    return 0;
}
/**
 * @brief Identifies the special storage kind of a local name.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param n Identifier name to search for or emit.
 * @return The storage kind, or zero for an ordinary slot or absent name.
 */
static int local_alias(Emitter *e, const char *n) {
    for (size_t i = e->count; i > 0; i--)
        if (!strcmp(e->locals[i - 1].name, n))
            return e->locals[i - 1].alias;
    return 0;
}
static int emit_fixed_global_array(Emitter *, const Decl *, const char *);
static int emit_static_local_scalar(Emitter *, const Decl *);
/**
 * @brief Emits a typed load from local storage or a field alias.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param n Identifier name to search for or emit.
 * @param slot Local-variable slot number.
 * @param t Crunch type node being inspected or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_local_load(Emitter *e, const char *n, unsigned slot, const Type *t) {
    if (local_alias(e, n) == LOCAL_FIELD_ALIAS)
        fprintf(e->out, "lload __local_field_%u %s\n", slot, tn(t));
    else if (local_alias(e, n) == LOCAL_ALIGNED_SCALAR)
        fprintf(e->out, "lload __aligned_scalar_%u %s\n", slot, tn(t));
    else if (local_alias(e, n) == LOCAL_STATIC_SCALAR)
        fprintf(e->out, "gload _crunch_static_local_%u %s\n", slot, tn(t));
    else
        fprintf(e->out, "load %u\n", slot);
}
/**
 * @brief Emits a typed store to local storage or a field alias.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param n Identifier name to search for or emit.
 * @param slot Local-variable slot number.
 * @param t Crunch type node being inspected or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_local_store(Emitter *e, const char *n, unsigned slot, const Type *t) {
    if (local_alias(e, n) == LOCAL_FIELD_ALIAS)
        fprintf(e->out, "lstore __local_field_%u %s\n", slot, tn(t));
    else if (local_alias(e, n) == LOCAL_ALIGNED_SCALAR)
        fprintf(e->out, "lstore __aligned_scalar_%u %s\n", slot, tn(t));
    else if (local_alias(e, n) == LOCAL_STATIC_SCALAR)
        fprintf(e->out, "gstore _crunch_static_local_%u %s\n", slot, tn(t));
    else
        fprintf(e->out, "store %u %s\n", slot, tn(t));
}
/**
 * @brief Emits the address of a local variable or field alias.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param n Identifier name to search for or emit.
 * @param slot Local-variable slot number.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_local_addr(Emitter *e, const char *n, unsigned slot) {
    if (local_alias(e, n) == LOCAL_FIELD_ALIAS)
        fprintf(e->out, "laddr __local_field_%u\n", slot);
    else if (local_alias(e, n) == LOCAL_ALIGNED_SCALAR)
        fprintf(e->out, "laddr __aligned_scalar_%u\n", slot);
    else if (local_alias(e, n) == LOCAL_STATIC_SCALAR)
        fprintf(e->out, "gaddr _crunch_static_local_%u\n", slot);
    else if (local_alias(e, n) == LOCAL_EXTENDED)
        fprintf(e->out, "laddr __extended_local_address_%u\n", slot);
    else if (local_alias(e, n) == LOCAL_EXTENDED_PARAM)
        fprintf(e->out, "load %u\n", slot);
    else if (local_alias(e, n) == LOCAL_EXTENDED_STATIC)
        fprintf(e->out, "gaddr _crunch_static_local_%u\n", slot);
    else
        fprintf(e->out, "addr %u\n", slot);
}
/**
 * @brief Finds a global declaration by name.
 * @param p AST program being searched or emitted.
 * @param n Identifier name to search for or emit.
 * @return Matching global variable declaration, or NULL when absent.
 */
static Decl *find_global(const AstProgram *p, const char *n) {
    Decl *declaration = NULL;
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (d && d->kind == DECL_VARIABLE && d->as.variable.name && !strcmp(d->as.variable.name, n)) {
            if (d->semantic_global_definition)
                return d;
            declaration = d;
        }
    }
    if (!declaration)
        for (size_t i = 0; i < p->block_externals.count; ++i) {
            Decl *d = p->block_externals.items[i];
            if (d->kind == DECL_VARIABLE && !strcmp(d->as.variable.name, n))
                return d;
        }
    return declaration;
}
/**
 * @brief Finds a function declaration by name.
 * @param p AST program being searched or emitted.
 * @param n Identifier name to search for or emit.
 * @return Matching function declaration, or NULL when absent.
 */
static Decl *find_function(const AstProgram *p, const char *n) {
    Decl *prototype = NULL;
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (d && d->kind == DECL_FUNCTION && d->as.function.name && !strcmp(d->as.function.name, n)) {
            if (d->as.function.body)
                return d;
            if (!prototype || d->is_extern || !prototype->is_extern)
                prototype = d;
        }
    }
    if (!prototype)
        for (size_t i = 0; i < p->block_externals.count; ++i) {
            Decl *d = p->block_externals.items[i];
            if (d->kind == DECL_FUNCTION && !strcmp(d->as.function.name, n))
                return d;
        }
    return prototype;
}
/**
 * @brief Finds a structure declaration by name.
 * @param p AST program being searched or emitted.
 * @param n Identifier name to search for or emit.
 * @return Matching structure declaration, or NULL when absent.
 */
static Decl *find_struct(const AstProgram *p, const char *n) {
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (d && d->kind == DECL_STRUCT && d->as.structure.name && !strcmp(d->as.structure.name, n))
            return d;
    }
    return NULL;
}
/**
 * @brief Finds a named field in a structure type.
 * @param p AST program being searched or emitted.
 * @param t Crunch type node being inspected or emitted.
 * @param n Identifier name to search for or emit.
 * @param depth Current nesting depth.
 * @return Matching structure-field declaration, or NULL when absent.
 */
static Decl *struct_field_depth(const AstProgram *p, const Type *t, const char *n, unsigned depth) {
    if (!t || t->kind != TYPE_STRUCT || !t->struct_name || depth > 24)
        return NULL;
    Decl *d = find_struct(p, t->struct_name);
    if (!d)
        return NULL;
    for (size_t i = 0; i < d->as.structure.field_count; i++) {
        Decl *f = d->as.structure.fields[i];
        if (f && f->as.variable.anonymous_member) {
            Decl *nested = struct_field_depth(p, f->as.variable.type, n, depth + 1);
            if (nested)
                return nested;
        } else if (f && f->as.variable.name && !strcmp(f->as.variable.name, n))
            return f;
    }
    return NULL;
}
/**
 * @brief Finds a field including promoted anonymous members.
 * @param p AST program containing structure declarations.
 * @param t Structure type.
 * @param n Field name.
 * @return Matching field, or NULL if absent.
 */
static Decl *struct_field(const AstProgram *p, const Type *t, const char *n) {
    return struct_field_depth(p, t, n, 0);
}
static const Type *expr_type(Emitter *e, const Expr *x);
/**
 * @brief Finds the structure type that owns a member expression.
 * @param e IR emitter state.
 * @param x Member expression.
 * @return Structure type, or NULL if the base is invalid.
 */
static const Type *member_base_type(Emitter *e, const Expr *x) {
    if (!x || x->kind != EXPR_MEMBER || !x->as.member.base)
        return NULL;
    const Type *base = expr_type(e, x->as.member.base);
    if (x->as.member.through_pointer)
        return base && base->kind == TYPE_POINTER ? base->pointee : NULL;
    return base;
}
/**
 * @brief Resolves the type of a structure-member expression.
 * @param e IR emitter state containing the program, output stream, and diagnostic context.
 * @param x Expression node being inspected or emitted.
 * @return Type of the structure member, or NULL when the expression is invalid or unsupported.
 */
static const Type *member_type(Emitter *e, const Expr *x) {
    const Type *base = member_base_type(e, x);
    Decl *f = base ? struct_field(e->program, base, x->as.member.name) : NULL;
    return f ? f->as.variable.type : NULL;
}
/**
 * @brief Builds a flattened local or global member path.
 * @param x Expression node being inspected or emitted.
 * @param out Writable character buffer receiving output.
 * @param cap Capacity or size in bytes.
 * @param global Whether the storage or name is global rather than local.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int member_path(const Expr *x, char *out, size_t cap, int global) {
    if (!x)
        return 0;
    if (x->kind == EXPR_NAME) {
        int r = snprintf(out, cap, "%s", x->as.name.name);
        return r >= 0 && (size_t)r < cap;
    }
    if (x->kind != EXPR_MEMBER || !x->as.member.base || x->as.member.through_pointer)
        return 0;
    char base[512];
    if (!member_path(x->as.member.base, base, sizeof base, global))
        return 0;
    int r = snprintf(out, cap, global ? "%s__%s" : "%s.%s", base, x->as.member.name);
    return r >= 0 && (size_t)r < cap;
}
/**
 * @brief Builds the storage key for a member expression.
 * @param x Expression node being inspected or emitted.
 * @param out Writable character buffer receiving output.
 * @param cap Capacity or size in bytes.
 * @param global Whether the storage or name is global rather than local.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int field_storage_key(const Expr *x, char *out, size_t cap, int global) {
    if (!x || x->kind != EXPR_MEMBER)
        return 0;
    return member_path(x, out, cap, global);
}
/**
 * @brief Checks whether a field has a supported scalar or pointer type.
 * @param t Crunch type node being inspected or emitted.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int scalar_field(const Type *t) {
    return isnum(t) || is_value_pointer(t) || (t && t->kind == TYPE_LONG_DOUBLE);
}
/**
 * @brief Tests whether an expression is an integer constant zero.
 * @param x Expression to evaluate.
 * @return Nonzero when x is a supported constant zero; otherwise zero.
 */
static int is_null_literal(const Expr *x) {
    if (x && x->kind == EXPR_CAST && x->as.cast.target_type &&
        x->as.cast.target_type->kind == TYPE_POINTER)
        return is_null_literal(x->as.cast.operand);
    if (x && x->semantic_unsigned) {
        uint64_t bits;
        TypeKind kind;
        return sema_integer_constant(x, &bits, &kind) && bits == 0;
    }
    return constant_expr_is_zero(x);
}
static int expression(Emitter *, const Expr *);
static int emit_aggregate_address(Emitter *, const Expr *, unsigned);
static int emit_discarded_expression(Emitter *, const Expr *, unsigned);
static int emit_compound_literal(Emitter *, const Expr *);
static int is_supported_array(const Type *);
/**
 * @brief Resolves a structure name from the IR program.
 * @param context AST program.
 * @param name Structure name.
 * @return Matching declaration, or NULL.
 */
static const Decl *layout_find_struct(const void *context, const char *name) {
    return find_struct(context, name);
}
/**
 * @brief Computes field offsets and the aligned structure size.
 * @param p AST program being searched or emitted.
 * @param sd Declaration node being inspected or emitted.
 * @param offsets Output array receiving each field offset in bytes.
 * @param size Output parameter receiving the size value.
 * @param alignment Required storage alignment in bytes.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int struct_layout(const AstProgram *p, const Decl *sd, size_t *offsets, size_t *size,
                         size_t *alignment) {
    return type_layout_struct(sd, layout_find_struct, p, offsets, size, alignment);
}
static int requires_object_copy(const AstProgram *, const Type *, unsigned);

/**
 * @brief Identifies a structure requiring binary80 object alignment.
 * @param p Program containing structure declarations.
 * @param t Structure type.
 * @return Nonzero for a structure with 16-byte alignment.
 */
static int aggregate_aligned_16(const AstProgram *p, const Type *t) {
    size_t size, alignment;
    return t && t->kind == TYPE_STRUCT &&
           type_layout_size(t, layout_find_struct, p, &size, &alignment) && alignment == 16;
}

/**
 * @brief Merges one System V eightbyte class.
 * @param current Existing class.
 * @param incoming Class of an overlapping field.
 * @return Merged class, or five for memory class.
 */
static unsigned merge_extended_class(unsigned current, unsigned incoming) {
    if (!current || current == incoming)
        return incoming;
    if (!incoming)
        return current;
    if (current == 5 || incoming == 5)
        return 5;
    if (current == 2 || incoming == 2)
        return 2;
    return 5;
}

/**
 * @brief Classifies fields of a sixteen-byte binary80-containing aggregate.
 * @param p Program containing structure declarations.
 * @param t Field type.
 * @param offset Field byte offset.
 * @param classes Two System V eightbyte classes.
 * @param depth Current nesting depth.
 * @return Nonzero when the layout can be classified.
 */
static int aggregate_extended_fields(const AstProgram *p, const Type *t, size_t offset,
                                     unsigned *classes, unsigned depth) {
    if (!t || depth > 24)
        return 0;
    if (t->kind == TYPE_STRUCT) {
        const Decl *d = find_struct(p, t->struct_name);
        FieldLayout fields[256];
        size_t size, alignment;
        if (!d || d->as.structure.field_count > 256 ||
            !type_layout_struct_fields(d, layout_find_struct, p, fields, &size, &alignment))
            return 0;
        for (size_t i = 0; i < d->as.structure.field_count; ++i) {
            const Decl *field = d->as.structure.fields[i];
            if (field_is_zero_storage(field))
                continue;
            if (field->as.variable.is_bit_field) {
                size_t bytes = (fields[i].bit_offset + fields[i].bit_width + 7) / 8;
                size_t start = offset + fields[i].offset;
                if (bytes && (start >= 16 || bytes > 16 - start))
                    return 0;
                for (size_t j = 0; j < bytes; ++j)
                    classes[(start + j) / 8] =
                        merge_extended_class(classes[(start + j) / 8], 2);
            } else if (!aggregate_extended_fields(p, field->as.variable.type,
                                                  offset + fields[i].offset, classes, depth + 1))
                return 0;
        }
        return 1;
    }
    if (t->kind == TYPE_ARRAY) {
        size_t size, alignment;
        if (!type_layout_size(t->pointee, layout_find_struct, p, &size, &alignment))
            return 0;
        for (size_t i = 0; i < t->array_length; ++i)
            if (!aggregate_extended_fields(p, t->pointee, offset + i * size, classes, depth + 1))
                return 0;
        return 1;
    }
    size_t size, alignment;
    if ((!isnum(t) && !is_value_pointer(t) && t->kind != TYPE_LONG_DOUBLE) ||
        !type_layout_size(t, layout_find_struct, p, &size, &alignment) ||
        !size || offset >= 16 || size > 16 - offset)
        return 0;
    if (t->kind == TYPE_LONG_DOUBLE) {
        if (offset != 0)
            return 0;
        classes[0] = merge_extended_class(classes[0], 3);
        classes[1] = merge_extended_class(classes[1], 4);
    } else {
        unsigned kind = t->kind == TYPE_REAL || t->kind == TYPE_FLOAT ? 1 : 2;
        for (size_t i = offset / 8; i <= (offset + size - 1) / 8; ++i)
            classes[i] = merge_extended_class(classes[i], kind);
    }
    return 1;
}

/**
 * @brief Classifies the eightbytes of a supported aggregate recursively.
 * @param p Program containing structure declarations.
 * @param t Type to classify.
 * @param offset Starting byte offset within the aggregate.
 * @param classes Two eightbyte classes, updated in place.
 * @param depth Current nesting depth.
 * @return Nonzero for a supported type.
 */
static int aggregate_fields(const AstProgram *p, const Type *t, size_t offset,
                            unsigned *classes, unsigned depth) {
    if (!t || depth > 24)
        return 0;
    if (t->kind == TYPE_ARRAY) {
        size_t size, alignment;
        if (!t->array_length || !type_layout_size(t->pointee, layout_find_struct, p,
                                                   &size, &alignment))
            return 0;
        for (size_t i = 0; i < t->array_length; i++)
            if (!aggregate_fields(p, t->pointee, offset + i * size, classes, depth + 1))
                return 0;
        return 1;
    }
    if (t->kind == TYPE_STRUCT) {
        if (!t->struct_name)
            return 0;
        const Decl *sd = find_struct(p, t->struct_name);
        FieldLayout fields[256];
        size_t size, alignment;
        if (!sd || !sd->as.structure.field_count || sd->as.structure.field_count > 256 ||
            !type_layout_struct_fields(sd, layout_find_struct, p, fields, &size, &alignment))
            return 0;
        for (size_t i = 0; i < sd->as.structure.field_count; i++) {
            const Decl *field = sd->as.structure.fields[i];
            if (field_is_zero_storage(field))
                continue;
            if (field->as.variable.is_bit_field) {
                size_t bytes = (fields[i].bit_offset + fields[i].bit_width + 7) / 8;
                size_t start = offset + fields[i].offset;
                if (bytes && (start >= 16 || bytes > 16 - start))
                    return 0;
                for (size_t j = 0; j < bytes; ++j)
                    classes[(start + j) / 8] = 2;
            } else if (!aggregate_fields(p, field->as.variable.type, offset + fields[i].offset,
                                         classes, depth + 1))
                return 0;
        }
        return 1;
    }
    size_t size, alignment;
    if ((!is_value_pointer(t) && !isnum(t)) ||
        !type_layout_size(t, layout_find_struct, p, &size, &alignment) ||
        !size || offset >= 16 || size > 16 - offset)
        return 0;
    unsigned kind = t->kind == TYPE_REAL || t->kind == TYPE_FLOAT ? 1 : 2;
    for (size_t i = offset / 8; i <= (offset + size - 1) / 8; i++)
        if (classes[i] < kind)
            classes[i] = kind;
    return 1;
}

/**
 * @brief Detects fields requiring memory-class System V aggregate passing.
 * @param p Program containing structure declarations.
 * @param t Type being inspected.
 * @param offset Byte offset relative to the containing aggregate.
 * @param depth Current nesting depth.
 * @return One if unaligned, zero if aligned, or minus one for an invalid layout.
 */
static int aggregate_unaligned(const AstProgram *p, const Type *t, size_t offset,
                               unsigned depth) {
    size_t size, alignment;
    if (!t || depth > 24 ||
        !type_layout_size(t, layout_find_struct, p, &size, &alignment) || !alignment)
        return -1;
    if (offset % alignment)
        return 1;
    if (t->kind == TYPE_ARRAY) {
        size_t element_size, element_alignment;
        if (!type_layout_size(t->pointee, layout_find_struct, p,
                              &element_size, &element_alignment))
            return -1;
        for (size_t i = 0; i < t->array_length; ++i) {
            int result = aggregate_unaligned(p, t->pointee, offset + i * element_size, depth + 1);
            if (result) return result;
        }
    } else if (t->kind == TYPE_STRUCT) {
        const Decl *d = find_struct(p, t->struct_name);
        FieldLayout fields[256];
        if (!d || d->as.structure.field_count > 256 ||
            !type_layout_struct_fields(d, layout_find_struct, p, fields, &size, &alignment))
            return -1;
        for (size_t i = 0; i < d->as.structure.field_count; ++i) {
            const Decl *field = d->as.structure.fields[i];
            if (field_is_zero_storage(field) || field->as.variable.is_bit_field)
                continue;
            int result = aggregate_unaligned(p, field->as.variable.type,
                                             offset + fields[i].offset, depth + 1);
            if (result) return result;
        }
    }
    return 0;
}

/**
 * @brief Reserves a padded result buffer and emits its address.
 * @param e IR emitter.
 * @param size Exact aggregate size.
 * @return Nonzero on success.
 */
static int emit_aggregate_result_buffer(Emitter *e, size_t size) {
    if (!size || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "unsupported aggregate result or too many locals");
        return 0;
    }
    unsigned slot = e->next_slot++;
    size_t padded = (size + 7) / 8 * 8;
    fprintf(e->out, "localblock __aggregate_result_%u %zu %d\n"
                    "localalias __aggregate_result_address_%u __aggregate_result_%u 0 byte\n"
                    "laddr __aggregate_result_address_%u\n",
            slot, padded,
            (e->program->contains_long_double || e->program->contains_alignas) ? 16 : 8,
            slot, slot, slot);
    return 1;
}

/**
 * @brief Determines the supported by-value size of a structure parameter.
 * @param p Program containing structure declarations.
 * @param t Structure type.
 * @param classes Output eightbyte classifications.
 * @return Size in bytes, or zero for an unsupported layout.
 */
static size_t aggregate_parameter_class(const AstProgram *p, const Type *t, unsigned *classes) {
    size_t size, alignment;
    classes[0] = classes[1] = 0;
    if (!t || t->kind != TYPE_STRUCT ||
        !type_layout_size(t, layout_find_struct, p, &size, &alignment) || !size ||
        size > 999992)
        return 0;
    if (size <= 16) {
        int unaligned = aggregate_unaligned(p, t, 0, 0);
        if (unaligned < 0)
            return 0;
        if (unaligned) {
            classes[0] = alignment == 16 ? 4 : 5;
            return size;
        }
    }
    if (alignment == 16) {
        if (size == 16 && aggregate_extended_fields(p, t, 0, classes, 0)) {
            if (classes[0] == 3 && classes[1] == 4) {
                classes[0] = 3;
                return size;
            }
            if (classes[0] <= 2 && classes[1] <= 2)
                return size;
        }
        classes[0] = 4;
        classes[1] = 0;
        return size;
    }
    if (alignment > 8)
        return 0;
    if (size > 16)
        return size;
    if (!aggregate_fields(p, t, 0, classes, 0))
        return 0;
    return size;
}

/**
 * @brief Returns the size of a supported aggregate parameter.
 * @param p Program containing structure declarations.
 * @param t Aggregate type.
 * @return Size in bytes, or zero when unsupported.
 */
static size_t aggregate_parameter_size(const AstProgram *p, const Type *t) {
    unsigned classes[2];
    return aggregate_parameter_class(p, t, classes);
}

/**
 * @brief Identifies a 16-byte-aligned aggregate with one classified eightbyte.
 * @param p Program containing the aggregate declaration.
 * @param t Aggregate type.
 * @param size Aggregate size.
 * @param classes Aggregate eightbyte classes.
 * @return Nonzero if only the first eightbyte is classified.
 */
static int aggregate_one_aligned_word(const AstProgram *p, const Type *t,
                                      size_t size, const unsigned *classes) {
    return size == 16 && classes[0] && classes[0] <= 2 && !classes[1] &&
           aggregate_aligned_16(p, t);
}

/**
 * @brief Writes an IR parameter type, including supported aggregates.
 * @param out Destination stream.
 * @param p Program containing structure declarations.
 * @param t Parameter type.
 * @return No value.
 */
static void emit_parameter_type(FILE *out, const AstProgram *p, const Type *t) {
    if (t && t->kind == TYPE_LONG_DOUBLE) {
        fputs("ext", out);
    } else if (t && t->kind == TYPE_STRUCT) {
        unsigned classes[2];
        size_t size = aggregate_parameter_class(p, t, classes);
        if (classes[0] == 3) {
            fputs("ext", out);
            return;
        }
        if (classes[0] == 4) {
            fprintf(out, "mem%zua", size);
            return;
        }
        if (classes[0] == 5) {
            fprintf(out, "mem%zu", size);
            return;
        }
        if (aggregate_one_aligned_word(p, t, size, classes)) {
            fputs(classes[0] == 1 ? "agg16a1s" : "agg16a1i", out);
            return;
        }
        if (aggregate_aligned_16(p, t) && size == 16 &&
            classes[0] && classes[0] <= 2 && classes[1] && classes[1] <= 2) {
            if (classes[0] == 2 && classes[1] == 2)
                fputs("agg16a", out);
            else
                fprintf(out, "agg16a%c%c", classes[0] == 1 ? 's' : 'i',
                        classes[1] == 1 ? 's' : 'i');
            return;
        }
        fprintf(out, size > 16 ? "mem%zu" : "agg%zu", size);
        if (size <= 16 && (classes[0] == 1 || classes[1] == 1)) {
            fputc(classes[0] == 1 ? 's' : 'i', out);
            if (size > 8)
                fputc(classes[1] == 1 ? 's' : 'i', out);
        }
    } else
        fputs(tn(t), out);
}
/**
 * @brief Checks whether an array element type is supported.
 * @param t Crunch type node being inspected or emitted.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int is_supported_array(const Type *t) {
    return t && t->kind == TYPE_POINTER && t->pointee &&
           (isnum(t->pointee) || t->pointee->kind == TYPE_LONG_DOUBLE ||
            t->pointee->kind == TYPE_STRUCT ||
            t->pointee->kind == TYPE_ARRAY ||
            is_value_pointer(t->pointee));
}
/**
 * @brief Computes the byte stride of an array element.
 * @param p AST program being searched, analyzed, emitted, or destroyed.
 * @param t Crunch type being checked, copied, or emitted.
 * @param size Output parameter receiving the size result.
 * @return One element’s byte size, or zero when the type/layout is unsupported.
 */
static int array_element_size(const AstProgram *p, const Type *t, size_t *size) {
    if (!is_supported_array(t) || !size)
        return 0;
    const Type *element = t->pointee;
    if (element->kind == TYPE_LONG_DOUBLE) {
        *size = 16;
        return 1;
    }

    if (element->kind == TYPE_ARRAY) {
        size_t alignment;
        return type_layout_size(element, layout_find_struct, p, size, &alignment);
    }
    if (element->kind == TYPE_BYTE || element->kind == TYPE_BOOL ||
        element->kind == TYPE_SIGNED_CHAR ||
        element->kind == TYPE_UNSIGNED_CHAR) {
        *size = 1;
        return 1;
    }
    if (element->kind == TYPE_SHORT || element->kind == TYPE_UNSIGNED_SHORT) {
        *size = 2;
        return 1;
    }
    if (element->kind == TYPE_INT || element->kind == TYPE_ENUM || element->kind == TYPE_UNSIGNED_INT ||
        element->kind == TYPE_FLOAT) {
        *size = 4;
        return 1;
    }
    if (element->kind == TYPE_LONG || element->kind == TYPE_UNSIGNED_LONG ||
        element->kind == TYPE_LONG_LONG || element->kind == TYPE_UNSIGNED_LONG_LONG ||
        element->kind == TYPE_REAL ||
        element->kind == TYPE_POINTER) {
        *size = 8;
        return 1;
    }
    Decl *structure = find_struct(p, element->struct_name);
    size_t offsets[256], alignment;
    return structure && structure->as.structure.field_count <= 256 &&
           struct_layout(p, structure, offsets, size, &alignment);
}

/**
 * @brief Finds the saved byte size of an active VLA dimension.
 * @param e IR emitter.
 * @param type Array type whose size is needed.
 * @param slot Receives the local slot containing its byte size.
 * @return One if the type belongs to an active VLA, otherwise zero.
 */
static int vla_size_slot(Emitter *e, const Type *type, unsigned *slot) {
    if (!type || type->kind != TYPE_ARRAY)
        return 0;
    for (size_t i = e->count; i > 0; --i) {
        const Local *local = &e->locals[i - 1];
        const Type *start = local->type;
        int pointer = start && start->kind == TYPE_POINTER && start->pointee &&
                      start->pointee->kind == TYPE_ARRAY;
        if (local->alias != LOCAL_VLA && !pointer)
            continue;
        if (pointer)
            start = start->pointee;
        unsigned dimension = 0;
        for (const Type *part = start; part && part->kind == TYPE_ARRAY;
             part = part->pointee, ++dimension) {
            if (part == type || (part->array_bound && part->array_bound == type->array_bound)) {
                if (pointer) {
                    if (!part->array_bound || !part->array_bound->variable) {
                        const Type *inner = part;
                        while (inner && inner->kind == TYPE_ARRAY &&
                               (!inner->array_bound || !inner->array_bound->variable))
                            inner = inner->pointee;
                        if (!inner || inner->kind != TYPE_ARRAY)
                            return 0;
                    }
                    *slot = e->vla_parameter_sizes[local->slot]
                                ? e->vla_parameter_sizes[local->slot] - 1 + dimension
                                : local->slot + dimension + 1;
                } else
                    *slot = dimension ? local->slot + dimension + 2 : local->slot + 1;
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief Emits pointer arithmetic with an address and element offset on the stack.
 * @param e IR emitter state.
 * @param pointer Pointer type whose pointee determines the stride.
 * @param subtract Nonzero to subtract the offset.
 * @return One on success, or zero with a diagnostic for an unsupported layout.
 */
static int emit_pointer_offset(Emitter *e, const Type *pointer, int subtract) {
    if (!is_supported_array(pointer)) {
        fail(e, "pointer arithmetic requires a numeric, structure, or pointer element type");
        return 0;
    }
    if (pointer->pointee->kind != TYPE_STRUCT && pointer->pointee->kind != TYPE_ARRAY &&
        pointer->pointee->kind != TYPE_LONG_DOUBLE) {
        fprintf(e->out, "ptr%s %s\n", subtract ? "sub" : "add", tn(pointer->pointee));
        return 1;
    }
    unsigned size_slot;
    if (vla_size_slot(e, pointer->pointee, &size_slot)) {
        if (subtract)
            fputs("neg long\n", e->out);
        fprintf(e->out, "load %u\nmul long\nptradd byte\n", size_slot);
        return 1;
    }
    size_t stride;
    if (!array_element_size(e->program, pointer, &stride) || !stride || stride > INT64_MAX) {
        fail(e, "pointer arithmetic requires a supported complete structure layout");
        return 0;
    }
    if (subtract)
        fputs("neg long\n", e->out);
    if (stride <= INT32_MAX)
        fprintf(e->out, "ptradd_bytes %zu\n", stride);
    else
        fprintf(e->out, "push %zu\nmul long\nptradd byte\n", stride);
    return 1;
}
/**
 * @brief Finds the byte offset and declaration of a structure field.
 * @param p AST program being searched, analyzed, emitted, or destroyed.
 * @param t Crunch type being checked, copied, or emitted.
 * @param name Identifier or name to find, compare, or emit.
 * @param offset Output parameter receiving the offset result.
 * @param field Structure field declaration or field name.
 * @param depth Current nesting depth.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int struct_field_offset_depth(const AstProgram *p, const Type *t, const char *name,
                                     size_t *offset, Decl **field, unsigned depth) {
    if (!t || t->kind != TYPE_STRUCT || !t->struct_name || depth > 24)
        return 0;
    Decl *structure = find_struct(p, t->struct_name);
    size_t offsets[256], size, alignment;
    if (!structure || structure->as.structure.field_count > 256 ||
        !struct_layout(p, structure, offsets, &size, &alignment))
        return 0;
    for (size_t i = 0; i < structure->as.structure.field_count; i++) {
        Decl *member = structure->as.structure.fields[i];
        if (member->as.variable.anonymous_member) {
            size_t nested_offset;
            if (struct_field_offset_depth(p, member->as.variable.type, name, &nested_offset,
                                          field, depth + 1)) {
                *offset = offsets[i] + nested_offset;
                return 1;
            }
        } else if (member->as.variable.name && !strcmp(member->as.variable.name, name)) {
            *offset = offsets[i];
            if (field)
                *field = member;
            return 1;
        }
    }
    return 0;
}
/**
 * @brief Computes the offset of a field, including promoted fields.
 * @param p AST program containing structure declarations.
 * @param t Structure type.
 * @param name Field name.
 * @param offset Receives the byte offset.
 * @param field Optionally receives the field declaration.
 * @return One if the field exists and its layout is supported.
 */
static int struct_field_offset(const AstProgram *p, const Type *t, const char *name, size_t *offset,
                               Decl **field) {
    return struct_field_offset_depth(p, t, name, offset, field, 0);
}

/**
 * @brief Resolves a field's byte and bit position through anonymous members.
 * @param p Program containing structure declarations.
 * @param t Structure type.
 * @param name Field name.
 * @param result Receives the field layout.
 * @param field Receives the field declaration.
 * @param depth Current nesting depth.
 * @return Nonzero if the field is found.
 */
static int bit_field_layout_depth(const AstProgram *p, const Type *t, const char *name,
                                  FieldLayout *result, const Decl **field, unsigned depth) {
    if (!t || t->kind != TYPE_STRUCT || !t->struct_name || depth > 24)
        return 0;
    const Decl *structure = find_struct(p, t->struct_name);
    FieldLayout fields[256];
    size_t size, alignment;
    if (!structure || structure->as.structure.field_count > 256 ||
        !type_layout_struct_fields(structure, layout_find_struct, p, fields, &size, &alignment))
        return 0;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        const Decl *member = structure->as.structure.fields[i];
        if (member->as.variable.anonymous_member) {
            if (bit_field_layout_depth(p, member->as.variable.type, name, result, field,
                                       depth + 1)) {
                result->offset += fields[i].offset;
                return 1;
            }
        } else if (member->as.variable.name && !strcmp(member->as.variable.name, name)) {
            *result = fields[i];
            *field = member;
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Finds the layout of a bit-field member expression.
 * @param e IR emitter.
 * @param x Member expression.
 * @param layout Receives its position.
 * @param field Receives its declaration.
 * @return Nonzero if the member is a bit-field.
 */
static int bit_field_info(Emitter *e, const Expr *x, FieldLayout *layout, const Decl **field) {
    return x && x->kind == EXPR_MEMBER &&
           bit_field_layout_depth(e->program, member_base_type(e, x), x->as.member.name,
                                  layout, field, 0) && (*field)->as.variable.is_bit_field;
}

/**
 * @brief Emits the address of the first byte occupied by a bit-field.
 * @param e IR emitter.
 * @param x Member expression.
 * @param layout Field position.
 * @return Nonzero on success.
 */
static int emit_bit_field_address(Emitter *e, const Expr *x, const FieldLayout *layout) {
    if (!(x->as.member.through_pointer ? expression(e, x->as.member.base)
                                      : emit_aggregate_address(e, x->as.member.base, 0)))
        return 0;
    if (layout->offset)
        fprintf(e->out, "push %zu\nptradd byte\n", layout->offset);
    return 1;
}

/**
 * @brief Identifies a flexible array member expression.
 * @param e IR emitter.
 * @param x Member expression.
 * @return Nonzero when the selected field is a flexible array.
 */
static int is_flexible_member(Emitter *e, const Expr *x) {
    const Type *base = member_base_type(e, x);
    Decl *field = base && x ? struct_field(e->program, base, x->as.member.name) : NULL;
    return field_is_zero_storage(field);
}

/**
 * @brief Emits the address of a flexible array member.
 * @param e IR emitter.
 * @param x Flexible array member expression.
 * @return Nonzero on success.
 */
static int emit_flexible_member_address(Emitter *e, const Expr *x) {
    size_t offset;
    if (!struct_field_offset(e->program, member_base_type(e, x), x->as.member.name,
                             &offset, NULL) ||
        !(x->as.member.through_pointer ? expression(e, x->as.member.base)
                                       : emit_aggregate_address(e, x->as.member.base, 0)))
        return 0;
    if (offset)
        fprintf(e->out, "push %zu\nptradd byte\n", offset);
    return 1;
}
/**
 * @brief Checks whether a structure expression uses indirect storage.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int is_indirect_structure(Emitter *e, const Expr *x) {
    if (!x)
        return 0;
    if (x->kind == EXPR_NAME) {
        unsigned slot;
        const Type *local_type;
        if (lookup(e, x->as.name.name, &slot, &local_type))
            return 0;
        Decl *global = find_global(e->program, x->as.name.name);
        return global && ((global->is_extern && !global->semantic_global_definition) ||
                          local_alias(e, x->as.name.name) == LOCAL_EXTERN) &&
               global->as.variable.type &&
               global->as.variable.type->kind == TYPE_STRUCT;
    }
    if (x->kind == EXPR_CAST && x->as.cast.target_type &&
        x->as.cast.target_type->kind == TYPE_STRUCT && x->as.cast.operand &&
        x->as.cast.operand->kind == EXPR_ARRAY_INIT)
        return 1;
    if (x->kind == EXPR_BINARY && x->as.binary.op == BINARY_COMMA &&
        expr_type(e, x) && expr_type(e, x)->kind == TYPE_STRUCT)
        return 1;
    if (x->kind == EXPR_CONDITIONAL && expr_type(e, x) &&
        expr_type(e, x)->kind == TYPE_STRUCT)
        return 1;
    if (x->kind == EXPR_CALL && expr_type(e, x) &&
        expr_type(e, x)->kind == TYPE_STRUCT)
        return 1;
    if (x->kind == EXPR_ASSIGN && expr_type(e, x) && expr_type(e, x)->kind == TYPE_STRUCT &&
        (requires_object_copy(e->program, expr_type(e, x), 0) ||
         aggregate_aligned_16(e->program, expr_type(e, x))))
        return 1;
    if (x->kind == EXPR_UNARY && x->as.unary.op == UNARY_DEREFERENCE) {
        const Type *type = expr_type(e, x);
        return type && type->kind == TYPE_STRUCT;
    }
    if (x->kind == EXPR_INDEX) {
        const Type *base = expr_type(e, x->as.index.base);
        return base && base->kind == TYPE_POINTER && base->pointee &&
               base->pointee->kind == TYPE_STRUCT;
    }
    return x->kind == EXPR_MEMBER && expr_type(e, x) && expr_type(e, x)->kind == TYPE_STRUCT &&
           (x->as.member.through_pointer || is_indirect_structure(e, x->as.member.base));
}
/**
 * @brief Checks whether a member expression accesses indirect structure storage.
 * @param e IR emitter state.
 * @param x Member expression.
 * @return Nonzero for indirect member access.
 */
static int is_indirect_member(Emitter *e, const Expr *x) {
    return x && x->kind == EXPR_MEMBER &&
           (x->as.member.through_pointer || is_indirect_structure(e, x->as.member.base));
}
/**
 * @brief Emits the address of an indexed structure element or indirect member.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int emit_indirect_address(Emitter *e, const Expr *x) {
    if (!x)
        return 0;
    if (x->kind == EXPR_NAME && is_indirect_structure(e, x)) {
        fprintf(e->out, "gaddr %s\n", x->as.name.name);
        return 1;
    }
    if (x->kind == EXPR_CAST && is_indirect_structure(e, x))
        return emit_compound_literal(e, x);
    if (x->kind == EXPR_BINARY && x->as.binary.op == BINARY_COMMA &&
        is_indirect_structure(e, x))
        return emit_aggregate_address(e, x, 0);
    if ((x->kind == EXPR_CALL || x->kind == EXPR_CONDITIONAL || x->kind == EXPR_ASSIGN) &&
        is_indirect_structure(e, x))
        return emit_aggregate_address(e, x, 0);
    if (x->kind == EXPR_UNARY && x->as.unary.op == UNARY_DEREFERENCE &&
        is_indirect_structure(e, x))
        return expression(e, x->as.unary.operand);
    if (x->kind == EXPR_INDEX) {
        const Type *base = expr_type(e, x->as.index.base);
        size_t stride;
        if (!base || base->kind != TYPE_POINTER || !base->pointee ||
            base->pointee->kind != TYPE_STRUCT || !array_element_size(e->program, base, &stride))
            return 0;
        if (!expression(e, x->as.index.base) || !expression(e, x->as.index.index))
            return 0;
        fprintf(e->out, "ptradd_bytes %zu\n", stride);
        return 1;
    }
    if (is_indirect_member(e, x)) {
        size_t offset;
        if (!struct_field_offset(e->program, member_base_type(e, x), x->as.member.name, &offset,
                                 NULL) ||
            !(x->as.member.through_pointer ? expression(e, x->as.member.base)
                                           : emit_indirect_address(e, x->as.member.base)))
            return 0;
        if (offset)
            fprintf(e->out, "push %zu\nptradd byte\n", offset);
        return 1;
    }
    return 0;
}
/**
 * @brief Emits the address of a field supporting pointer arithmetic.
 * @param e IR emitter state.
 * @param x Field expression to address.
 * @return One on success, or zero with a diagnostic.
 */
static int emit_pointer_field_address(Emitter *e, const Expr *x) {
    const Type *base = member_base_type(e, x);
    Decl *field = struct_field(e->program, base, x->as.member.name);
    if (!field || !is_supported_array(field->as.variable.type)) {
        fail(e, "pointer arithmetic requires a supported pointer field");
        return 0;
    }
    if (is_indirect_member(e, x)) {
        if (!emit_indirect_address(e, x)) {
            fail(e, "cannot address indirect structure pointer field");
            return 0;
        }
        return 1;
    }
    char key[512], gkey[512], root[512];
    unsigned slot;
    const Type *type;
    if (!field_storage_key(x, key, sizeof key, 0) ||
        !field_storage_key(x, gkey, sizeof gkey, 1) ||
        !member_path(x, root, sizeof root, 1)) {
        fail(e, "cannot address pointer field");
        return 0;
    }
    if (lookup(e, key, &slot, &type)) {
        emit_local_addr(e, key, slot);
        return 1;
    }
    char *sep = strstr(root, "__");
    if (sep) {
        *sep = '\0';
        Decl *global = find_global(e->program, root);
        if (global && global->as.variable.type &&
            global->as.variable.type->kind == TYPE_STRUCT) {
            fprintf(e->out, "gaddr %s\n", gkey);
            return 1;
        }
    }
    fail(e, "pointer field storage not found");
    return 0;
}
/**
 * @brief Emits the address of an indexed numeric or pointer element.
 * @param e IR emitter state.
 * @param x Indexed expression.
 * @return One on success; zero on failure.
 */
static int emit_index_address(Emitter *e, const Expr *x) {
    const Type *base = expr_type(e, x->as.index.base);
    if (!is_supported_array(base))
        return 0;
    if (!expression(e, x->as.index.base) || !expression(e, x->as.index.index))
        return 0;
    return emit_pointer_offset(e, base, 0);
}
/**
 * @brief Emits the address of a pointer-valued member, dereference, or index.
 * @param e IR emitter state.
 * @param x Pointer lvalue to address.
 * @return One on success, or zero with a diagnostic.
 */
static int emit_pointer_lvalue_address(Emitter *e, const Expr *x) {
    if (!x || !is_supported_array(expr_type(e, x))) {
        fail(e, "pointer arithmetic requires a supported pointer lvalue");
        return 0;
    }
    if (x->kind == EXPR_MEMBER)
        return emit_pointer_field_address(e, x);
    if (x->kind == EXPR_UNARY && x->as.unary.op == UNARY_DEREFERENCE)
        return expression(e, x->as.unary.operand);
    if (x->kind == EXPR_INDEX && emit_index_address(e, x))
        return 1;
    if (x->kind == EXPR_CAST && x->as.cast.operand &&
        x->as.cast.operand->kind == EXPR_ARRAY_INIT)
        return emit_compound_literal(e, x);
    fail(e, "cannot address pointer lvalue");
    return 0;
}
/**
 * @brief Allocates and returns a copy of a NUL-terminated string.
 * @param s Text spelling to inspect, parse, or serialize.
 * @return Allocated string copy, or NULL if allocation fails.
 */
static char *dup_text(const char *s) {
    size_t n = strlen(s) + 1;
    char *r = malloc(n);
    if (r)
        memcpy(r, s, n);
    return r;
}
/**
 * @brief Emits flattened global aliases for all structure fields.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param sd Declaration node being checked, serialized, or emitted.
 * @param block Text or name input to this operation.
 * @param path Text or name input to this operation.
 * @param baseoff Count, capacity, size, or byte offset used by this operation.
 * @param depth Current recursive nesting depth.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int emit_global_aliases(Emitter *e, const Decl *sd, const char *block, const char *path,
                               size_t baseoff, unsigned depth) {
    if (depth > 24)
        return 0;
    size_t offsets[256], sz, al;
    if (!sd || sd->as.structure.field_count > 256 ||
        !struct_layout(e->program, sd, offsets, &sz, &al))
        return 0;
    for (size_t i = 0; i < sd->as.structure.field_count; i++) {
        Decl *f = sd->as.structure.fields[i];
        if (field_is_zero_storage(f) || f->as.variable.is_bit_field)
            continue;
        char fieldpath[512];
        if (snprintf(fieldpath, sizeof fieldpath, f->as.variable.anonymous_member ? "%s%s" : "%s__%s",
                     path, f->as.variable.anonymous_member ? "" : f->as.variable.name) >=
            (int)sizeof fieldpath)
            return 0;
        const Type *t = f->as.variable.type;
        if (t && t->kind == TYPE_STRUCT) {
            Decl *nested = find_struct(e->program, t->struct_name);
            if (!emit_global_aliases(e, nested, block, fieldpath, baseoff + offsets[i], depth + 1))
                return 0;
            continue;
        }
        if (t && (t->kind == TYPE_ARRAY || t->kind == TYPE_LONG_DOUBLE))
            fprintf(e->out, "globalalias %s %s %zu byte\n", fieldpath, block,
                    baseoff + offsets[i]);
        else
            fprintf(e->out, "globalalias %s %s %zu %s\n", fieldpath, block, baseoff + offsets[i],
                    tn(t));
    }
    return 1;
}
static unsigned label(Emitter *);
static void mark(Emitter *, unsigned);
static unsigned static_local_symbol(Emitter *, char *, size_t);

/**
 * @brief Registers local or static field aliases.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param base Text or name input to this operation.
 * @param block Text or name input to this operation.
 * @param sd Declaration node being checked, serialized, or emitted.
 * @param baseoff Count, capacity, size, or byte offset used by this operation.
 * @param depth Current recursive nesting depth.
 * @param static_storage Nonzero for a static local structure.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int register_local_fields(Emitter *e, const char *base, const char *block, const Decl *sd,
                                 size_t baseoff, unsigned depth, int static_storage) {
    if (depth > 24 || !sd)
        return 0;
    size_t offsets[256], sz, al;
    if (sd->as.structure.field_count > 256 || !struct_layout(e->program, sd, offsets, &sz, &al))
        return 0;
    for (size_t i = 0; i < sd->as.structure.field_count; i++) {
        Decl *f = sd->as.structure.fields[i];
        if (field_is_zero_storage(f) || f->as.variable.is_bit_field)
            continue;
        char key[512];
        if (snprintf(key, sizeof key, f->as.variable.anonymous_member ? "%s%s" : "%s.%s",
                     base, f->as.variable.anonymous_member ? "" : f->as.variable.name) >=
            (int)sizeof key)
            return 0;
        const Type *t = f->as.variable.type;
        size_t off = baseoff + offsets[i];
        if (t && t->kind == TYPE_STRUCT) {
            Decl *nested = find_struct(e->program, t->struct_name);
            if (!register_local_fields(e, key, block, nested, off, depth + 1, static_storage))
                return 0;
            continue;
        }
        if (e->count >= CRUNCH_LOCAL_LIMIT)
            return 0;
        char *alias = dup_text(key);
        if (!alias) {
            fail_oom(e);
            return 0;
        }
        if (!track_local_name(e, alias)) {
            free(alias);
            return 0;
        }
        if (static_storage) {
            char name[64];
            unsigned field_id = static_local_symbol(e, name, sizeof name);
            e->locals[e->count++] = (Local){alias, field_id, t, LOCAL_STATIC_SCALAR};
            fprintf(e->out, "globalalias %s %s %zu %s\n", name, block, off,
                    t && (t->kind == TYPE_ARRAY || t->kind == TYPE_LONG_DOUBLE) ? "byte" : tn(t));
        } else {
            unsigned field_id = e->next_field_alias++;
            e->locals[e->count++] = (Local){alias, field_id, t, LOCAL_FIELD_ALIAS};
            fprintf(e->out, "localalias __local_field_%u %s %zu %s\n", field_id, block, off,
                    t && (t->kind == TYPE_ARRAY || t->kind == TYPE_LONG_DOUBLE) ? "byte" : tn(t));
            if (is_value_pointer(t))
                fprintf(e->out, "push 0\nlstore __local_field_%u ptr\n", field_id);
        }
    }
    return 1;
}
/**
 * @brief Infers an expression type in the current emitter scope.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @return Inferred expression type, or NULL when it cannot be determined.
 */
static const Type *expr_type(Emitter *, const Expr *);
static const Type *expr_type_raw(Emitter *, const Expr *);
/**
 * @brief Recursively infers an expression type through the guarded wrapper.
 * @param e IR emitter state containing program and diagnostic context.
 * @param x Expression node being analyzed.
 * @return Inferred type, or NULL when it cannot be determined.
 */
static const Type *expr_type_impl(Emitter *e, const Expr *x) {
    unsigned s;
    const Type *t = NULL;
    if (!x)
        return NULL;
    switch (x->kind) {
    case EXPR_OFFSETOF:
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        return &TYPE_ULONG_VALUE;
    case EXPR_STRING:
        return pointer_to(e, x->string_literal_wide ? &TYPE_INT_VALUE : &TYPE_BYTE_VALUE);
    case EXPR_INTEGER:
        if ((x->integer_suffix != INTEGER_SUFFIX_NONE && x->integer_suffix != INTEGER_SUFFIX_L &&
             x->integer_suffix != INTEGER_SUFFIX_LL) || x->integer_literal_unsigned) {
            if (x->integer_suffix == INTEGER_SUFFIX_ULL || x->integer_suffix == INTEGER_SUFFIX_LL)
                return &TYPE_ULLONG_VALUE;
            return x->integer_suffix == INTEGER_SUFFIX_UL ||
                           x->integer_suffix == INTEGER_SUFFIX_L ||
                           x->unsigned_integer_value > UINT32_MAX
                       ? &TYPE_ULONG_VALUE : &TYPE_UINT_VALUE;
        }
        if (x->integer_suffix == INTEGER_SUFFIX_LL)
            return &TYPE_LLONG_VALUE;
        if (x->integer_suffix == INTEGER_SUFFIX_L)
            return &TYPE_LONG_VALUE;
        return x->integer_literal_out_of_range ? &TYPE_LONG_VALUE
               : (x->as.integer_value < -2147483648LL || x->as.integer_value > 2147483647LL)
                   ? &TYPE_LONG_VALUE
                   : &TYPE_INT_VALUE;
    case EXPR_REAL:
        return x->real_literal_extended_text ? &TYPE_LONG_DOUBLE_VALUE :
               x->real_literal_float ? &TYPE_FLOAT_VALUE : &TYPE_REAL_VALUE;
    case EXPR_CHAR:
        return x->char_literal_wide || e->program->c11_numeric ? &TYPE_INT_VALUE : &TYPE_BYTE_VALUE;
    case EXPR_NAME:
        if (x->semantic_enum_constant)
            return &TYPE_INT_VALUE;
        if (x->semantic_function_name)
            return &e->function_name_type;
        if (lookup(e, x->as.name.name, &s, &t))
            return t;
        {
            Decl *g = find_global(e->program, x->as.name.name);
            if (g)
                return g->as.variable.type;
            Decl *fn = find_function(e->program, x->as.name.name);
            return fn ? function_type(e, fn) : NULL;
        }
    case EXPR_MEMBER:
        return member_type(e, x);
    case EXPR_CALL:
        if (x->semantic_builtin_alloca)
            return pointer_to(e, &TYPE_VOID_VALUE);
        if (x->as.call.callee && x->as.call.callee->kind == EXPR_NAME) {
            const char *n = x->as.call.callee->as.name.name;
            Decl *d = find_function(e->program, n);
            if (lookup(e, n, &s, &t) && t && t->kind == TYPE_POINTER && t->pointee &&
                t->pointee->kind == TYPE_FUNCTION && t->pointee->function_signature)
                return t->pointee->function_signature->as.function.return_type;
            if (!d && (!strcmp(n, "__crunch_va_start") || !strcmp(n, "__crunch_va_copy") ||
                       !strcmp(n, "__crunch_va_end") || !strcmp(n, "__builtin_va_start") ||
                       !strcmp(n, "__builtin_va_copy") || !strcmp(n, "__builtin_va_end")))
                return &TYPE_VOID_VALUE;
            if (!d && (!strcmp(n, "__crunch_va_arg") || !strcmp(n, "__builtin_va_arg")) &&
                x->as.call.arguments.count == 2)
                return expr_type(e, x->as.call.arguments.items[1]);
            if (d)
                return d->as.function.return_type;
            if (!strcmp(n, "copy") || !strcmp(n, "fill")) {
                if (!x->as.call.arguments.count)
                    return NULL;
                if (is_null_literal(x->as.call.arguments.items[0]))
                    return pointer_to(e, &TYPE_BYTE_VALUE);
                return expr_type(e, x->as.call.arguments.items[0]);
            }
            if (!strcmp(n, "strcmp") || !strcmp(n, "compare"))
                return &TYPE_INT_VALUE;
        }
        {
            const Type *callee = expr_type(e, x->as.call.callee);
            if (callee && callee->kind == TYPE_POINTER && callee->pointee &&
                callee->pointee->kind == TYPE_FUNCTION && callee->pointee->function_signature)
                return callee->pointee->function_signature->as.function.return_type;
        }
        return NULL;
    case EXPR_CAST:
        return x->as.cast.target_type;
    case EXPR_UNARY:
        if (x->as.unary.op == UNARY_LOGICAL_NOT)
            return &TYPE_INT_VALUE;
        if (x->as.unary.op == UNARY_ADDRESS)
            return pointer_to(e, expr_type_raw(e, x->as.unary.operand));
        if (x->as.unary.op == UNARY_DEREFERENCE) {
            const Type *q = expr_type(e, x->as.unary.operand);
            return q && q->kind == TYPE_POINTER ? q->pointee : NULL;
        }
        t = expr_type(e, x->as.unary.operand);
        if (t && t->kind == TYPE_ENUM &&
            (x->as.unary.op == UNARY_PLUS || x->as.unary.op == UNARY_MINUS ||
             x->as.unary.op == UNARY_BITWISE_NOT))
            return type_effective_kind(t) == TYPE_UNSIGNED_INT ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE;
        if (t && (promoted_small_kind(t->kind) ||
                  (e->program->c11_numeric && t->kind == TYPE_BYTE)) &&
            (x->as.unary.op == UNARY_PLUS || x->as.unary.op == UNARY_MINUS ||
             x->as.unary.op == UNARY_BITWISE_NOT))
            return &TYPE_INT_VALUE;
        return t;
    case EXPR_INDEX: {
        const Type *q = expr_type(e, x->as.index.base);
        return q && q->kind == TYPE_POINTER ? q->pointee : NULL;
    }
    case EXPR_BINARY: {
        BinaryOp o = x->as.binary.op;
        const Type *l = expr_type(e, x->as.binary.left), *r = expr_type(e, x->as.binary.right);
        if (o == BINARY_COMMA)
            return r;
        if (o == BINARY_POWER)
            return &TYPE_REAL_VALUE;
        if (o >= BINARY_EQUAL && o <= BINARY_GREATER_EQUAL) {
            static const Type it = {TYPE_INT, 0, 0, false, 0, NULL, false, NULL, false, NULL, false, false};
            return &it;
        }
        if (o == BINARY_SUBTRACT && l && r &&
            l->kind == TYPE_POINTER && r->kind == TYPE_POINTER)
            return &TYPE_LONG_VALUE;
        if ((o == BINARY_ADD || o == BINARY_SUBTRACT) && l && l->kind == TYPE_POINTER)
            return l;
        if (o == BINARY_ADD && r && r->kind == TYPE_POINTER)
            return r;
        if (o == BINARY_LOGICAL_AND || o == BINARY_LOGICAL_OR)
            return &TYPE_INT_VALUE;
        if (l && r && (o == BINARY_SHIFT_LEFT || o == BINARY_SHIFT_RIGHT) &&
            (e->program->c11_numeric || l->kind == TYPE_UNSIGNED_INT || l->kind == TYPE_UNSIGNED_LONG ||
             r->kind == TYPE_UNSIGNED_INT || r->kind == TYPE_UNSIGNED_LONG ||
             promoted_small_kind(l->kind) || promoted_small_kind(r->kind)))
            return l->kind == TYPE_ENUM
                       ? (type_effective_kind(l) == TYPE_UNSIGNED_INT ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE)
                       : l->kind == TYPE_BYTE || promoted_small_kind(l->kind) ? &TYPE_INT_VALUE : l;
        return promote(l, r, e->program->c11_numeric);
    }
    case EXPR_ASSIGN:
        return expr_type(e, x->as.assign.target);
    case EXPR_CONDITIONAL: {
        const Type *l = expr_type(e, x->as.conditional.then_expr),
                   *r = expr_type(e, x->as.conditional.else_expr);
        if (l && r && l->kind == TYPE_STRUCT && r->kind == TYPE_STRUCT)
            return l;
        if (r && r->kind == TYPE_POINTER && r->pointee &&
            r->pointee->kind == TYPE_VOID)
            return r;
        if (l && l->kind == TYPE_POINTER)
            return l;
        if (r && r->kind == TYPE_POINTER)
            return r;
        return promote(l, r, e->program->c11_numeric);
    }
    default:
        return NULL;
    }
}
/**
 * @brief Infers an expression type while enforcing the IR traversal depth limit.
 * @param e IR emitter state containing diagnostic and nesting context.
 * @param x Expression node whose type is inferred.
 * @return Inferred type, or NULL when inference fails or is too deeply nested.
 */
static const Type *expr_type_raw(Emitter *e, const Expr *x) {
    if (e->type_depth >= IR_MAX_AST_RECURSION_DEPTH) {
        e->recursion_limit_exceeded = 1;
        fail(e, "maximum IR type-inference nesting depth exceeded");
        return NULL;
    }
    ++e->type_depth;
    const Type *result = expr_type_impl(e, x);
    --e->type_depth;
    return result;
}
/**
 * @brief Infers the value type of an expression, including array decay.
 * @param e IR emitter.
 * @param x Expression to inspect.
 * @return Inferred type, or NULL on failure.
 */
static const Type *expr_type(Emitter *e, const Expr *x) {
    const Type *result = expr_type_raw(e, x);
    if (result && x && result->kind == TYPE_UNSIGNED_INT && x->semantic_bit_field_width &&
        x->semantic_bit_field_width < 32 && x->semantic_type == TYPE_INT)
        return &TYPE_INT_VALUE;
    return result && result->kind == TYPE_ARRAY ? pointer_to(e, result->pointee) :
           result && result->kind == TYPE_FUNCTION ? pointer_to(e, result) : result;
}

/**
 * @brief Reserves a binary80 expression temporary and pushes its address.
 * @param e IR emitter.
 * @return Nonzero on success.
 */
static int emit_extended_temporary(Emitter *e) {
    if (e->next_extended >= 512) {
        fail(e, "too many long double temporaries");
        return 0;
    }
    unsigned id = e->next_extended++;
    fprintf(e->out, "localblock __extended_%u 16 16\n"
                    "localalias __extended_address_%u __extended_%u 0 byte\n"
                    "laddr __extended_address_%u\n", id, id, id, id);
    return 1;
}

/**
 * @brief Emits a boolean conversion for a value already on the IR stack.
 * @param e IR emitter.
 * @param type Source value type.
 * @return No value.
 */
static void emit_truth(Emitter *e, const Type *type) {
    if (type && type->kind == TYPE_LONG_DOUBLE)
        fputs("extbool\n", e->out);
    else
        fprintf(e->out, "bool %s\n", tn(type));
}

/**
 * @brief Emits a permitted numeric conversion instruction.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param from Crunch type being checked, copied, or emitted.
 * @param to Crunch type being checked, copied, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int cast_emit(Emitter *e, const Type *from, const Type *to) {
    if (from && to && from->kind == TYPE_LONG_DOUBLE && to->kind == TYPE_LONG_DOUBLE)
        return 1;
    if (from && to && from->kind == TYPE_LONG_DOUBLE) {
        if (to->kind == TYPE_BOOL) {
            fputs("extbool\ncast int byte\n", e->out);
            return 1;
        }
        if (!isnum(to)) {
            fail(e, "unsupported conversion from long double");
            return 0;
        }
        if (to->kind == TYPE_BYTE || promoted_small_kind(to->kind)) {
            fprintf(e->out, "extto long\ncast long %s\n", tn(to));
        } else {
            fprintf(e->out, "extto %s\n", tn(to));
        }
        return 1;
    }
    if (from && to && to->kind == TYPE_LONG_DOUBLE) {
        if (!isnum(from) || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
            fail(e, "unsupported conversion to long double");
            return 0;
        }
        const char *type = tn(from);
        if (from->kind == TYPE_BYTE || promoted_small_kind(from->kind)) {
            fprintf(e->out, "cast %s int\n", type);
            type = "int";
        }
        unsigned slot = e->next_slot++;
        fprintf(e->out, "local %u %s\nstore %u %s\n", slot, type, slot, type);
        if (!emit_extended_temporary(e))
            return 0;
        fprintf(e->out, "load %u\nextfrom %s\n", slot, type);
        return 1;
    }
    if (from && to && to->kind == TYPE_BOOL && from->kind != TYPE_BOOL) {
        if (!isnum(from) && from->kind != TYPE_POINTER) {
            fail(e, "unsupported conversion to _Bool");
            return 0;
        }
        fprintf(e->out, "bool %s\ncast int byte\n", tn(from));
        return 1;
    }
    if (!from || !to || (from->kind == to->kind && from->kind != TYPE_ENUM) ||
        (isnum(from) && isnum(to) && !strcmp(tn(from), tn(to))))
        return 1;
    if (!isnum(from) || !isnum(to)) {
        fail(e, "unsupported numeric conversion");
        return 0;
    }
    if (e->program->c11_numeric &&
        (from->kind == TYPE_REAL || from->kind == TYPE_FLOAT) &&
        (to->kind == TYPE_SIGNED_CHAR || to->kind == TYPE_SHORT || to->kind == TYPE_INT ||
         (to->kind == TYPE_ENUM && type_effective_kind(to) == TYPE_INT))) {
        /* Bound the original value, then truncate through long without changing IR casts. */
        const char *lower = to->kind == TYPE_SIGNED_CHAR ? "0xc060200000000000"
                            : to->kind == TYPE_SHORT ? "0xc0e0002000000000"
                            : "0xc1e0000000200000";
        const char *upper = to->kind == TYPE_SIGNED_CHAR ? "0x4060000000000000"
                            : to->kind == TYPE_SHORT ? "0x40e0000000000000"
                            : "0x41e0000000000000";
        unsigned invalid = label(e), end = label(e);
        if (from->kind == TYPE_FLOAT)
            fputs("cast float real\n", e->out);
        fprintf(e->out, "dup\npushr %s\ngt real\njz L%u\n"
                        "dup\npushr %s\nlt real\njz L%u\n"
                        "cast real long\ncast long %s\njmp L%u\n",
                lower, invalid, upper, invalid, tn(to), end);
        mark(e, invalid);
        fprintf(e->out, "drop\npush 0\ncast int %s\n", tn(to));
        mark(e, end);
    } else if (e->program->c11_numeric && from->kind == TYPE_REAL && to->kind == TYPE_BYTE) {
        unsigned invalid = label(e), end = label(e);
        fprintf(e->out, "dup\npushr 0xbff0000000000000\ngt real\njz L%u\n"
                        "dup\npushr 0x4070000000000000\nlt real\njz L%u\n"
                        "cast real int\ncast int byte\njmp L%u\n",
                invalid, invalid, end);
        mark(e, invalid);
        fputs("drop\npush 0\ncast int byte\n", e->out);
        mark(e, end);
    } else
        fprintf(e->out, "cast %s %s\n", tn(from), tn(to));
    return 1;
}
/**
 * @brief Returns the root variable name of a nested member expression.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @return Root variable name, or NULL when the expression has no name root.
 */
static const char *expr_root_name(const Expr *x) {
    while (x && x->kind == EXPR_MEMBER)
        x = x->as.member.base;
    return x && x->kind == EXPR_NAME ? x->as.name.name : NULL;
}
/**
 * @brief Checks whether an expression is rooted in a local variable.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int expr_is_local(Emitter *e, const Expr *x) {
    const char *n = expr_root_name(x);
    unsigned slot;
    const Type *t;
    return n && lookup(e, n, &slot, &t);
}
/**
 * @brief Appends a field name to a flattened structure path.
 * @param out Output FILE stream.
 * @param cap Capacity of the output buffer.
 * @param base Text or name input to this operation.
 * @param field Structure field declaration or field name.
 * @param local int value used by this operation.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int path_append(char *out, size_t cap, const char *base, const char *field, int local) {
    int r = snprintf(out, cap, local ? "%s.%s" : "%s__%s", base, field);
    return r >= 0 && (size_t)r < cap;
}
/**
 * @brief Copies a fixed array whose destination and source addresses are on the stack.
 * @param e IR emitter.
 * @param type Fixed-array type to copy.
 * @return One on success, or zero with a diagnostic.
 */
static int emit_fixed_array_copy(Emitter *e, const Type *type) {
    size_t size, alignment;
    if (!type_layout_size(type, layout_find_struct, e->program, &size, &alignment) ||
        !size || size > INT64_MAX) {
        fail(e, "unsupported fixed array field layout");
        return 0;
    }
    if (!e->fixed_array_copy_slot) {
        if (e->next_slot > CRUNCH_LOCAL_LIMIT - 3) {
            fail(e, "too many locals for fixed array field copy");
            return 0;
        }
        e->fixed_array_copy_slot = e->next_slot + 1;
        fprintf(e->out, "local %u ptr\nlocal %u ptr\nlocal %u long\n",
                e->next_slot, e->next_slot + 1, e->next_slot + 2);
        e->next_slot += 3;
    }
    unsigned dst = e->fixed_array_copy_slot - 1, src = dst + 1, index = dst + 2;
    unsigned loop = label(e), done = label(e);
    fprintf(e->out, "store %u ptr\nstore %u ptr\npush 0\ncast int long\nstore %u long\n",
            src, dst, index);
    mark(e, loop);
    fprintf(e->out, "load %u\npush %zu\nlt long\njz L%u\n"
                    "load %u\nload %u\nptradd byte\n"
                    "load %u\nload %u\nptradd byte\nloadind byte\nstoreind byte\ndrop\n"
                    "load %u\npush 1\ncast int long\nadd long\nstore %u long\njmp L%u\n",
            index, size, done, dst, index, src, index, index, index, loop);
    mark(e, done);
    return 1;
}

/**
 * @brief Emits recursive field-by-field copies between named structures.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param sd Declaration node being checked, serialized, or emitted.
 * @param dst Text or name input to this operation.
 * @param dstlocal int value used by this operation.
 * @param src Text or name input to this operation.
 * @param srclocal int value used by this operation.
 * @param depth Current recursive nesting depth.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int emit_struct_copy_fields(Emitter *e, const Decl *sd, const char *dst, int dstlocal,
                                   const char *src, int srclocal, unsigned depth) {
    if (!sd || depth > 24)
        return 0;
    for (size_t i = 0; i < sd->as.structure.field_count; i++) {
        Decl *f = sd->as.structure.fields[i];
        if (field_is_zero_storage(f))
            continue;
        char dk[512], sk[512];
        const Type *t = f->as.variable.type;
        if (f->as.variable.anonymous_member) {
            if (snprintf(dk, sizeof dk, "%s", dst) >= (int)sizeof dk ||
                snprintf(sk, sizeof sk, "%s", src) >= (int)sizeof sk)
                return 0;
        } else if (!path_append(dk, sizeof dk, dst, f->as.variable.name, dstlocal) ||
                   !path_append(sk, sizeof sk, src, f->as.variable.name, srclocal))
            return 0;
        if (t && t->kind == TYPE_STRUCT) {
            Decl *nested = find_struct(e->program, t->struct_name);
            if (!emit_struct_copy_fields(e, nested, dk, dstlocal, sk, srclocal, depth + 1))
                return 0;
            continue;
        }
        if (t && t->kind == TYPE_ARRAY) {
            unsigned ds, ss;
            if (dstlocal) {
                if (!lookup(e, dk, &ds, NULL))
                    return 0;
                emit_local_addr(e, dk, ds);
            } else
                fprintf(e->out, "gaddr %s\n", dk);
            if (srclocal) {
                if (!lookup(e, sk, &ss, NULL))
                    return 0;
                emit_local_addr(e, sk, ss);
            } else
                fprintf(e->out, "gaddr %s\n", sk);
            if (!emit_fixed_array_copy(e, t))
                return 0;
            continue;
        }
        if (dstlocal) {
            unsigned ds;
            const Type *dt;
            if (!lookup(e, dk, &ds, &dt))
                return 0;
            if (srclocal) {
                unsigned ss;
                const Type *st;
                if (!lookup(e, sk, &ss, &st))
                    return 0;
                emit_local_load(e, sk, ss, st);
            } else
                fprintf(e->out, "gload %s %s\n", sk, tn(dt));
            emit_local_store(e, dk, ds, dt);
        } else {
            if (srclocal) {
                unsigned ss;
                const Type *st;
                if (!lookup(e, sk, &ss, &st))
                    return 0;
                emit_local_load(e, sk, ss, st);
                fprintf(e->out, "gstore %s %s\n", dk, tn(t));
            } else
                fprintf(e->out, "gload %s %s\ngstore %s %s\n", sk, tn(t), dk, tn(t));
        }
    }
    return 1;
}
static int emit_struct_copy_expressions(Emitter *, const Expr *, const Expr *, const Type *,
                                        unsigned);
/**
 * @brief Detects aggregates requiring object-representation copies.
 * @param p Program containing structure declarations.
 * @param t Aggregate type to inspect.
 * @param depth Current nesting depth.
 * @return Nonzero if the type contains a union or bit-field.
 */
static int requires_object_copy(const AstProgram *p, const Type *t, unsigned depth) {
    if (!t || depth > 24)
        return 0;
    if (t->kind == TYPE_ARRAY)
        return requires_object_copy(p, t->pointee, depth + 1);
    if (t->kind != TYPE_STRUCT)
        return 0;
    if (t->is_union)
        return 1;
    const Decl *sd = t->struct_name ? find_struct(p, t->struct_name) : NULL;
    if (!sd)
        return 0;
    for (size_t i = 0; i < sd->as.structure.field_count; i++)
        if (sd->as.structure.fields[i]->as.variable.is_bit_field ||
            (!field_is_zero_storage(sd->as.structure.fields[i]) &&
             requires_object_copy(p, sd->as.structure.fields[i]->as.variable.type, depth + 1)))
            return 1;
    return 0;
}

/**
 * @brief Emits whole-structure assignment between supported expressions.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param dstx Expression node being analyzed, serialized, or emitted.
 * @param srcx Expression node being analyzed, serialized, or emitted.
 * @param t Crunch type being checked, copied, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int emit_struct_assignment(Emitter *e, const Expr *dstx, const Expr *srcx, const Type *t) {
    if (!dstx || !srcx || !t || t->kind != TYPE_STRUCT || !t->struct_name)
        return 0;
    if (srcx->kind == EXPR_BINARY && srcx->as.binary.op == BINARY_COMMA) {
        if (!emit_discarded_expression(e, srcx->as.binary.left, 0))
            return 0;
        return emit_struct_assignment(e, dstx, srcx->as.binary.right, t);
    }
    if (requires_object_copy(e->program, t, 0) || aggregate_aligned_16(e->program, t)) {
        Expr dst = {0};
        dst.kind = EXPR_UNARY;
        dst.as.unary.op = UNARY_ADDRESS;
        dst.as.unary.operand = (Expr *)dstx;
        if (!expression(e, &dst))
            return 0;
        fputs("dup\n", e->out);
        if (!emit_aggregate_address(e, srcx, 0) || !emit_fixed_array_copy(e, t))
            return 0;
        return 1;
    }
    int indirect[2] = {is_indirect_structure(e, dstx), is_indirect_structure(e, srcx)};
    if (indirect[0] || indirect[1]) {
        unsigned needed = (unsigned)(indirect[0] + indirect[1]);
        if (e->count > CRUNCH_LOCAL_LIMIT - needed || e->next_slot > CRUNCH_LOCAL_LIMIT - needed) {
            fail(e, "too many locals for structure assignment");
            return 0;
        }
        const Type *pointer = pointer_to(e, t);
        if (!pointer)
            return 0;
        size_t scope = e->count;
        const Expr *parts[2] = {dstx, srcx};
        Expr names[2] = {0}, indexes[2] = {0}, zero = {0};
        char storage0[48], storage1[48];
        char *storage[2] = {storage0, storage1};
        zero.kind = EXPR_INTEGER;
        for (size_t i = 0; i < 2; i++) {
            if (!indirect[i])
                continue;
            if (e->next_slot >= CRUNCH_LOCAL_LIMIT) {
                e->count = scope;
                fail(e, "too many locals for structure assignment");
                return 0;
            }
            unsigned slot = e->next_slot++;
            fprintf(e->out, "local %u ptr\n", slot);
            if (!emit_indirect_address(e, parts[i])) {
                e->count = scope;
                return 0;
            }
            fprintf(e->out, "store %u ptr\n", slot);
            snprintf(storage[i], sizeof storage0, "__struct_copy$%u", slot);
            e->locals[e->count++] = (Local){storage[i], slot, pointer, 0};
            names[i].kind = EXPR_NAME;
            names[i].as.name.name = storage[i];
            names[i].as.name.length = strlen(storage[i]);
            indexes[i].kind = EXPR_INDEX;
            indexes[i].as.index.base = &names[i];
            indexes[i].as.index.index = &zero;
            parts[i] = &indexes[i];
        }
        int copied = emit_struct_copy_expressions(e, parts[0], parts[1], t, 0);
        e->count = scope;
        if (!copied)
            return 0;
        fprintf(e->out, "push 0\n");
        return 1;
    }
    int dl = expr_is_local(e, dstx), sl = expr_is_local(e, srcx);
    char dst[512], src[512];
    if (!member_path(dstx, dst, sizeof dst, dl ? 0 : 1) ||
        !member_path(srcx, src, sizeof src, sl ? 0 : 1))
        return 0;
    Decl *sd = find_struct(e->program, t->struct_name);
    if (!emit_struct_copy_fields(e, sd, dst, dl, src, sl, 0))
        return 0;
    fprintf(e->out, "push 0\n");
    return 1;
}
static int expression(Emitter *, const Expr *);
/**
 * @brief Emits an expression and converts it to a requested type if needed.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @param to Crunch type being checked, copied, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int expression_as(Emitter *e, const Expr *x, const Type *to) {
    const Type *from = expr_type(e, x);
    if (to && to->kind == TYPE_POINTER && is_null_literal(x)) {
        fputs("push 0\n", e->out);
        return 1;
    }

    if (!expression(e, x))
        return 0;
    if (from && to &&
        (from->kind != to->kind ||
         (from->kind == TYPE_ENUM && type_effective_kind(from) != type_effective_kind(to))) &&
        !cast_emit(e, from, to))
        return 0;
    return 1;
}
/**
 * @brief Emits recursive copies between structure-valued expressions.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param dst Expression node being analyzed, serialized, or emitted.
 * @param src Expression node being analyzed, serialized, or emitted.
 * @param sd Structure declaration containing the fields to copy.
 * @param depth Current recursive nesting depth.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int emit_struct_copy_expression_fields(Emitter *e, const Expr *dst, const Expr *src,
                                              const Decl *sd, unsigned depth) {
    if (!dst || !src || !sd || depth > 24)
        return 0;
    for (size_t i = 0; i < sd->as.structure.field_count; i++) {
        Decl *f = sd->as.structure.fields[i];
        if (field_is_zero_storage(f))
            continue;
        if (f->as.variable.anonymous_member) {
            Decl *nested = find_struct(e->program, f->as.variable.type->struct_name);
            if (!emit_struct_copy_expression_fields(e, dst, src, nested, depth + 1))
                return 0;
            continue;
        }
        Expr dm = {0}, sm = {0};
        dm.kind = sm.kind = EXPR_MEMBER;
        dm.as.member.base = (Expr *)dst;
        sm.as.member.base = (Expr *)src;
        dm.as.member.name = sm.as.member.name = f->as.variable.name;
        dm.as.member.name_length = sm.as.member.name_length = f->as.variable.name_length;
        if (f->as.variable.type && f->as.variable.type->kind == TYPE_STRUCT) {
            if (!emit_struct_copy_expressions(e, &dm, &sm, f->as.variable.type, depth + 1))
                return 0;
        } else if (f->as.variable.type && f->as.variable.type->kind == TYPE_ARRAY) {
            if (!expression(e, &dm) || !expression(e, &sm) ||
                !emit_fixed_array_copy(e, f->as.variable.type))
                return 0;
        } else {
            Expr assignment = {0};
            assignment.kind = EXPR_ASSIGN;
            assignment.as.assign.op = ASSIGN_SIMPLE;
            assignment.as.assign.target = &dm;
            assignment.as.assign.value = &sm;
            if (!expression(e, &assignment))
                return 0;
            fprintf(e->out, "drop\n");
        }
    }
    return 1;
}
/**
 * @brief Emits recursive copies between structure-valued expressions.
 * @param e IR emitter state containing the program and output stream.
 * @param dst Destination expression.
 * @param src Source expression.
 * @param t Structure type to copy.
 * @param depth Current nesting depth.
 * @return One on success, otherwise zero.
 */
static int emit_struct_copy_expressions(Emitter *e, const Expr *dst, const Expr *src, const Type *t,
                                        unsigned depth) {
    if (!t || t->kind != TYPE_STRUCT)
        return 0;
    return emit_struct_copy_expression_fields(e, dst, src, find_struct(e->program, t->struct_name),
                                              depth);
}
/**
 * @brief Allocates and returns the next IR label number.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static unsigned label(Emitter *e) {
    return e->next_label++;
}
/**
 * @brief Emits an IR label definition.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param n Number of elements, bytes, or records.
 * @return No value; the function performs the documented output or state update.
 */
static void mark(Emitter *e, unsigned n) {
    fprintf(e->out, "label L%u\n", n);
}

static int statement(Emitter *, const Stmt *);
static void emit_stack_restore(Emitter *, unsigned);
/**
 * @brief Encodes a string literal as hexadecimal bytes for IR.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @param out Output FILE stream.
 * @param cap Capacity of the output buffer.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int string_hex(const Expr *x, char *out, size_t cap) {
    const unsigned char *b;
    size_t n, i = 1, used = 0;
    if (!x || x->kind != EXPR_STRING || x->as.string.length < 2)
        return 0;
    if (x->string_literal_wide) {
        size_t count = string_array_count(x);
        static const char digits[] = "0123456789abcdef";
        if (!x->as.string.wide_bytes || count > (SIZE_MAX - 1) / 8 ||
            cap < count * 8 + 1)
            return 0;
        for (size_t j = 0; j < count * 4; ++j) {
            unsigned byte = x->as.string.wide_bytes[j];
            out[j * 2] = digits[byte >> 4];
            out[j * 2 + 1] = digits[byte & 15];
        }
        out[count * 8] = 0;
        return 1;
    }
    b = x->as.string.bytes;
    n = x->as.string.length;
    while (i + 1 < n) {
        unsigned v = b[i++];
        if (v == '\\' && i + 1 < n) {
            unsigned char c = b[i++];
            if (c >= '0' && c <= '7') {
                v = c - '0';
                for (int digits = 1; digits < 3 && i + 1 < n &&
                     b[i] >= '0' && b[i] <= '7'; ++digits)
                    v = v * 8 + (b[i++] - '0');
            } else switch (c) {
            case 'a':
                v = 7;
                break;
            case 'b':
                v = 8;
                break;
            case 'f':
                v = 12;
                break;
            case 'n':
                v = '\n';
                break;
            case 'r':
                v = '\r';
                break;
            case 't':
                v = '\t';
                break;
            case 'v':
                v = 11;
                break;
            case 'e':
            case 'E':
                v = 27;
                break;
            case '\\':
                v = '\\';
                break;
            case '\'':
                v = '\'';
                break;
            case '\"':
                v = '\"';
                break;
            case 'x': {
                v = 0;
                while (i + 1 < n && isxdigit(b[i])) {
                    unsigned h = b[i++];
                    v = v * 16 + (h <= '9' ? h - '0' : (h | 32) - 'a' + 10);
                }
                break;
            }
            default:
                v = c;
                break;
            }
        }
        if (used + 2 >= cap)
            return 0;
        static const char hex[] = "0123456789abcdef";
        out[used++] = hex[(v >> 4) & 15];
        out[used++] = hex[v & 15];
    }
    if (used + 2 >= cap)
        return 0;
    out[used++] = '0';
    out[used++] = '0';
    out[used] = 0;
    return 1;
}
/**
 * @brief Emits a numeric compound operation on a value already on the stack.
 * @param e IR emitter state.
 * @param target_type Type of the value being updated.
 * @param value Right-hand expression.
 * @param assignment Compound assignment operator.
 * @return One on success, or zero on failure.
 */
static int emit_compound_numeric(Emitter *e, const Type *target_type, const Expr *value,
                                 AssignOp assignment) {
    const Type *rhs = expr_type(e, value);
    int unsigned_shift = (assignment == ASSIGN_SHIFT_LEFT || assignment == ASSIGN_SHIFT_RIGHT) &&
        rhs && (e->program->c11_numeric || target_type->kind == TYPE_UNSIGNED_INT ||
                target_type->kind == TYPE_UNSIGNED_LONG || rhs->kind == TYPE_UNSIGNED_INT ||
                rhs->kind == TYPE_UNSIGNED_LONG || promoted_small_kind(target_type->kind) ||
                promoted_small_kind(rhs->kind));
    const Type *optype = unsigned_shift
                             ? (target_type->kind == TYPE_ENUM
                                    ? (type_effective_kind(target_type) == TYPE_UNSIGNED_INT
                                           ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE)
                                    : target_type->kind == TYPE_BYTE || promoted_small_kind(target_type->kind)
                                          ? &TYPE_INT_VALUE : target_type)
                             : promote(target_type, rhs, e->program->c11_numeric);
    if (!optype) {
        fail(e, "compound assignment requires numeric operands");
        return 0;
    }
    if (optype->kind == TYPE_LONG_DOUBLE) {
        const char *op = assignment == ASSIGN_ADD ? "extadd"
                       : assignment == ASSIGN_SUBTRACT ? "extsub"
                       : assignment == ASSIGN_MULTIPLY ? "extmul"
                       : assignment == ASSIGN_DIVIDE ? "extdiv" : NULL;
        if (!op || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
            fail(e, "unsupported long double compound assignment");
            return 0;
        }
        if (!cast_emit(e, target_type, optype))
            return 0;
        if (e->next_slot >= CRUNCH_LOCAL_LIMIT) {
            fail(e, "too many locals for long double compound assignment");
            return 0;
        }
        unsigned slot = e->next_slot++;
        fprintf(e->out, "local %u ptr\nstore %u ptr\n", slot, slot);
        if (!emit_extended_temporary(e))
            return 0;
        fprintf(e->out, "load %u\n", slot);
        if (!expression_as(e, value, optype))
            return 0;
        fprintf(e->out, "%s\n", op);
        return cast_emit(e, optype, target_type);
    }
    if (target_type->kind != optype->kind && !cast_emit(e, target_type, optype))
        return 0;
    if (!(unsigned_shift ? expression(e, value) : expression_as(e, value, optype)))
        return 0;
    const char *op = assignment == ASSIGN_ADD           ? "add"
                     : assignment == ASSIGN_SUBTRACT    ? "sub"
                     : assignment == ASSIGN_MULTIPLY    ? "mul"
                     : assignment == ASSIGN_DIVIDE      ? "div"
                     : assignment == ASSIGN_REMAINDER   ? "rem"
                     : assignment == ASSIGN_BITWISE_AND ? "band"
                     : assignment == ASSIGN_BITWISE_OR  ? "bor"
                     : assignment == ASSIGN_BITWISE_XOR ? "bxor"
                     : assignment == ASSIGN_SHIFT_LEFT  ? "shl"
                                                        : "shr";
    fprintf(e->out, "%s %s\n", op, tn(optype));
    if (target_type->kind != optype->kind && !cast_emit(e, optype, target_type))
        return 0;
    return 1;
}
/**
 * @brief Emits a scalar assignment through an address already on the stack.
 * @param e IR emitter state.
 * @param target_type Type of the addressed scalar.
 * @param value Right-hand expression.
 * @param assignment Assignment operator.
 * @return One on success, or zero on failure.
 */
static int emit_indirect_assignment(Emitter *e, const Type *target_type, const Expr *value,
                                    AssignOp assignment) {
    if (assignment == ASSIGN_SIMPLE) {
        if (!expression_as(e, value, target_type))
            return 0;
    } else {
        if (!isnum(target_type)) {
            fail(e, "indirect compound assignment requires a numeric value");
            return 0;
        }
        fprintf(e->out, "dup\nloadind %s\n", tn(target_type));
        if (!emit_compound_numeric(e, target_type, value, assignment))
            return 0;
    }
    fprintf(e->out, "storeind %s\n", tn(target_type));
    return 1;
}
/**
 * @brief Emits a numeric increment or decrement of the value on the stack.
 * @param e IR emitter state.
 * @param type Numeric type of the value.
 * @param subtract Nonzero for decrement.
 * @return No value; emits the updated value.
 */
static void emit_numeric_step(Emitter *e, const Type *type, int subtract) {
    const Type *optype = type->kind == TYPE_BYTE || promoted_small_kind(type->kind)
                             ? promote(type, type, e->program->c11_numeric) : type;
    if (type->kind != optype->kind)
        fprintf(e->out, "cast %s %s\n", tn(type), tn(optype));
    if (type->kind == TYPE_REAL)
        fputs("pushr 0x3ff0000000000000\n", e->out);
    else if (type->kind == TYPE_FLOAT)
        fputs("pushf 0x3f800000\n", e->out);
    else
        fputs("push 1\n", e->out);
    fprintf(e->out, "%s %s\n", subtract ? "sub" : "add", tn(optype));
    if (type->kind == TYPE_BOOL)
        (void)cast_emit(e, optype, type);
    else
        fprintf(e->out, "cast %s %s\n", tn(optype), tn(type));
}
/**
 * @brief Emits an indirect numeric increment or decrement.
 * @param e IR emitter state.
 * @param target Numeric lvalue being updated.
 * @param type Type of the numeric lvalue.
 * @param op Prefix or postfix increment or decrement operator.
 * @return One on success, or zero on failure.
 */
static int emit_indirect_numeric_step(Emitter *e, const Expr *target, const Type *type,
                                      UnaryOp op) {
    Expr address = {0};
    address.kind = EXPR_UNARY;
    address.as.unary.op = UNARY_ADDRESS;
    address.as.unary.operand = (Expr *)target;
    if (!expression(e, &address))
        return 0;
    int post = op == UNARY_POST_INCREMENT || op == UNARY_POST_DECREMENT;
    unsigned slot = 0;
    if (post) {
        TypeKind slot_kind = type_effective_kind(type);
        /* No nested expression runs while the saved value is live, so slots can be reused. */
        if (!e->numeric_postfix_slots[slot_kind]) {
            if (e->next_slot >= CRUNCH_LOCAL_LIMIT) {
                fail(e, "too many locals for numeric postfix update");
                return 0;
            }
            slot = e->next_slot++;
            e->numeric_postfix_slots[slot_kind] = slot + 1;
            fprintf(e->out, "local %u %s\n", slot, tn(type));
        } else {
            slot = e->numeric_postfix_slots[slot_kind] - 1;
        }
    }
    fprintf(e->out, "dup\nloadind %s\n", tn(type));
    if (post)
        fprintf(e->out, "dup\nstore %u %s\n", slot, tn(type));
    emit_numeric_step(e, type, op == UNARY_PRE_DECREMENT || op == UNARY_POST_DECREMENT);
    fprintf(e->out, "storeind %s\n", tn(type));
    if (post)
        fprintf(e->out, "drop\nload %u\n", slot);
    return 1;
}

/**
 * @brief Loads the bytes occupied by a bit-field as an unsigned integer.
 * @param e IR emitter.
 * @param address Slot containing the address of the first byte.
 * @param bit_offset Bit position within the first byte.
 * @param width Field width.
 * @return No value; leaves the bytes on the IR stack.
 */
static void emit_bit_field_bytes(Emitter *e, unsigned address, unsigned bit_offset, unsigned width) {
    unsigned bytes = (bit_offset + width + 7) / 8;
    fputs("push 0\ncast int ulong\n", e->out);
    for (unsigned i = 0; i < bytes; ++i) {
        fprintf(e->out, "load %u\npush %u\nptradd byte\nloadind byte\ncast byte ulong\n",
                address, i);
        if (i)
            fprintf(e->out, "push %u\nshl ulong\n", i * 8);
        fputs("bor ulong\n", e->out);
    }
}

/**
 * @brief Extracts and sign-extends a bit-field from an unsigned stack value.
 * @param e IR emitter.
 * @param type Declared field type.
 * @param bit_offset Bit position within the stack value.
 * @param width Field width.
 * @return No value; replaces the stack value with the extracted field.
 */
static void emit_bit_field_extract(Emitter *e, const Type *type, unsigned bit_offset,
                                   unsigned width) {
    uint64_t mask = (UINT64_C(1) << width) - 1;
    if (bit_offset)
        fprintf(e->out, "push %u\nshr ulong\n", bit_offset);
    fprintf(e->out, "push %" PRIu64 "\nband ulong\n", mask);
    if (type->kind == TYPE_INT) {
        fputs("cast ulong int\n", e->out);
        if (width < 32)
            fprintf(e->out, "push %u\nshl int\npush %u\nshr int\n",
                    32 - width, 32 - width);
    } else {
        fprintf(e->out, "cast ulong %s\n", tn(type));
    }
}

/**
 * @brief Emits a bit-field read with one evaluation of its containing object.
 * @param e IR emitter.
 * @param x Member expression.
 * @param field Field declaration.
 * @param layout Field position.
 * @return Nonzero on success.
 */
static int emit_bit_field_read(Emitter *e, const Expr *x, const Decl *field,
                               const FieldLayout *layout) {
    if (!layout->bit_width || layout->bit_width > 32 || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "unsupported bit-field read");
        return 0;
    }
    unsigned address = e->next_slot++;
    fprintf(e->out, "local %u ptr\n", address);
    if (!emit_bit_field_address(e, x, layout))
        return 0;
    fprintf(e->out, "store %u ptr\n", address);
    emit_bit_field_bytes(e, address, layout->bit_offset, layout->bit_width);
    emit_bit_field_extract(e, field->as.variable.type, layout->bit_offset, layout->bit_width);
    return 1;
}

/**
 * @brief Emits a bit-field's semantic integer promotion when required.
 * @param e IR emitter.
 * @param x Expression whose value is being emitted.
 * @param field Field declaration.
 * @param layout Field position.
 * @return No value; converts the value on the IR stack if necessary.
 */
static void emit_bit_field_promotion(Emitter *e, const Expr *x, const Decl *field,
                                     const FieldLayout *layout) {
    if (field->as.variable.type->kind == TYPE_UNSIGNED_INT && layout->bit_width < 32 &&
        x->semantic_type == TYPE_INT)
        fputs("cast uint int\n", e->out);
}

/**
 * @brief Updates a bit-field at an address already on the stack.
 * @param e IR emitter.
 * @param type Declared field type.
 * @param bit_offset Position within the first byte.
 * @param width Field width.
 * @param value Right operand, or NULL for increment or decrement.
 * @param assignment Assignment operator when value is non-NULL.
 * @param step One for increment, minus one for decrement, zero for assignment.
 * @param post Nonzero to return the old value.
 * @return Nonzero on success.
 */
static int emit_bit_field_update(Emitter *e, const Type *type, unsigned bit_offset,
                                 unsigned width, const Expr *value, AssignOp assignment,
                                 int step, int post) {
    int compound = !step && assignment != ASSIGN_SIMPLE;
    unsigned needed = 2u + (post ? 1u : 0u) + (step || compound ? 1u : 0u) +
                      (compound ? 1u : 0u);
    if (!type || !width || width > 32 || e->next_slot > CRUNCH_LOCAL_LIMIT - needed ||
        (compound && e->count >= CRUNCH_LOCAL_LIMIT)) {
        fail(e, "unsupported bit-field update");
        return 0;
    }
    unsigned address = e->next_slot++, saved = e->next_slot++;
    unsigned old = post ? e->next_slot++ : 0;
    unsigned bytes = step || compound ? e->next_slot++ : 0;
    unsigned right = compound ? e->next_slot++ : 0;
    fprintf(e->out, "local %u ptr\nlocal %u ulong\n", address, saved);
    if (post)
        fprintf(e->out, "local %u %s\n", old, tn(type));
    if (step || compound)
        fprintf(e->out, "local %u ulong\n", bytes);
    fprintf(e->out, "store %u ptr\n", address);
    const Type *operation_type = type->kind == TYPE_UNSIGNED_INT && width < 32
                                     ? &TYPE_INT_VALUE : type;
    const Type *right_type = compound ? expr_type(e, value) : NULL;
    if (compound) {
        if (!right_type) {
            fail(e, "unsupported bit-field compound operand");
            return 0;
        }
        fprintf(e->out, "local %u %s\n", right,
                right_type->kind == TYPE_LONG_DOUBLE ? "ptr" : tn(right_type));
        if (!expression(e, value))
            return 0;
        fprintf(e->out, "store %u %s\n", right,
                right_type->kind == TYPE_LONG_DOUBLE ? "ptr" : tn(right_type));
    }
    if (step || compound) {
        emit_bit_field_bytes(e, address, bit_offset, width);
        fprintf(e->out, "dup\nstore %u ulong\n", bytes);
        emit_bit_field_extract(e, type, bit_offset, width);
        if (post)
            fprintf(e->out, "dup\nstore %u %s\n", old, tn(type));
        if (operation_type != type && !cast_emit(e, type, operation_type))
            return 0;
        if (step)
            emit_numeric_step(e, operation_type, step < 0);
        else {
            char name[64];
            snprintf(name, sizeof name, "__bit_field_rhs_%u", right);
            e->locals[e->count++] = (Local){name, right, right_type,
                                           right_type->kind == TYPE_LONG_DOUBLE
                                               ? LOCAL_EXTENDED_PARAM : 0};
            Expr operand = {0};
            operand.kind = EXPR_NAME;
            operand.as.name.name = name;
            operand.as.name.length = strlen(name);
            int ok = emit_compound_numeric(e, operation_type, &operand, assignment);
            --e->count;
            if (!ok)
                return 0;
        }
    } else {
        operation_type = type;
        if (!expression_as(e, value, type))
            return 0;
    }
    if (!cast_emit(e, operation_type, &TYPE_ULONG_VALUE))
        return 0;
    uint64_t mask = (UINT64_C(1) << width) - 1;
    fprintf(e->out, "push %" PRIu64 "\nband ulong\nstore %u ulong\n", mask, saved);
    unsigned byte_count = (bit_offset + width + 7) / 8;
    uint64_t shifted_mask = mask << bit_offset;
    for (unsigned i = 0; i < byte_count; ++i) {
        unsigned byte_mask = (unsigned)((shifted_mask >> (8 * i)) & 255);
        fprintf(e->out, "load %u\npush %u\nptradd byte\n", address, i);
        if (step || compound) {
            fprintf(e->out, "load %u\n", bytes);
            if (i)
                fprintf(e->out, "push %u\nshr ulong\n", i * 8);
        } else {
            fputs("dup\nloadind byte\ncast byte ulong\n", e->out);
        }
        fprintf(e->out, "push %u\nband ulong\nload %u\n", 255u ^ byte_mask, saved);
        if (!i && bit_offset)
            fprintf(e->out, "push %u\nshl ulong\n", bit_offset);
        else if (i)
            fprintf(e->out, "push %u\nshr ulong\n", 8 * i - bit_offset);
        fprintf(e->out, "push %u\nband ulong\nbor ulong\ncast ulong byte\n"
                        "storeind byte\ndrop\n", byte_mask);
    }
    if (post) {
        fprintf(e->out, "load %u\n", old);
    } else {
        fprintf(e->out, "load %u\n", saved);
        emit_bit_field_extract(e, type, 0, width);
    }
    return 1;
}

/**
 * @brief Emits the address of a supported structure-valued argument.
 * @param e IR emitter.
 * @param x Structure-valued expression.
 * @param depth Current conditional-expression nesting depth.
 * @return Nonzero on success.
 */
static int emit_aggregate_address(Emitter *e, const Expr *x, unsigned depth) {
    if (!x || depth >= IR_MAX_AST_RECURSION_DEPTH) {
        fail(e, "unsupported structure argument expression");
        return 0;
    }
    if (x->kind == EXPR_CAST && is_indirect_structure(e, x))
        return emit_compound_literal(e, x);
    if (x->kind == EXPR_BINARY && x->as.binary.op == BINARY_COMMA)
        return expression(e, x);
    if (x->kind == EXPR_CALL)
        return expression(e, x);
    if (x->kind == EXPR_CONDITIONAL) {
        unsigned other = label(e), end = label(e);
        const Expr *condition = x->as.conditional.condition;
        if (!expression(e, condition))
            return 0;
        emit_truth(e, expr_type(e, condition));
        fprintf(e->out, "jz L%u\n", other);
        if (!emit_aggregate_address(e, x->as.conditional.then_expr, depth + 1))
            return 0;
        fprintf(e->out, "jmp L%u\n", end);
        mark(e, other);
        if (!emit_aggregate_address(e, x->as.conditional.else_expr, depth + 1))
            return 0;
        mark(e, end);
        return 1;
    }
    if (x->kind == EXPR_ASSIGN) {
        const Type *type = expr_type(e, x);
        if (type && (requires_object_copy(e->program, type, 0) ||
                     aggregate_aligned_16(e->program, type)))
            return expression(e, x);
        char path[512];
        if (!member_path(x->as.assign.target, path, sizeof path, 0) || !expression(e, x)) {
            fail(e, "unsupported structure assignment argument");
            return 0;
        }
        fputs("drop\n", e->out);
        return emit_aggregate_address(e, x->as.assign.target, depth + 1);
    }
    Expr address = {0};
    address.kind = EXPR_UNARY;
    address.as.unary.op = UNARY_ADDRESS;
    address.as.unary.operand = (Expr *)x;
    return expression(e, &address);
}

/**
 * @brief Emits a discarded expression, retaining volatile aggregate reads.
 * @param e IR emitter.
 * @param x Expression whose value is discarded.
 * @param depth Current nesting depth.
 * @return Nonzero on success.
 */
static int emit_discarded_expression(Emitter *e, const Expr *x, unsigned depth) {
    if (!x || depth >= IR_MAX_AST_RECURSION_DEPTH) {
        fail(e, "maximum discarded-expression nesting depth exceeded");
        return 0;
    }
    const Type *type = expr_type(e, x);
    if (type && type->kind == TYPE_STRUCT && x->kind == EXPR_BINARY &&
        x->as.binary.op == BINARY_COMMA)
        return emit_discarded_expression(e, x->as.binary.left, depth + 1) &&
               emit_discarded_expression(e, x->as.binary.right, depth + 1);
    if (type && type->kind == TYPE_STRUCT && x->kind == EXPR_CONDITIONAL) {
        unsigned other = label(e), end = label(e);
        if (!expression(e, x->as.conditional.condition))
            return 0;
        emit_truth(e, expr_type(e, x->as.conditional.condition));
        fprintf(e->out, "jz L%u\n", other);
        if (!emit_discarded_expression(e, x->as.conditional.then_expr, depth + 1))
            return 0;
        fprintf(e->out, "jmp L%u\n", end);
        mark(e, other);
        if (!emit_discarded_expression(e, x->as.conditional.else_expr, depth + 1))
            return 0;
        mark(e, end);
        return 1;
    }
    if (!type || type->kind != TYPE_STRUCT || x->kind == EXPR_ASSIGN) {
        if (!expression(e, x))
            return 0;
        fputs("drop\n", e->out);
        return 1;
    }
    if (!emit_aggregate_address(e, x, 0))
        return 0;
    if (!x->semantic_volatile_lvalue) {
        fputs("drop\n", e->out);
        return 1;
    }
    size_t size, alignment;
    if (!type_layout_size(type, layout_find_struct, e->program, &size, &alignment) ||
        !size || size > INT64_MAX) {
        fail(e, "unsupported volatile structure layout");
        return 0;
    }
    if (!e->volatile_read_slot) {
        if (e->next_slot > CRUNCH_LOCAL_LIMIT - 2) {
            fail(e, "too many locals for volatile structure read");
            return 0;
        }
        e->volatile_read_slot = e->next_slot + 1;
        fprintf(e->out, "local %u ptr\nlocal %u long\n", e->next_slot, e->next_slot + 1);
        e->next_slot += 2;
    }
    unsigned address = e->volatile_read_slot - 1, index = address + 1;
    unsigned loop = label(e), done = label(e);
    fprintf(e->out, "store %u ptr\npush 0\ncast int long\nstore %u long\n",
            address, index);
    mark(e, loop);
    fprintf(e->out, "load %u\npush %zu\nlt long\njz L%u\n"
                    "load %u\nload %u\nptradd byte\nloadind byte\ndrop\n"
                    "load %u\npush 1\ncast int long\nadd long\nstore %u long\njmp L%u\n",
            index, size, done, address, index, index, index, loop);
    mark(e, done);
    return 1;
}

/**
 * @brief Snapshots an aggregate argument at its evaluation point.
 * @param e IR emitter.
 * @param x Structure-valued argument.
 * @param size Exact structure size in bytes.
 * @return Nonzero on success.
 */
static int emit_aggregate_argument(Emitter *e, const Expr *x, size_t size) {
    if (!size || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "unsupported structure argument or too many locals");
        return 0;
    }
    unsigned slot = e->next_slot++;
    size_t padded = (size + 7) / 8 * 8;
    fprintf(e->out, "local %u ptr\nlocalblock __aggregate_arg_%u %zu %d\n"
                    "localalias __aggregate_arg_address_%u __aggregate_arg_%u 0 byte\n",
            slot, slot, padded,
            (e->program->contains_long_double || e->program->contains_alignas) ? 16 : 8,
            slot, slot);
    if (!emit_aggregate_address(e, x, 0))
        return 0;
    fprintf(e->out, "store %u ptr\n", slot);
    if (size > 16) {
        fprintf(e->out, "laddr __aggregate_arg_address_%u\nload %u\n", slot, slot);
        if (!emit_fixed_array_copy(e, expr_type(e, x)))
            return 0;
        fprintf(e->out, "laddr __aggregate_arg_address_%u\n", slot);
        return 1;
    }
    for (size_t i = 0; i < padded; i++) {
        fprintf(e->out, "laddr __aggregate_arg_address_%u\npush %zu\nptradd byte\n", slot, i);
        if (i < size)
            fprintf(e->out, "load %u\npush %zu\nptradd byte\nloadind byte\n", slot, i);
        else
            fputs("push 0\n", e->out);
        fputs("storeind byte\ndrop\n", e->out);
    }
    fprintf(e->out, "laddr __aggregate_arg_address_%u\n", slot);
    return 1;
}

/**
 * @brief Returns the type used for an unnamed variadic argument.
 * @param type Argument type after array and function decay.
 * @return Type after the default argument promotions, or NULL for void.
 */
static const Type *variadic_argument_type(const Type *type) {
    if (!type || type->kind == TYPE_VOID)
        return NULL;
    if (type->kind == TYPE_FLOAT)
        return &TYPE_REAL_VALUE;
    if (type->kind == TYPE_BYTE || promoted_small_kind(type->kind))
        return &TYPE_INT_VALUE;
    if (type->kind == TYPE_ENUM)
        return type_effective_kind(type) == TYPE_UNSIGNED_INT ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE;
    return type;
}

/**
 * @brief Emits a call through a typed function pointer, including variadic calls.
 * @param e IR emitter.
 * @param call Call expression.
 * @return Nonzero on success.
 */
static int emit_indirect_call(Emitter *e, const Expr *call) {
    const Type *pointer = expr_type(e, call->as.call.callee);
    Decl *signature = pointer && pointer->kind == TYPE_POINTER && pointer->pointee &&
                      pointer->pointee->kind == TYPE_FUNCTION
                          ? pointer->pointee->function_signature : NULL;
    size_t fixed = signature ? signature->as.function.parameters.count : 0;
    if (!signature || call->as.call.arguments.count < fixed ||
        (!signature->as.function.variadic && fixed != call->as.call.arguments.count) ||
        call->as.call.arguments.count > 128) {
        fail(e, "unsupported indirect function call");
        return 0;
    }
    const Type *parameters[128];
    for (size_t i = 0; i < call->as.call.arguments.count; ++i) {
        parameters[i] = i < fixed ? signature->as.function.parameters.items[i].type :
                        variadic_argument_type(expr_type(e, call->as.call.arguments.items[i]));
        if (!parameters[i]) {
            fail(e, "unsupported variadic argument type");
            return 0;
        }
    }
    const Type *result = signature->as.function.return_type;
    if (result && result->kind == TYPE_STRUCT) {
        if (!emit_aggregate_result_buffer(e, aggregate_parameter_size(e->program, result)))
            return 0;
    } else if (result && result->kind == TYPE_LONG_DOUBLE && !emit_extended_temporary(e)) {
        return 0;
    }
    if (!expression(e, call->as.call.callee))
        return 0;
    for (size_t i = 0; i < call->as.call.arguments.count; ++i) {
        const Type *parameter = parameters[i];
        if (parameter && parameter->kind == TYPE_STRUCT
                ? !emit_aggregate_argument(e, call->as.call.arguments.items[i],
                                           aggregate_parameter_size(e->program, parameter))
                : !expression_as(e, call->as.call.arguments.items[i], parameter))
            return 0;
    }
    fprintf(e->out, "callptr %zu ", call->as.call.arguments.count);
    emit_parameter_type(e->out, e->program, result);
    for (size_t i = 0; i < call->as.call.arguments.count; ++i) {
        fputc(' ', e->out);
        emit_parameter_type(e->out, e->program, parameters[i]);
    }
    fputc('\n', e->out);
    return 1;
}

/**
 * @brief Emits a formatted call using the ordinary variadic function ABI.
 * @param e IR emitter.
 * @param call Call expression.
 * @param function Fixed-parameter function declaration.
 * @return Nonzero on success.
 */
static int emit_external_formatted_call(Emitter *e, const Expr *call, const Decl *function) {
    size_t count = call->as.call.arguments.count;
    size_t fixed = function->as.function.parameters.count;
    const Type *parameters[130];
    if (count > 130)
        return 0;
    fprintf(e->out, "fnaddr %s\n", function->as.function.name);
    for (size_t i = 0; i < count; ++i) {
        parameters[i] = i < fixed ? function->as.function.parameters.items[i].type :
                        variadic_argument_type(expr_type(e, call->as.call.arguments.items[i]));
        if (!parameters[i] || !expression_as(e, call->as.call.arguments.items[i], parameters[i]))
            return 0;
    }
    fprintf(e->out, "callptr %zu int", count);
    for (size_t i = 0; i < count; ++i) {
        fputc(' ', e->out);
        emit_parameter_type(e->out, e->program, parameters[i]);
    }
    fputc('\n', e->out);
    return 1;
}

/**
 * @brief Emits the address of a supported indirect binary80 lvalue.
 * @param e IR emitter.
 * @param x Binary80 lvalue.
 * @return Nonzero on success.
 */
static int emit_extended_lvalue(Emitter *e, const Expr *x) {
    if (x && x->kind == EXPR_CAST && x->as.cast.operand &&
        x->as.cast.operand->kind == EXPR_ARRAY_INIT)
        return emit_compound_literal(e, x);
    if (x && x->kind == EXPR_UNARY && x->as.unary.op == UNARY_DEREFERENCE)
        return expression(e, x->as.unary.operand);
    if (x && x->kind == EXPR_INDEX)
        return emit_index_address(e, x);
    if (x && x->kind == EXPR_MEMBER) {
        Expr address = {0};
        address.kind = EXPR_UNARY;
        address.as.unary.op = UNARY_ADDRESS;
        address.as.unary.operand = (Expr *)x;
        return expression(e, &address);
    }
    if (x && x->kind == EXPR_NAME) {
        unsigned slot;
        const Type *type;
        if (lookup(e, x->as.name.name, &slot, &type) && type &&
            type->kind == TYPE_LONG_DOUBLE) {
            emit_local_addr(e, x->as.name.name, slot);
            return 1;
        }
        Decl *global = find_global(e->program, x->as.name.name);
        if (global && global->as.variable.type &&
            global->as.variable.type->kind == TYPE_LONG_DOUBLE) {
            fprintf(e->out, "gaddr %s\n", global->as.variable.name);
            return 1;
        }
    }
    fail(e, "unsupported long double lvalue");
    return 0;
}

/**
 * @brief Emits a binary80 compound assignment or increment with one lvalue evaluation.
 * @param e IR emitter.
 * @param target Binary80 lvalue.
 * @param value Right operand, or NULL for an increment or decrement.
 * @param op Binary80 arithmetic instruction.
 * @param post Nonzero to return the original value.
 * @return Nonzero on success.
 */
static int emit_extended_update(Emitter *e, const Expr *target, const Expr *value,
                                const char *op, int post) {
    if (e->next_slot > CRUNCH_LOCAL_LIMIT - 2) {
        fail(e, "too many locals for long double update");
        return 0;
    }
    unsigned address = e->next_slot++, old = e->next_slot++;
    fprintf(e->out, "local %u ptr\nlocal %u ptr\n", address, old);
    if (!emit_extended_lvalue(e, target))
        return 0;
    fprintf(e->out, "store %u ptr\n", address);
    if (!emit_extended_temporary(e))
        return 0;
    fprintf(e->out, "load %u\nextcopy\nstore %u ptr\n", address, old);
    if (post)
        fprintf(e->out, "load %u\n", old);
    else if (!emit_extended_temporary(e))
        return 0;
    fprintf(e->out, "load %u\nload %u\n", address, old);
    if (value) {
        if (!expression_as(e, value, &TYPE_LONG_DOUBLE_VALUE))
            return 0;
    } else {
        fputs("push 1\n", e->out);
        if (!cast_emit(e, &TYPE_INT_VALUE, &TYPE_LONG_DOUBLE_VALUE))
            return 0;
    }
    fprintf(e->out, "%s\n%s\n", op, post ? "drop" : "extcopy");
    return 1;
}

/**
 * @brief Emits IR for one expression and reports unsupported forms.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param x Expression node being analyzed, serialized, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int expression_impl(Emitter *e, const Expr *x) {
    unsigned s = 0;
    const Type *t = NULL;
    if (!x) {
        fail(e, "missing expression");
        return 0;
    }
    e->location = x->location;
    const Type *extended_type = e->program->contains_long_double ? expr_type(e, x) : NULL;
    if (extended_type && extended_type->kind == TYPE_LONG_DOUBLE) {
        if (x->kind == EXPR_REAL) {
            long double value;
            unsigned char bytes[16] = {0};
            if (sizeof(value) < 10 || !sema_numeric_constant(x, &value)) {
                fail(e, "cannot encode long double literal");
                return 0;
            }
            memcpy(bytes, &value, 10);
            unsigned id = e->next_string++;
            fprintf(e->out, "stringlit __crunch_string_%u ", id);
            for (size_t i = 0; i < sizeof(bytes); ++i)
                fprintf(e->out, "%02x", bytes[i]);
            fprintf(e->out, "\nstraddr __crunch_string_%u\n", id);
            return 1;
        }
        if ((x->kind == EXPR_CAST && x->as.cast.operand &&
             x->as.cast.operand->kind == EXPR_ARRAY_INIT) ||
            (x->kind == EXPR_UNARY && x->as.unary.op == UNARY_DEREFERENCE) ||
            x->kind == EXPR_INDEX || x->kind == EXPR_NAME || x->kind == EXPR_MEMBER) {
            if (!emit_extended_temporary(e) || !emit_extended_lvalue(e, x))
                return 0;
            fputs("extcopy\n", e->out);
            return 1;
        }
        if (x->kind == EXPR_UNARY &&
            (x->as.unary.op == UNARY_PLUS || x->as.unary.op == UNARY_MINUS)) {
            if (x->as.unary.op == UNARY_MINUS && !emit_extended_temporary(e))
                return 0;
            if (!expression(e, x->as.unary.operand))
                return 0;
            if (x->as.unary.op == UNARY_MINUS)
                fputs("extneg\n", e->out);
            return 1;
        }
        if (x->kind == EXPR_UNARY &&
            (x->as.unary.op == UNARY_PRE_INCREMENT || x->as.unary.op == UNARY_PRE_DECREMENT ||
             x->as.unary.op == UNARY_POST_INCREMENT || x->as.unary.op == UNARY_POST_DECREMENT)) {
            UnaryOp op = x->as.unary.op;
            return emit_extended_update(e, x->as.unary.operand, NULL,
                                        op == UNARY_PRE_DECREMENT || op == UNARY_POST_DECREMENT
                                            ? "extsub" : "extadd",
                                        op == UNARY_POST_INCREMENT || op == UNARY_POST_DECREMENT);
        }
        if (x->kind == EXPR_ASSIGN) {
            if (x->as.assign.op != ASSIGN_SIMPLE) {
                const char *op = x->as.assign.op == ASSIGN_ADD ? "extadd"
                               : x->as.assign.op == ASSIGN_SUBTRACT ? "extsub"
                               : x->as.assign.op == ASSIGN_MULTIPLY ? "extmul"
                               : x->as.assign.op == ASSIGN_DIVIDE ? "extdiv" : NULL;
                if (!op) {
                    fail(e, "unsupported long double compound assignment");
                    return 0;
                }
                return emit_extended_update(e, x->as.assign.target, x->as.assign.value, op, 0);
            }
            if (!emit_extended_temporary(e) || !emit_extended_lvalue(e, x->as.assign.target) ||
                !expression_as(e, x->as.assign.value, &TYPE_LONG_DOUBLE_VALUE))
                return 0;
            fputs("extcopy\nextcopy\n", e->out);
            return 1;
        }
        if (x->kind != EXPR_BINARY && x->kind != EXPR_CONDITIONAL &&
            x->kind != EXPR_CAST && x->kind != EXPR_CALL) {
            fail(e, "long double code generation is not yet supported for this expression");
            return 0;
        }
    }
    switch (x->kind) {
    case EXPR_OFFSETOF:
        if (!x->as.offset_of.resolved) {
            fail(e, "unresolved offsetof expression");
            return 0;
        }
        if (x->as.offset_of.constant) {
            fprintf(e->out, "pushu %" PRIu64 " ulong\n", x->as.offset_of.value);
            return 1;
        }
        fprintf(e->out, "pushu %" PRIu64 " ulong\n", x->as.offset_of.base_offset);
        for (size_t i = 0; i < x->as.offset_of.count; ++i) {
            const InitializerDesignator *item = &x->as.offset_of.items[i];
            if (item->index) {
                if (!expression_as(e, item->index, &TYPE_ULONG_VALUE))
                    return 0;
                fprintf(e->out, "pushu %zu ulong\nmul ulong\nadd ulong\n",
                        item->semantic_stride);
            }
        }
        return 1;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (x->as.size_of.semantic_vla_parameter) {
            const Parameter *parameter = x->as.size_of.semantic_vla_parameter;
            const Type *array = parameter->type->pointee;
            unsigned dimension = x->as.size_of.semantic_vla_dimension, slot;
            while (dimension-- && array)
                array = array->pointee;
            if (!vla_size_slot(e, array, &slot)) {
                fail(e, "variable-length parameter size is out of scope");
                return 0;
            }
            if (x->as.size_of.operand->kind != EXPR_NAME) {
                if (!expression(e, x->as.size_of.operand))
                    return 0;
                fputs("drop\n", e->out);
            }
            fprintf(e->out, "load %u\n", slot);
            return 1;
        }
        if (x->as.size_of.semantic_vla_decl) {
            unsigned slot;
            const Decl *decl = x->as.size_of.semantic_vla_decl;
            if (!lookup(e, decl->as.variable.name, &slot, NULL) ||
                (!decl->as.variable.semantic_vla_pointer &&
                 local_alias(e, decl->as.variable.name) != LOCAL_VLA)) {
                fail(e, "variable-length array size is out of scope");
                return 0;
            }
            if (x->as.size_of.operand->kind != EXPR_NAME) {
                if (!expression(e, x->as.size_of.operand))
                    return 0;
                fputs("drop\n", e->out);
            }
            unsigned dimension = x->as.size_of.semantic_vla_dimension;
            fprintf(e->out, "load %u\n", decl->as.variable.semantic_vla_pointer
                    ? slot + dimension + 1 : dimension ? slot + dimension + 2 : slot + 1);
            return 1;
        }
        if (!x->as.size_of.resolved) {
            fail(e, "unresolved size or alignment expression");
            return 0;
        }
        fprintf(e->out, "pushu %" PRId64 " ulong\n", x->as.size_of.size);
        return 1;
    case EXPR_INTEGER:
        if (x->integer_literal_out_of_range) {
            fail(e, "integer literal exceeds signed long range");
            return 0;
        }
        if ((x->integer_suffix != INTEGER_SUFFIX_NONE && x->integer_suffix != INTEGER_SUFFIX_L &&
             x->integer_suffix != INTEGER_SUFFIX_LL) ||
            x->integer_literal_unsigned)
            fprintf(e->out, "pushu %" PRIu64 " %s\n", x->unsigned_integer_value,
                    tn(expr_type(e, x)));
        else
            fprintf(e->out, "push %" PRId64 "\n", x->as.integer_value);
        return 1;
    case EXPR_REAL: {
        if (x->real_literal_float) {
            float value = (float)x->as.real_value;
            uint32_t bits;
            memcpy(&bits, &value, sizeof bits);
            fprintf(e->out, "pushf 0x%08" PRIx32 "\n", bits);
            return 1;
        }
        uint64_t bits;
        memcpy(&bits, &x->as.real_value, sizeof bits);
        fprintf(e->out, "pushr 0x%016" PRIx64 "\n", bits);
        return 1;
    }
    case EXPR_CHAR:
        fprintf(e->out, "push %d\n", (int)x->as.char_value);
        return 1;
    case EXPR_STRING: {
        char *hex = malloc(65536);
        if (!hex) {
            fail_oom(e);
            return 0;
        }
        unsigned id = e->next_string++;
        if (!string_hex(x, hex, 65536)) {
            free(hex);
            fail(e, "string literal is too large or invalid");
            return 0;
        }
        fprintf(e->out, "stringlit __crunch_string_%u %s%s\nstraddr __crunch_string_%u\n",
                id, hex, x->string_literal_wide ? " 4" : "", id);
        free(hex);
        return 1;
    }
    case EXPR_NAME:
        if (x->semantic_enum_constant) {
            fprintf(e->out, "push %" PRId32 "\n", x->semantic_enum_value);
            return 1;
        }
        if (x->semantic_function_name) {
            if (!e->function_name_string) {
                unsigned id = e->next_string++;
                e->function_name_string = id + 1;
                fprintf(e->out, "stringlit __crunch_string_%u ", id);
                for (const unsigned char *p = (const unsigned char *)e->function_name; *p; ++p)
                    fprintf(e->out, "%02x", (unsigned)*p);
                fputs("00\n", e->out);
            }
            fprintf(e->out, "straddr __crunch_string_%u\n", e->function_name_string - 1);
            return 1;
        }
        if (lookup(e, x->as.name.name, &s, &t)) {
            if (t && t->kind == TYPE_ARRAY) {
                if (local_alias(e, x->as.name.name) == LOCAL_STATIC_ARRAY)
                    fprintf(e->out, "gaddr _crunch_static_local_%u\n", s);
                else if (local_alias(e, x->as.name.name) == LOCAL_VLA)
                    fprintf(e->out, "load %u\n", s);
                else
                    fprintf(e->out, "laddr __local_array_address_%u\n", s);
            }
            else
                emit_local_load(e, x->as.name.name, s, t);
            return 1;
        }
        if ((t = NULL, find_global(e->program, x->as.name.name))) {
            Decl *g = find_global(e->program, x->as.name.name);
            if (g->as.variable.type && g->as.variable.type->kind == TYPE_ARRAY)
                fprintf(e->out, "gaddr %s\n", g->as.variable.name);
            else
                fprintf(e->out, "gload %s %s\n", g->as.variable.name, tn(g->as.variable.type));
            return 1;
        }
        if (find_function(e->program, x->as.name.name)) {
            fprintf(e->out, "fnaddr %s\n", x->as.name.name);
            return 1;
        }
        fail(e, "unknown variable '%s' in IR subset", x->as.name.name);
        return 0;
    case EXPR_MEMBER: {
        FieldLayout layout;
        const Decl *field;
        if (bit_field_info(e, x, &layout, &field)) {
            if (!emit_bit_field_read(e, x, field, &layout))
                return 0;
            emit_bit_field_promotion(e, x, field, &layout);
            return 1;
        }
        const Type *raw = expr_type_raw(e, x);
        if (is_flexible_member(e, x))
            return emit_flexible_member_address(e, x);
        if (is_indirect_member(e, x)) {
            const Type *ft = raw;
            if (!ft || !emit_indirect_address(e, x)) {
                fail(e, "invalid indirect structure member access");
                return 0;
            }
            if (ft->kind != TYPE_STRUCT && ft->kind != TYPE_ARRAY)
                fprintf(e->out, "loadind %s\n", tn(ft));
            return 1;
        }
        const Type *ft = raw;
        if (!x->as.member.base || !ft || (!scalar_field(ft) && ft->kind != TYPE_ARRAY)) {
            fail(e, "member expression is not a supported scalar field");
            return 0;
        }
        char key[512], gkey[512], root[512];
        if (!field_storage_key(x, key, sizeof key, 0)) {
            fail(e, "unsupported member expression");
            return 0;
        }
        if (lookup(e, key, &s, &t)) {
            if (ft->kind == TYPE_ARRAY)
                emit_local_addr(e, key, s);
            else
                emit_local_load(e, key, s, t);
            return 1;
        }
        if (!field_storage_key(x, gkey, sizeof gkey, 1) || !member_path(x, root, sizeof root, 1)) {
            fail(e, "member name too long");
            return 0;
        }
        char *sep = strstr(root, "__");
        if (sep) {
            *sep = '\0';
            Decl *g = find_global(e->program, root);
            if (g && g->as.variable.type && g->as.variable.type->kind == TYPE_STRUCT) {
                if (ft->kind == TYPE_ARRAY)
                    fprintf(e->out, "gaddr %s\n", gkey);
                else
                    fprintf(e->out, "gload %s %s\n", gkey, tn(ft));
                return 1;
            }
        }
        fail(e, "member storage not found");
        return 0;
    }
    case EXPR_UNARY: {
        UnaryOp op = x->as.unary.op;
        const Expr *v = x->as.unary.operand;
        if (op == UNARY_ADDRESS) {
            if (v && v->kind == EXPR_CAST && v->as.cast.target_type &&
                v->as.cast.operand &&
                v->as.cast.operand->kind == EXPR_ARRAY_INIT)
                return emit_compound_literal(e, v);
            if (v && v->kind == EXPR_MEMBER && is_flexible_member(e, v))
                return emit_flexible_member_address(e, v);
            if (v && v->semantic_function_name)
                return expression(e, v);
            if (v && v->kind == EXPR_NAME && !lookup(e, v->as.name.name, &s, NULL) &&
                !find_global(e->program, v->as.name.name) &&
                find_function(e->program, v->as.name.name)) {
                fprintf(e->out, "fnaddr %s\n", v->as.name.name);
                return 1;
            }
            if (v && v->kind == EXPR_UNARY && v->as.unary.op == UNARY_DEREFERENCE)
                return expression(e, v->as.unary.operand);
            if (v && is_indirect_structure(e, v))
                return emit_indirect_address(e, v);
            if (is_indirect_member(e, v)) {
                if (!emit_indirect_address(e, v)) {
                    fail(e, "cannot address indirect structure member");
                    return 0;
                }
                return 1;
            }
            if (v && v->kind == EXPR_INDEX) {
                const Type *element = expr_type_raw(e, v);
                if (!isnum(element) && !is_value_pointer(element) &&
                    (!element || element->kind != TYPE_LONG_DOUBLE) &&
                    (!element || (element->kind != TYPE_ARRAY && element->kind != TYPE_STRUCT))) {
                    fail(e, "address-of index requires a numeric or pointer element");
                    return 0;
                }
                return emit_index_address(e, v);
            }
            if (v && v->kind == EXPR_NAME && lookup(e, v->as.name.name, &s, &t)) {
                if (t && t->kind == TYPE_STRUCT) {
                    if (local_alias(e, v->as.name.name) == LOCAL_STATIC_STRUCT)
                        fprintf(e->out, "gaddr _crunch_static_local_%u\n", s);
                    else
                        fprintf(e->out, "laddr __local_struct_address_%u\n", s);
                }
                else if (t && t->kind == TYPE_ARRAY) {
                    if (local_alias(e, v->as.name.name) == LOCAL_STATIC_ARRAY)
                        fprintf(e->out, "gaddr _crunch_static_local_%u\n", s);
                    else if (local_alias(e, v->as.name.name) == LOCAL_VLA)
                        fprintf(e->out, "load %u\n", s);
                    else
                        fprintf(e->out, "laddr __local_array_address_%u\n", s);
                }
                else
                    emit_local_addr(e, v->as.name.name, s);
                return 1;
            }
            if (v && v->kind == EXPR_NAME) {
                Decl *g = find_global(e->program, v->as.name.name);
                if (g && (isnum(g->as.variable.type) || is_value_pointer(g->as.variable.type) ||
                          (g->as.variable.type && g->as.variable.type->kind == TYPE_LONG_DOUBLE) ||
                     (g->as.variable.type &&
                      (g->as.variable.type->kind == TYPE_STRUCT ||
                       g->as.variable.type->kind == TYPE_ARRAY)))) {
                    fprintf(e->out, "gaddr %s\n", v->as.name.name);
                    return 1;
                }
            }
            if (v && v->kind == EXPR_MEMBER) {
                const Type *ft = expr_type_raw(e, v);
                char key[512], gkey[512], root[512];
                if (ft && ft->kind == TYPE_STRUCT) {
                    size_t offset;
                    Expr base_address = {0};
                    base_address.kind = EXPR_UNARY;
                    base_address.as.unary.op = UNARY_ADDRESS;
                    base_address.as.unary.operand = v->as.member.base;
                    if (!struct_field_offset(e->program, member_base_type(e, v),
                                             v->as.member.name, &offset, NULL) ||
                        !expression(e, &base_address)) {
                        fail(e, "cannot address structure member");
                        return 0;
                    }
                    if (offset)
                        fprintf(e->out, "push %zu\nptradd byte\n", offset);
                    return 1;
                }
                if (!ft || (!scalar_field(ft) && ft->kind != TYPE_ARRAY) ||
                    !field_storage_key(v, key, sizeof key, 0)) {
                    fail(e, "address-of member requires a scalar field");
                    return 0;
                }
                if (lookup(e, key, &s, &t)) {
                    emit_local_addr(e, key, s);
                    return 1;
                }
                if (!field_storage_key(v, gkey, sizeof gkey, 1) ||
                    !member_path(v, root, sizeof root, 1)) {
                    fail(e, "member name too long");
                    return 0;
                }
                char *sep = strstr(root, "__");
                if (sep) {
                    *sep = '\0';
                    Decl *g = find_global(e->program, root);
                    if (g && g->as.variable.type && g->as.variable.type->kind == TYPE_STRUCT) {
                        fprintf(e->out, "gaddr %s\n", gkey);
                        return 1;
                    }
                }
            }
            fail(e, "address-of requires a supported variable or structure member");
            return 0;
        }
        if (op == UNARY_DEREFERENCE) {
            const Type *pt = expr_type(e, v);
            if (pt && pt->kind == TYPE_POINTER && pt->pointee &&
                pt->pointee->kind == TYPE_FUNCTION)
                return expression(e, v);
            if (pt && pt->kind == TYPE_POINTER && pt->pointee &&
                (pt->pointee->kind == TYPE_STRUCT || pt->pointee->kind == TYPE_ARRAY))
                return expression(e, v);
            if (!pt || pt->kind != TYPE_POINTER ||
                (!isnum(pt->pointee) && !is_value_pointer(pt->pointee))) {
                fail(e, "dereference requires a pointer to a supported scalar");
                return 0;
            }
            if (!expression(e, v))
                return 0;
            fprintf(e->out, "loadind %s\n", tn(pt->pointee));
            return 1;
        }
        if (op == UNARY_PRE_INCREMENT || op == UNARY_PRE_DECREMENT || op == UNARY_POST_INCREMENT ||
            op == UNARY_POST_DECREMENT) {
            FieldLayout layout;
            const Decl *field;
            if (bit_field_info(e, v, &layout, &field)) {
                if (!emit_bit_field_address(e, v, &layout))
                    return 0;
                if (!emit_bit_field_update(e, field->as.variable.type, layout.bit_offset,
                                           layout.bit_width, NULL, ASSIGN_SIMPLE,
                                           op == UNARY_PRE_DECREMENT || op == UNARY_POST_DECREMENT
                                               ? -1 : 1,
                                           op == UNARY_POST_INCREMENT || op == UNARY_POST_DECREMENT))
                    return 0;
                emit_bit_field_promotion(e, x, field, &layout);
                return 1;
            }
            const Type *value_type = expr_type(e, v);
            if (v && value_type && value_type->kind == TYPE_POINTER &&
                (v->kind == EXPR_MEMBER ||
                 v->kind == EXPR_INDEX ||
                 (v->kind == EXPR_CAST && v->as.cast.operand &&
                  v->as.cast.operand->kind == EXPR_ARRAY_INIT) ||
                 (v->kind == EXPR_UNARY && v->as.unary.op == UNARY_DEREFERENCE))) {
                const Type *pointer_type = value_type;
                int post = op == UNARY_POST_INCREMENT || op == UNARY_POST_DECREMENT;
                int subtract = op == UNARY_PRE_DECREMENT || op == UNARY_POST_DECREMENT;
                if (!emit_pointer_lvalue_address(e, v))
                    return 0;
                fputs("dup\nloadind ptr\n", e->out);
                fputs("push 1\n", e->out);
                if (!emit_pointer_offset(e, pointer_type, subtract))
                    return 0;
                fputs("storeind ptr\n", e->out);
                if (post) {
                    /* Recover the old pointer from the value just stored. */
                    fputs("push 1\n", e->out);
                    if (!emit_pointer_offset(e, pointer_type, !subtract))
                        return 0;
                }
                return 1;
            }
            if (v && isnum(value_type) &&
                (v->kind != EXPR_NAME || !lookup(e, v->as.name.name, &s, &t)))
                return emit_indirect_numeric_step(e, v, value_type, op);
            if (!v || v->kind != EXPR_NAME) {
                fail(e, "increment/decrement requires a local variable or global pointer");
                return 0;
            }
            int local = lookup(e, v->as.name.name, &s, &t);
            Decl *global = local ? NULL : find_global(e->program, v->as.name.name);
            if (!local && (!global || !is_supported_array(global->as.variable.type))) {
                fail(e, "increment/decrement requires a local variable or global pointer");
                return 0;
            }
            if (global)
                t = global->as.variable.type;
            if (global)
                fprintf(e->out, "gload %s ptr\n", global->as.variable.name);
            else
                emit_local_load(e, v->as.name.name, s, t);
            if (op == UNARY_POST_INCREMENT || op == UNARY_POST_DECREMENT)
                fprintf(e->out, "dup\n");
            if (t->kind == TYPE_POINTER) {
                fputs("push 1\n", e->out);
                if (!emit_pointer_offset(e, t, op == UNARY_PRE_DECREMENT ||
                                               op == UNARY_POST_DECREMENT))
                    return 0;
            } else {
                emit_numeric_step(e, t, op == UNARY_PRE_DECREMENT || op == UNARY_POST_DECREMENT);
            }
            if (op == UNARY_PRE_INCREMENT || op == UNARY_PRE_DECREMENT)
                fprintf(e->out, "dup\n");
            if (global)
                fprintf(e->out, "gstore %s ptr\n", global->as.variable.name);
            else
                emit_local_store(e, v->as.name.name, s, t);
            return 1;
        }
        if ((op == UNARY_PLUS || op == UNARY_MINUS || op == UNARY_BITWISE_NOT) &&
            expr_type(e, v) &&
            (promoted_small_kind(expr_type(e, v)->kind) ||
             (e->program->c11_numeric && expr_type(e, v)->kind == TYPE_BYTE))) {
            if (!expression_as(e, v, &TYPE_INT_VALUE))
                return 0;
        } else if (!expression(e, v))
            return 0;
        switch (op) {
        case UNARY_MINUS:
            fprintf(e->out, "neg %s\n", tn(expr_type(e, x)));
            break;
        case UNARY_PLUS:
            break;
        case UNARY_LOGICAL_NOT:
            if (expr_type(e, v) && expr_type(e, v)->kind == TYPE_LONG_DOUBLE)
                fputs("extbool\nnot int\n", e->out);
            else
                fprintf(e->out, "not %s\n", tn(expr_type(e, v)));
            break;
        case UNARY_BITWISE_NOT:
            fprintf(e->out, "bitnot %s\n", tn(expr_type(e, x)));
            break;
        default:
            fail(e, "unary operator not yet supported by Crunch IR");
            return 0;
        }
        return 1;
    }
    case EXPR_BINARY: {
        BinaryOp op = x->as.binary.op;
        if (op == BINARY_COMMA) {
            const Type *right = expr_type(e, x->as.binary.right);
            if (!emit_discarded_expression(e, x->as.binary.left, 0))
                return 0;
            return right && right->kind == TYPE_STRUCT
                       ? emit_aggregate_address(e, x->as.binary.right, 0)
                       : expression(e, x->as.binary.right);
        }
        if (op == BINARY_LOGICAL_AND || op == BINARY_LOGICAL_OR) {
            unsigned sh = label(e), end = label(e);
            if (!expression(e, x->as.binary.left))
                return 0;
            emit_truth(e, expr_type(e, x->as.binary.left));
            fprintf(e->out, "%s L%u\n", op == BINARY_LOGICAL_AND ? "jz" : "jnz", sh);
            if (!expression(e, x->as.binary.right))
                return 0;
            if (e->typed_ir) {
                emit_truth(e, expr_type(e, x->as.binary.right));
                fprintf(e->out, "jmp L%u\n", end);
            } else
                fprintf(e->out, "bool\njmp L%u\n", end);
            mark(e, sh);
            fprintf(e->out, "push %d\n", op == BINARY_LOGICAL_OR ? 1 : 0);
            mark(e, end);
            return 1;
        }
        const Type *l = expr_type(e, x->as.binary.left), *r = expr_type(e, x->as.binary.right);
        if (op == BINARY_POWER) {
            if (!expression_as(e, x->as.binary.left, &TYPE_REAL_VALUE) ||
                !expression_as(e, x->as.binary.right, &TYPE_REAL_VALUE))
                return 0;
            fprintf(e->out, "pow real\n");
            return 1;
        }
        if (op == BINARY_SUBTRACT && l && r &&
            l->kind == TYPE_POINTER && r->kind == TYPE_POINTER) {
            unsigned size_slot;
            if (vla_size_slot(e, l->pointee, &size_slot)) {
                if (!expression(e, x->as.binary.left) || !expression(e, x->as.binary.right))
                    return 0;
                fprintf(e->out, "ptrdiff\nload %u\ndiv long\n", size_slot);
                return 1;
            }
            size_t stride;
            if (!array_element_size(e->program, l, &stride) || !stride || stride > INT64_MAX) {
                fail(e, "pointer subtraction requires a supported complete object layout");
                return 0;
            }
            if (!expression(e, x->as.binary.left) || !expression(e, x->as.binary.right))
                return 0;
            fputs("ptrdiff\n", e->out);
            if (stride != 1)
                fprintf(e->out, "push %zu\ndiv long\n", stride);
            return 1;
        }
        if ((op == BINARY_ADD || op == BINARY_SUBTRACT) &&
            ((l && is_supported_array(l) && r && isnum(r) &&
              r->kind != TYPE_REAL && r->kind != TYPE_FLOAT) ||
             (op == BINARY_ADD && r && is_supported_array(r) && l &&
              isnum(l) && l->kind != TYPE_REAL && l->kind != TYPE_FLOAT))) {
            const Type *pt = l && l->kind == TYPE_POINTER ? l : r;
            const Expr *pe = l && l->kind == TYPE_POINTER ? x->as.binary.left : x->as.binary.right;
            const Expr *ie = pe == x->as.binary.left ? x->as.binary.right : x->as.binary.left;
            if (!expression(e, pe) ||
                !((pt->pointee->kind == TYPE_STRUCT || pt->pointee->kind == TYPE_ARRAY)
                      ? expression_as(e, ie, &TYPE_LONG_VALUE)
                      : expression(e, ie)))
                return 0;
            return emit_pointer_offset(e, pt, op == BINARY_SUBTRACT);
        }
        if ((op == BINARY_EQUAL || op == BINARY_NOT_EQUAL) && l && r &&
            ((l->kind == TYPE_POINTER && is_null_literal(x->as.binary.right)) ||
             (r->kind == TYPE_POINTER && is_null_literal(x->as.binary.left)))) {
            const Expr *pointer_value =
                is_null_literal(x->as.binary.left) ? x->as.binary.right : x->as.binary.left;
            if (is_null_literal(x->as.binary.left) && is_null_literal(x->as.binary.right))
                pointer_value = x->as.binary.left;
            if (!expression(e, pointer_value))
                return 0;
            fprintf(e->out, "push 0\n%s ptr\n", op == BINARY_EQUAL ? "eq" : "ne");
            return 1;
        }
        if ((op >= BINARY_EQUAL && op <= BINARY_GREATER_EQUAL) && l && r &&
            l->kind == TYPE_POINTER && r->kind == TYPE_POINTER) {
            if (!expression(e, x->as.binary.left) || !expression(e, x->as.binary.right))
                return 0;
            const char *po = op == BINARY_EQUAL        ? "eq"
                             : op == BINARY_NOT_EQUAL  ? "ne"
                             : op == BINARY_LESS       ? "lt"
                             : op == BINARY_LESS_EQUAL ? "le"
                             : op == BINARY_GREATER    ? "gt"
                                                       : "ge";
            fprintf(e->out, "%s ptr\n", po);
            return 1;
        }
        int unsigned_shift = (op == BINARY_SHIFT_LEFT || op == BINARY_SHIFT_RIGHT) &&
            l && r && (e->program->c11_numeric || l->kind == TYPE_UNSIGNED_INT || l->kind == TYPE_UNSIGNED_LONG ||
                       r->kind == TYPE_UNSIGNED_INT || r->kind == TYPE_UNSIGNED_LONG ||
                       promoted_small_kind(l->kind) || promoted_small_kind(r->kind));
        const Type *common = unsigned_shift ? (l->kind == TYPE_ENUM
                                                   ? (type_effective_kind(l) == TYPE_UNSIGNED_INT
                                                          ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE)
                                                   : l->kind == TYPE_BYTE || promoted_small_kind(l->kind)
                                                         ? &TYPE_INT_VALUE : l)
                                           : promote(l, r, e->program->c11_numeric);
        int cmp = op >= BINARY_EQUAL && op <= BINARY_GREATER_EQUAL;
        if (!common) {
            fail(e, "cannot infer numeric operation type");
            return 0;
        }
        if (common->kind == TYPE_LONG_DOUBLE) {
            const char *extended_op =
                op == BINARY_ADD ? "extadd" : op == BINARY_SUBTRACT ? "extsub" :
                op == BINARY_MULTIPLY ? "extmul" : op == BINARY_DIVIDE ? "extdiv" :
                op == BINARY_EQUAL ? "exteq" : op == BINARY_NOT_EQUAL ? "extne" :
                op == BINARY_LESS ? "extlt" : op == BINARY_LESS_EQUAL ? "extle" :
                op == BINARY_GREATER ? "extgt" : op == BINARY_GREATER_EQUAL ? "extge" : NULL;
            if (!extended_op) {
                fail(e, "unsupported long double binary operation");
                return 0;
            }
            if ((!cmp && !emit_extended_temporary(e)) ||
                !expression_as(e, x->as.binary.left, common) ||
                !expression_as(e, x->as.binary.right, common))
                return 0;
            fprintf(e->out, "%s\n", extended_op);
            return 1;
        }
        if (!expression_as(e, x->as.binary.left, common) ||
            !(unsigned_shift ? expression(e, x->as.binary.right)
                             : expression_as(e, x->as.binary.right, common)))
            return 0;
        switch (op) {
        case BINARY_ADD:
            fprintf(e->out, "add %s\n", tn(common));
            break;
        case BINARY_SUBTRACT:
            fprintf(e->out, "sub %s\n", tn(common));
            break;
        case BINARY_MULTIPLY:
            fprintf(e->out, "mul %s\n", tn(common));
            break;
        case BINARY_DIVIDE:
            fprintf(e->out, "div %s\n", tn(common));
            break;
        case BINARY_REMAINDER:
            fprintf(e->out, "rem %s\n", tn(common));
            break;
        case BINARY_EQUAL:
            fprintf(e->out, "eq %s\n", tn(common));
            break;
        case BINARY_NOT_EQUAL:
            fprintf(e->out, "ne %s\n", tn(common));
            break;
        case BINARY_LESS:
            fprintf(e->out, "lt %s\n", tn(common));
            break;
        case BINARY_LESS_EQUAL:
            fprintf(e->out, "le %s\n", tn(common));
            break;
        case BINARY_GREATER:
            fprintf(e->out, "gt %s\n", tn(common));
            break;
        case BINARY_GREATER_EQUAL:
            fprintf(e->out, "ge %s\n", tn(common));
            break;
        case BINARY_BITWISE_AND:
            fprintf(e->out, "band %s\n", tn(common));
            break;
        case BINARY_BITWISE_OR:
            fprintf(e->out, "bor %s\n", tn(common));
            break;
        case BINARY_BITWISE_XOR:
            fprintf(e->out, "bxor %s\n", tn(common));
            break;
        case BINARY_SHIFT_LEFT:
            fprintf(e->out, "shl %s\n", tn(common));
            break;
        case BINARY_SHIFT_RIGHT:
            fprintf(e->out, "shr %s\n", tn(common));
            break;
        default:
            fail(e, "unsupported binary operation");
            return 0;
        }
        (void)cmp;
        return 1;
    }
    case EXPR_INDEX: {
        const Type *pt = expr_type(e, x->as.index.base);
        if (pt && pt->kind == TYPE_POINTER && pt->pointee && pt->pointee->kind == TYPE_STRUCT) {
            if (!emit_indirect_address(e, x)) {
                fail(e, "invalid structure-array index");
                return 0;
            }
            return 1;
        }
        if (pt && pt->kind == TYPE_POINTER && pt->pointee && pt->pointee->kind == TYPE_ARRAY)
            return emit_index_address(e, x);
        if (!pt || pt->kind != TYPE_POINTER ||
            (!isnum(pt->pointee) && (!pt->pointee || pt->pointee->kind != TYPE_POINTER))) {
            fail(e, "indexing requires a supported pointer element type");
            return 0;
        }
        if (!emit_index_address(e, x))
            return 0;
        fprintf(e->out, "loadind %s\n", tn(pt->pointee));
        return 1;
    }
    case EXPR_CALL: {
        const Expr *c = x->as.call.callee;
        if (x->semantic_builtin_alloca) {
            if (x->as.call.arguments.count != 1 ||
                !expression_as(e, x->as.call.arguments.items[0], &TYPE_ULONG_VALUE))
                return 0;
            fputs("stackalloc_live\n", e->out);
            return 1;
        }
        if (!c || c->kind != EXPR_NAME ||
            lookup(e, c->as.name.name, &s, NULL) ||
            find_global(e->program, c->as.name.name))
            return emit_indirect_call(e, x);
        Decl *fn = find_function(e->program, c->as.name.name);
        const char *bn = c->as.name.name;
        if (!fn && (!strcmp(bn, "__crunch_va_start") || !strcmp(bn, "__builtin_va_start"))) {
            if (x->as.call.arguments.count != 2 ||
                !expression(e, x->as.call.arguments.items[0]))
                return 0;
            fputs("vastart\npush 0\n", e->out);
            return 1;
        }
        if (!fn && (!strcmp(bn, "__crunch_va_arg") || !strcmp(bn, "__builtin_va_arg"))) {
            if (x->as.call.arguments.count != 2)
                return 0;
            const Type *marker = expr_type(e, x->as.call.arguments.items[1]);
            if (!marker || marker->kind != TYPE_POINTER || !marker->pointee)
                return 0;
            if (marker->pointee->kind == TYPE_STRUCT) {
                unsigned classes[2];
                size_t size = aggregate_parameter_class(e->program, marker->pointee, classes);
                if (!size) {
                    fail(e, "va_arg aggregate has an unsupported ABI layout");
                    return 0;
                }
                if (!emit_aggregate_result_buffer(e, size))
                    return 0;
                if (classes[0] == 3)
                    fputs("dup\n", e->out);
                if (!expression(e, x->as.call.arguments.items[0]))
                    return 0;
                if (classes[0] == 3) {
                    fputs("vaarg ext\n", e->out);
                    return emit_fixed_array_copy(e, marker->pointee);
                }
                fputs("vaargagg ", e->out);
                emit_parameter_type(e->out, e->program, marker->pointee);
                fputc('\n', e->out);
                return 1;
            }
            if (!expression(e, x->as.call.arguments.items[0]))
                return 0;
            fprintf(e->out, "vaarg %s\n", marker->pointee->kind == TYPE_REAL ? "fp" :
                    marker->pointee->kind == TYPE_LONG_DOUBLE ? "ext" : "gp");
            return 1;
        }
        if (!fn && (!strcmp(bn, "__crunch_va_end") || !strcmp(bn, "__builtin_va_end"))) {
            if (x->as.call.arguments.count != 1 || !expression(e, x->as.call.arguments.items[0]))
                return 0;
            fputs("drop\npush 0\n", e->out);
            return 1;
        }
        if (!fn && (!strcmp(bn, "__crunch_va_copy") || !strcmp(bn, "__builtin_va_copy"))) {
            if (x->as.call.arguments.count != 2 || !expression(e, x->as.call.arguments.items[0]) ||
                !expression(e, x->as.call.arguments.items[1]))
                return 0;
            fputs("vacopy\npush 0\n", e->out);
            return 1;
        }
        if (!fn && (!strcmp(bn, "copy") || !strcmp(bn, "fill") || !strcmp(bn, "compare"))) {
            size_t expected = 3;
            if (x->as.call.arguments.count != expected) {
                fail(e, "built-in '%s' expects %zu argument%s", bn, expected,
                     expected == 1 ? "" : "s");
                return 0;
            }
            for (size_t i = 0; i < expected; i++) {
                const Type *at = expr_type(e, x->as.call.arguments.items[i]);
                if (i == 0 || (!strcmp(bn, "copy") && i == 1) ||
                    (!strcmp(bn, "compare") && i == 1)) {
                    if ((!at || at->kind != TYPE_POINTER) &&
                        !is_null_literal(x->as.call.arguments.items[i])) {
                        fail(e, "argument %zu to built-in '%s' must be a pointer", i + 1, bn);
                        return 0;
                    }
                } else if (i == 1 && (!isnum(at) || at->kind == TYPE_REAL ||
                                      at->kind == TYPE_FLOAT)) {
                    fail(e, "fill value must be an integer type");
                    return 0;
                } else if (i == 2 && (!isnum(at) || at->kind == TYPE_REAL ||
                                      at->kind == TYPE_FLOAT)) {
                    fail(e, "byte count to built-in '%s' must be an integer type", bn);
                    return 0;
                }
                if (!expression(e, x->as.call.arguments.items[i]))
                    return 0;
                if (i == 2 && at) {
                    TypeKind kind = type_effective_kind(at);
                    if (kind == TYPE_SIGNED_CHAR || kind == TYPE_SHORT || kind == TYPE_INT ||
                        kind == TYPE_LONG || kind == TYPE_LONG_LONG) {
                        unsigned nonnegative = label(e);
                        if (!cast_emit(e, at, &TYPE_LONG_VALUE))
                            return 0;
                        fprintf(e->out, "dup\npush 0\nlt long\njz L%u\n"
                                        "drop\npush 0\ncast int long\n", nonnegative);
                        mark(e, nonnegative);
                    }
                }
            }
            const char *target = !strcmp(bn, "copy")    ? "memcpy"
                                 : !strcmp(bn, "fill")    ? "memset"
                                 : !strcmp(bn, "compare") ? "memcmp"
                                                          : bn;
            fprintf(e->out, "%s %s %zu\n", e->module ? "runtime_call" : "call", target, expected);
            return 1;
        }
        if (e->module && crunch_is_runtime_declaration(fn) && !strcmp(bn, "vfork")) {
            unsigned user_call = label(e), done = label(e);
            fprintf(e->out, "fnaddr vfork\nweak_fnaddr __crunch_fallback_vfork\neq ptr\njz L%u\n"
                            "runtime_call vfork 0\njmp L%u\n", user_call, done);
            mark(e, user_call);
            fputs("call vfork 0\n", e->out);
            mark(e, done);
            return 1;
        }
        if (crunch_is_runtime_declaration(fn) &&
            (!strcmp(bn, "printf") || !strcmp(bn, "fprintf") || !strcmp(bn, "scanf") ||
             !strcmp(bn, "fscanf") || !strcmp(bn, "sprintf") || !strcmp(bn, "snprintf"))) {
            size_t fixed = !strcmp(bn, "snprintf") ? 3 :
                           (!strcmp(bn, "fprintf") || !strcmp(bn, "fscanf") || !strcmp(bn, "sprintf")) ? 2 : 1;
            unsigned user_call = 0, done = 0;
            if (e->module) {
                user_call = label(e);
                done = label(e);
                fprintf(e->out, "fnaddr %s\nweak_fnaddr __crunch_fallback_%s\neq ptr\njz L%u\n",
                        bn, bn, user_call);
            }
            for (size_t i = 0; i < x->as.call.arguments.count; ++i) {
                const Expr *arg = x->as.call.arguments.items[i];
                const Type *t = expr_type(e, arg);
                if (!(fixed == 3 && i == 1
                          ? expression_as(e, arg, fn->as.function.parameters.items[1].type)
                      : i >= fixed && t && t->kind == TYPE_FLOAT
                          ? expression_as(e, arg, &TYPE_REAL_VALUE) : expression(e, arg)))
                    return 0;
                if (i >= fixed)
                    fprintf(e->out, "push %d\n", (!strcmp(bn, "scanf") || !strcmp(bn, "fscanf")) ? 4 :
                            t->kind == TYPE_LONG_DOUBLE ? 7 :
                            t->kind == TYPE_REAL || t->kind == TYPE_FLOAT ? 3 :
                            t->kind == TYPE_POINTER ? 4 :
                            t->kind == TYPE_LONG || t->kind == TYPE_LONG_LONG ? 2 :
                            type_effective_kind(t) == TYPE_UNSIGNED_INT ? 5 :
                            t->kind == TYPE_UNSIGNED_LONG || t->kind == TYPE_UNSIGNED_LONG_LONG ? 6 : 1);
            }
            fprintf(e->out, "%s %s %zu\n", e->module ? "runtime_call" : "call", bn,
                    fixed + 2 * (x->as.call.arguments.count - fixed));
            if (e->module) {
                fprintf(e->out, "jmp L%u\n", done);
                mark(e, user_call);
                if (!emit_external_formatted_call(e, x, fn))
                    return 0;
                mark(e, done);
            }
            return 1;
        }
        if (fn && fn->as.function.variadic) {
            if (!e->module && !fn->as.function.body) {
                fail(e, "external variadic calls require module compilation");
                return 0;
            }
            return emit_indirect_call(e, x);
        }
        if (!fn || fn->as.function.parameters.count != x->as.call.arguments.count) {
            fail(e, "unknown function or wrong argument count calling '%s'", c->as.name.name);
            return 0;
        }
        if (!e->module && !strcmp(bn, "print") && !fn->as.function.body) {
            fail(e, "unknown function '%s'", bn);
            return 0;
        }
        if (x->as.call.arguments.count > 128) {
            fail(e, "function call has too many arguments");
            return 0;
        }
        int aggregate_result = fn->as.function.return_type &&
                               (fn->as.function.return_type->kind == TYPE_STRUCT ||
                                fn->as.function.return_type->kind == TYPE_LONG_DOUBLE);
        if (aggregate_result) {
            if (fn->as.function.return_type->kind == TYPE_STRUCT
                    ? !emit_aggregate_result_buffer(e, aggregate_parameter_size(e->program,
                                                       fn->as.function.return_type))
                    : !emit_extended_temporary(e))
                return 0;
        }
        for (size_t i = 0; i < x->as.call.arguments.count; i++) {
            const Type *parameter = fn->as.function.parameters.items[i].type;
            if (parameter && parameter->kind == TYPE_STRUCT
                    ? !emit_aggregate_argument(e, x->as.call.arguments.items[i],
                                               aggregate_parameter_size(e->program, parameter))
                    : !expression_as(e, x->as.call.arguments.items[i], parameter))
                return 0;
        }
        fprintf(e->out, "%s %s %zu\n", aggregate_result ? "callagg" :
                e->module && module_runtime_declaration(fn)
                    ? "runtime_call" : "call",
                c->as.name.name, x->as.call.arguments.count);
        /* These runtime calls retain signed result cells in the IR contract. */
        if ((!e->module || module_runtime_declaration(fn)) &&
            crunch_is_runtime_declaration(fn) &&
            fn->as.function.return_type->kind == TYPE_UNSIGNED_LONG &&
            (!strcmp(bn, "strlen") || !strcmp(bn, "fread") || !strcmp(bn, "fwrite")))
            fputs("cast long ulong\n", e->out);
        return 1;
    }
    case EXPR_ASSIGN: {
        const Expr *target = x->as.assign.target;
        FieldLayout layout;
        const Decl *field;
        if (bit_field_info(e, target, &layout, &field)) {
            if (!emit_bit_field_address(e, target, &layout))
                return 0;
            if (!emit_bit_field_update(e, field->as.variable.type, layout.bit_offset,
                                       layout.bit_width, x->as.assign.value, x->as.assign.op,
                                       0, 0))
                return 0;
            emit_bit_field_promotion(e, x, field, &layout);
            return 1;
        }
        const Type *target_type = expr_type(e, target);
        if (target_type && target_type->kind == TYPE_STRUCT) {
            if (x->as.assign.op != ASSIGN_SIMPLE || expr_type(e, x->as.assign.value) == NULL ||
                expr_type(e, x->as.assign.value)->kind != TYPE_STRUCT) {
                fail(e, "whole-structure assignment requires matching structure values");
                return 0;
            }
            if (!emit_struct_assignment(e, target, x->as.assign.value, target_type)) {
                fail(e, "whole-structure assignment currently requires named structure variables "
                        "or members");
                return 0;
            }
            return 1;
        }
        if (target && target_type && target_type->kind == TYPE_POINTER &&
            (target->kind == EXPR_MEMBER || target->kind == EXPR_INDEX ||
             (target->kind == EXPR_CAST && target->as.cast.operand &&
              target->as.cast.operand->kind == EXPR_ARRAY_INIT) ||
             (target->kind == EXPR_UNARY && target->as.unary.op == UNARY_DEREFERENCE)) &&
            (x->as.assign.op == ASSIGN_ADD || x->as.assign.op == ASSIGN_SUBTRACT)) {
            if (!emit_pointer_lvalue_address(e, target))
                return 0;
            fputs("dup\nloadind ptr\n", e->out);
            if (!(target_type->pointee && target_type->pointee->kind == TYPE_STRUCT
                      ? expression_as(e, x->as.assign.value, &TYPE_LONG_VALUE)
                      : expression(e, x->as.assign.value)) ||
                !emit_pointer_offset(e, target_type, x->as.assign.op == ASSIGN_SUBTRACT))
                return 0;
            fputs("storeind ptr\n", e->out);
            return 1;
        }
        if (target && target->kind == EXPR_UNARY && target->as.unary.op == UNARY_DEREFERENCE) {
            const Type *pt = expr_type(e, target->as.unary.operand);
            if (!pt || pt->kind != TYPE_POINTER ||
                (!isnum(pt->pointee) && !is_value_pointer(pt->pointee))) {
                fail(e, "indirect assignment requires a supported scalar value");
                return 0;
            }
            if (!expression(e, target->as.unary.operand))
                return 0;
            return emit_indirect_assignment(e, pt->pointee, x->as.assign.value, x->as.assign.op);
        }
        if (target && target->kind == EXPR_CAST && target->as.cast.operand &&
            target->as.cast.operand->kind == EXPR_ARRAY_INIT) {
            if (!emit_compound_literal(e, target))
                return 0;
            return emit_indirect_assignment(e, target_type, x->as.assign.value, x->as.assign.op);
        }
        if (target && target->kind == EXPR_INDEX) {
            const Type *pt = expr_type(e, target->as.index.base);
            if (!pt || pt->kind != TYPE_POINTER ||
                (!isnum(pt->pointee) && !is_value_pointer(pt->pointee))) {
                fail(e, "indexed assignment requires a supported scalar element");
                return 0;
            }
            if (!emit_index_address(e, target))
                return 0;
            return emit_indirect_assignment(e, pt->pointee, x->as.assign.value, x->as.assign.op);
        }
        if (is_indirect_member(e, target)) {
            const Type *ft = expr_type(e, target);
            if (!ft || ft->kind == TYPE_STRUCT || !emit_indirect_address(e, target)) {
                fail(e, "unsupported indirect structure member assignment");
                return 0;
            }
            return emit_indirect_assignment(e, ft, x->as.assign.value, x->as.assign.op);
        }
        if (target && target->kind == EXPR_MEMBER) {
            const Type *ft = expr_type(e, target);
            if (!ft || !scalar_field(ft)) {
                fail(e, "unsupported structure member assignment");
                return 0;
            }
            if (x->as.assign.op != ASSIGN_SIMPLE) {
                if (!isnum(ft)) {
                    fail(e, "structure member compound assignment requires a numeric field");
                    return 0;
                }
                if (!expression(e, target) ||
                    !emit_compound_numeric(e, ft, x->as.assign.value, x->as.assign.op))
                    return 0;
            } else if (!expression_as(e, x->as.assign.value, ft)) {
                return 0;
            }
            char key[512], gkey[512], root[512];
            if (!field_storage_key(target, key, sizeof key, 0)) {
                fail(e, "unsupported member target");
                return 0;
            }
            if (lookup(e, key, &s, &t)) {
                fprintf(e->out, "dup\n");
                emit_local_store(e, key, s, t);
                return 1;
            }
            if (!field_storage_key(target, gkey, sizeof gkey, 1) ||
                !member_path(target, root, sizeof root, 1)) {
                fail(e, "member name too long");
                return 0;
            }
            char *sep = strstr(root, "__");
            if (sep) {
                *sep = '\0';
                Decl *g = find_global(e->program, root);
                if (g && g->as.variable.type && g->as.variable.type->kind == TYPE_STRUCT) {
                    fprintf(e->out, "dup\ngstore %s %s\n", gkey, tn(ft));
                    return 1;
                }
            }
            fail(e, "member storage not found");
            return 0;
        }
        if (target && target->kind == EXPR_NAME && !lookup(e, target->as.name.name, &s, &t)) {
            Decl *g = find_global(e->program, target->as.name.name);
            if (!g) {
                fail(e, "assignment target must be a known variable or supported pointer lvalue");
                return 0;
            }
            t = g->as.variable.type;
            if (x->as.assign.op != ASSIGN_SIMPLE) {
                if (is_supported_array(t) &&
                    (x->as.assign.op == ASSIGN_ADD || x->as.assign.op == ASSIGN_SUBTRACT)) {
                    fprintf(e->out, "gload %s ptr\n", g->as.variable.name);
                    if (!(t->pointee->kind == TYPE_STRUCT
                              ? expression_as(e, x->as.assign.value, &TYPE_LONG_VALUE)
                              : expression(e, x->as.assign.value)) ||
                        !emit_pointer_offset(e, t, x->as.assign.op == ASSIGN_SUBTRACT))
                        return 0;
                    fprintf(e->out, "dup\ngstore %s ptr\n", g->as.variable.name);
                    return 1;
                }
                if (!isnum(t)) {
                    fail(e, "global compound assignment requires a numeric variable");
                    return 0;
                }
                fprintf(e->out, "gload %s %s\n", g->as.variable.name, tn(t));
                if (!emit_compound_numeric(e, t, x->as.assign.value, x->as.assign.op))
                    return 0;
            } else if (!expression_as(e, x->as.assign.value, t)) {
                return 0;
            }
            fprintf(e->out, "dup\ngstore %s %s\n", g->as.variable.name, tn(t));
            return 1;
        }
        if (!target || target->kind != EXPR_NAME || !lookup(e, target->as.name.name, &s, &t)) {
            fail(e, "assignment target must be a local variable or supported pointer lvalue");
            return 0;
        }
        if (t->kind == TYPE_POINTER && x->as.assign.op != ASSIGN_SIMPLE) {
            if ((x->as.assign.op != ASSIGN_ADD && x->as.assign.op != ASSIGN_SUBTRACT) ||
                !isnum(expr_type(e, x->as.assign.value)) ||
                (expr_type(e, x->as.assign.value)->kind == TYPE_REAL ||
                 expr_type(e, x->as.assign.value)->kind == TYPE_FLOAT)) {
                fail(e, "pointer compound assignment supports += and -= with integer operands");
                return 0;
            }
            emit_local_load(e, target->as.name.name, s, t);
            if (!(t->pointee && t->pointee->kind == TYPE_STRUCT
                      ? expression_as(e, x->as.assign.value, &TYPE_LONG_VALUE)
                      : expression(e, x->as.assign.value)) ||
                !emit_pointer_offset(e, t, x->as.assign.op == ASSIGN_SUBTRACT))
                return 0;
            fputs("dup\n", e->out);
            emit_local_store(e, target->as.name.name, s, t);
            return 1;
        }
        if (x->as.assign.op != ASSIGN_SIMPLE) {
            emit_local_load(e, target->as.name.name, s, t);
            if (!emit_compound_numeric(e, t, x->as.assign.value, x->as.assign.op))
                return 0;
        } else if (!expression_as(e, x->as.assign.value, t))
            return 0;
        fputs("dup\n", e->out);
        emit_local_store(e, target->as.name.name, s, t);
        return 1;
    }
    case EXPR_CONDITIONAL: {
        unsigned other = label(e), end = label(e);
        const Type *t = expr_type(e, x);
        if (!expression(e, x->as.conditional.condition))
            return 0;
        emit_truth(e, expr_type(e, x->as.conditional.condition));
        fprintf(e->out, "jz L%u\n", other);
        if (!expression_as(e, x->as.conditional.then_expr, t))
            return 0;
        fprintf(e->out, "jmp L%u\n", end);
        mark(e, other);
        if (!expression_as(e, x->as.conditional.else_expr, t))
            return 0;
        mark(e, end);
        return 1;
    }
    case EXPR_CAST: {
        const Type *from = expr_type(e, x->as.cast.operand);
        if (is_indirect_structure(e, x) ||
            (x->as.cast.target_type && x->as.cast.target_type->kind == TYPE_ARRAY &&
             x->as.cast.operand && x->as.cast.operand->kind == EXPR_ARRAY_INIT))
            return emit_compound_literal(e, x);
        if (x->as.cast.operand && x->as.cast.operand->kind == EXPR_ARRAY_INIT) {
            if (!emit_compound_literal(e, x))
                return 0;
            fprintf(e->out, "loadind %s\n", tn(x->as.cast.target_type));
            return 1;
        }
        if (x->as.cast.target_type && x->as.cast.target_type->kind == TYPE_VOID) {
            if (!emit_discarded_expression(e, x->as.cast.operand, 0))
                return 0;
            fputs("push 0\n", e->out);
            return 1;
        }
        if (x->as.cast.target_type && x->as.cast.target_type->kind == TYPE_POINTER &&
            is_null_literal(x->as.cast.operand)) {
            fputs("push 0\n", e->out);
            return 1;
        }
        if (x->as.cast.target_type && x->as.cast.target_type->kind == TYPE_POINTER &&
            from && isnum(from) && from->kind != TYPE_REAL && from->kind != TYPE_FLOAT &&
            from->kind != TYPE_LONG_DOUBLE) {
            fputs("push 0\n", e->out);
            if (!expression_as(e, x->as.cast.operand, &TYPE_LONG_VALUE))
                return 0;
            fputs("ptradd byte\n", e->out);
            return 1;
        }
        if (!expression(e, x->as.cast.operand))
            return 0;
        if (from && from->kind == TYPE_POINTER && x->as.cast.target_type &&
            x->as.cast.target_type->kind != TYPE_BOOL &&
            isnum(x->as.cast.target_type)) {
            fputs("cast ptr ulong\n", e->out);
            return cast_emit(e, &TYPE_ULONG_VALUE, x->as.cast.target_type);
        }
        return cast_emit(e, from, x->as.cast.target_type);
    }
    default:
        fail(e, "expression kind not yet supported by Crunch IR");
        return 0;
    }
}
/**
 * @brief Emits an expression while enforcing the IR traversal depth limit.
 * @param e IR emitter state containing output and diagnostic context.
 * @param x Expression node to emit.
 * @return One on success; zero on unsupported input or excessive nesting.
 */
static int expression(Emitter *e, const Expr *x) {
    if (e->expression_depth >= IR_MAX_AST_RECURSION_DEPTH) {
        e->recursion_limit_exceeded = 1;
        fail(e, "maximum IR expression nesting depth exceeded");
        return 0;
    }
    ++e->expression_depth;
    int result = expression_impl(e, x);
    --e->expression_depth;
    return result;
}
/**
 * @brief Recognizes direct calls to exit or to declared noreturn functions.
 * @param program AST containing function declarations.
 * @param expr Expression to inspect.
 * @param module Nonzero for module-mode compilation.
 * @return One for a recognized noreturn call; otherwise zero.
 */
static int expression_is_noreturn(const AstProgram *program, const Expr *expr, int module) {
    if (!expr || expr->kind != EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != EXPR_NAME)
        return 0;
    Decl *function = find_function(program, expr->as.call.callee->as.name.name);
    if (!module && !strcmp(expr->as.call.callee->as.name.name, "exit") &&
        (!function || crunch_is_runtime_declaration(function)))
        return 1;
    return function && function->kind == DECL_FUNCTION && function->as.function.noreturn;
}
/**
 * @brief Checks whether all reachable paths through a statement return.
 * @param program AST containing function declarations used by noreturn checks.
 * @param s Statement to analyze.
 * @param depth Current recursive analysis depth.
 * @param module Nonzero for module-mode compilation.
 * @return One if all paths return, zero if not, or -1 if the depth limit is exceeded.
 */
static int always_returns(const AstProgram *program, const Stmt *s, unsigned depth, int module) {
    if (!s)
        return 0;
    if (depth >= IR_MAX_STATEMENT_ANALYSIS_DEPTH)
        return -1;
    switch (s->kind) {
    case STMT_RETURN:
        return 1;
    case STMT_EXPRESSION:
        return expression_is_noreturn(program, s->as.expression.expression, module);
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.statements.count; i++) {
            const Stmt *l = s->as.block.statements.items[i];
            if (l) {
                int result = always_returns(program, l, depth + 1, module);
                if (result)
                    return result;
            }
        }
        return 0;
    case STMT_IF:
        if (!s->as.if_stmt.else_branch)
            return 0;
        {
            int then_result = always_returns(program, s->as.if_stmt.then_branch, depth + 1, module);
            if (then_result < 0)
                return -1;
            int else_result = always_returns(program, s->as.if_stmt.else_branch, depth + 1, module);
            if (else_result < 0)
                return -1;
            return then_result && else_result;
        }
    default:
        return 0;
    }
}
/**
 * @brief Emits IR for a statement and its nested control flow.
 * @param e IR emitter state containing program, output, and diagnostic context.
 * @param s Statement node being checked, serialized, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int switch_statement(Emitter *e, const Stmt *s) {
    size_t scope = e->count, count = s->as.switch_stmt.cases.count;
    size_t vla_scope = e->active_vla_count;
    const Type *type = expr_type(e, s->as.switch_stmt.expression);
    if (type && type->kind == TYPE_ENUM)
        type = type_effective_kind(type) == TYPE_UNSIGNED_INT ? &TYPE_UINT_VALUE : &TYPE_INT_VALUE;
    if (type && (promoted_small_kind(type->kind) ||
                 (e->program->c11_numeric && type->kind == TYPE_BYTE)))
        type = &TYPE_INT_VALUE;
    if (e->next_slot >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "too many locals for switch");
        return 0;
    }
    unsigned end = label(e), fallback = end, slot = e->next_slot++;
    unsigned *labels = count ? calloc(count, sizeof(*labels)) : NULL;
    if (count && !labels) {
        fail_oom(e);
        return 0;
    }
    fprintf(e->out, "local %u %s\n", slot, tn(type));
    for (size_t i = 0; i < count; i++) {
        labels[i] = label(e);
        if (!s->as.switch_stmt.cases.items[i].value)
            fallback = labels[i];
    }
    if (!expression_as(e, s->as.switch_stmt.expression, type)) {
        free(labels);
        return 0;
    }
    fprintf(e->out, "store %u %s\n", slot, tn(type));
    for (size_t i = 0; i < count; i++) {
        const Expr *value = s->as.switch_stmt.cases.items[i].value;
        if (!value)
            continue;
        fprintf(e->out, "load %u\n", slot);
        if (!expression_as(e, value, type)) {
            free(labels);
            return 0;
        }
        fprintf(e->out, "eq %s\njnz L%u\n", tn(type), labels[i]);
    }
    fprintf(e->out, "jmp L%u\n", fallback);
    if (e->break_depth >= 128) {
        free(labels);
        fail(e, "break nesting exceeds IR limit");
        return 0;
    }
    e->break_vla_counts[e->break_depth] = vla_scope;
    e->break_labels[e->break_depth++] = end;
    for (size_t j = 0; j < s->as.switch_stmt.prefix.count; j++) {
        if (!statement(e, s->as.switch_stmt.prefix.items[j])) {
            e->break_depth--;
            free(labels);
            release_local_names(e, scope);
            e->count = scope;
            return 0;
        }
    }
    for (size_t i = 0; i < count; i++) {
        mark(e, labels[i]);
        for (size_t j = 0; j < s->as.switch_stmt.cases.items[i].statements.count; j++) {
            if (!statement(e, s->as.switch_stmt.cases.items[i].statements.items[j])) {
                e->break_depth--;
                free(labels);
                release_local_names(e, scope);
                e->count = scope;
                return 0;
            }
        }
    }
    e->break_depth--;
    if (e->active_vla_count > vla_scope)
        emit_stack_restore(e, e->active_vla_marks[vla_scope]);
    mark(e, end);
    free(labels);
    release_local_names(e, scope);
    e->count = scope;
    e->active_vla_count = vla_scope;
    return 1;
}
/**
 * @brief Emits a bounded byte loop to zero storage or copy a string into it.
 * @param e IR emitter.
 * @param address Destination storage address alias.
 * @param offset Starting byte offset in the destination.
 * @param index_slot Temporary long slot used as the loop index.
 * @param count Number of bytes to write.
 * @param copy Nonzero to copy from source rather than write zeros.
 * @param source_slot Temporary pointer slot holding the source when copying.
 * @return No value.
 */
static void emit_array_byte_loop(Emitter *e, const char *address, size_t offset,
                                 unsigned index_slot, size_t count, int copy,
                                 unsigned source_slot) {
    unsigned loop = label(e), done = label(e);
    fprintf(e->out, "push 0\ncast int long\nstore %u long\n", index_slot);
    mark(e, loop);
    fprintf(e->out, "load %u\npush %zu\ncast int long\nlt long\njz L%u\n"
                    "laddr %s\npush %zu\nptradd byte\nload %u\nptradd byte\n",
            index_slot, count, done, address, offset, index_slot);
    if (copy)
        fprintf(e->out, "load %u\nload %u\nptradd byte\nloadind byte\n", source_slot,
                index_slot);
    else
        fputs("push 0\n", e->out);
    fprintf(e->out, "storeind byte\ndrop\nload %u\npush 1\ncast int long\n"
                    "add long\nstore %u long\njmp L%u\n", index_slot, index_slot, loop);
    mark(e, done);
}

static int fixed_address_constant(Emitter *, const Expr *, char *, size_t, int64_t *, unsigned);
static int fixed_array_numeric_bits(Emitter *, const Type *, const Expr *, uint64_t *);

/**
 * @brief Selects evaluated branches in a static pointer initializer.
 * @param value Initializer expression.
 * @return Selected expression, or the original nonconstant conditional.
 */
static const Expr *static_pointer_value(const Expr *value) {
    for (unsigned depth = 0; value && value->kind == EXPR_CONDITIONAL &&
                             depth < IR_MAX_AST_RECURSION_DEPTH; ++depth) {
        long double condition;
        if (!sema_numeric_constant(value->as.conditional.condition, &condition))
            break;
        value = condition ? value->as.conditional.then_expr : value->as.conditional.else_expr;
    }
    return value;
}

typedef struct {
    size_t offset;
    char symbol[128];
    int64_t addend;
} InitializerRelocation;

typedef struct {
    unsigned char *bytes;
    size_t length;
    InitializerRelocation *relocations;
    size_t relocation_count;
} StructInitData;

/**
 * @brief Grows the initialized prefix of a static structure with zero bytes.
 * @param e IR emitter.
 * @param data Static initialization buffer.
 * @param length Required prefix length.
 * @return Nonzero on success.
 */
static int grow_struct_init_data(Emitter *e, StructInitData *data, size_t length) {
    if (length <= data->length)
        return 1;
    unsigned char *bytes = realloc(data->bytes, length);
    if (!bytes) {
        fail_oom(e);
        return 0;
    }
    memset(bytes + data->length, 0, length - data->length);
    data->bytes = bytes;
    data->length = length;
    return 1;
}

static int emit_struct_init_value(Emitter *, const Type *, const Expr *, const char *, size_t,
                                  unsigned, StructInitData *, unsigned);

/**
 * @brief Consumes one subobject from an initializer list, including brace elision.
 * @param e IR emitter.
 * @param type Destination subobject type.
 * @param items Current brace-level initializer list.
 * @param index Next item in the list, updated on consumption.
 * @param block Local address alias or static block.
 * @param offset Subobject byte offset.
 * @param index_slot Local byte-loop counter.
 * @param data Static block bytes, or NULL for automatic storage.
 * @param depth Current nesting depth.
 * @return Nonzero on success.
 */
static int emit_initializer_item(Emitter *e, const Type *type, const ExprArray *items,
                                 size_t *index, const char *block, size_t offset,
                                 unsigned index_slot, StructInitData *data, unsigned depth) {
    if (!type || *index >= items->count || depth > 24) {
        fail(e, "unsupported aggregate initializer nesting");
        return 0;
    }
    const Expr *value = items->items[*index];
    int aggregate = type->kind == TYPE_STRUCT || type->kind == TYPE_ARRAY;
    const Type *actual = type->kind == TYPE_STRUCT && value->kind != EXPR_ARRAY_INIT
                             ? expr_type(e, value) : NULL;
    if (!aggregate || value->kind == EXPR_ARRAY_INIT ||
        (type->kind == TYPE_ARRAY && string_element_matches(type->pointee, value)) ||
        (actual && actual->kind == TYPE_STRUCT && actual->struct_name &&
         type->struct_name && !strcmp(actual->struct_name, type->struct_name))) {
        ++*index;
        return emit_struct_init_value(e, type, value, block, offset, index_slot, data, depth);
    }
    Decl *structure = NULL;
    size_t offsets[256], size, alignment, width = 0, count = type->array_length;
    if (type->kind == TYPE_STRUCT) {
        structure = find_struct(e->program, type->struct_name);
        if (!structure || structure->as.structure.field_count > 256 ||
            !struct_layout(e->program, structure, offsets, &size, &alignment)) {
            fail(e, "unsupported structure initializer layout");
            return 0;
        }
        count = structure->as.structure.is_union ? 1 : structure->as.structure.field_count;
    } else if (!type_layout_size(type->pointee, layout_find_struct, e->program,
                                 &width, &alignment)) {
        fail(e, "unsupported array initializer layout");
        return 0;
    }
    for (size_t i = 0; i < count && *index < items->count; ++i) {
        const Type *element = structure ? structure->as.structure.fields[i]->as.variable.type
                                        : type->pointee;
        size_t pos = structure ? offsets[i] : i * width;
        if (!emit_initializer_item(e, element, items, index, block, offset + pos,
                                   index_slot, data, depth + 1))
            return 0;
    }
    return 1;
}

/**
 * @brief Emits a structure initializer into local storage or static block data.
 * @param e IR emitter.
 * @param type Destination subobject type.
 * @param value Initializer expression.
 * @param block Local address alias or static block name.
 * @param offset Byte offset of the subobject.
 * @param index_slot Local byte-loop counter.
 * @param data Static block bytes, or NULL for automatic storage.
 * @param depth Current aggregate nesting depth.
 * @return Nonzero on success.
 */
static int emit_struct_init_value(Emitter *e, const Type *type, const Expr *value,
                                  const char *block, size_t offset, unsigned index_slot,
                                  StructInitData *data, unsigned depth) {
    if (!type || !value || depth > 24) {
        fail(e, "unsupported structure initializer nesting");
        return 0;
    }
    if (type->kind == TYPE_ARRAY && value->kind == EXPR_ARRAY_INIT &&
        value->as.array_init.count == 1 &&
        string_element_matches(type->pointee, value->as.array_init.items[0]))
        return emit_struct_init_value(e, type, value->as.array_init.items[0], block,
                                      offset, index_slot, data, depth + 1);
    if (type->kind == TYPE_ARRAY && value->kind == EXPR_STRING) {
        size_t width = value->string_literal_wide ? 4 : 1;
        size_t count = string_array_count(value);
        if (count > type->array_length)
            count = type->array_length;
        count *= width;
        if (data) {
            size_t full = string_array_count(value) * width;
            if (full > (SIZE_MAX - 1) / 2) {
                fail(e, "structure string initializer is too large");
                return 0;
            }
            char *hex = malloc(full * 2 + 1);
            if (!hex) {
                fail_oom(e);
                return 0;
            }
            if (!string_hex(value, hex, full * 2 + 1)) {
                free(hex);
                fail(e, "invalid structure string initializer");
                return 0;
            }
            if (!grow_struct_init_data(e, data, offset + count)) {
                free(hex);
                return 0;
            }
            for (size_t i = 0; i < count; ++i) {
                unsigned hi = hex[i * 2] <= '9' ? hex[i * 2] - '0' : hex[i * 2] - 'a' + 10;
                unsigned lo = hex[i * 2 + 1] <= '9' ? hex[i * 2 + 1] - '0' : hex[i * 2 + 1] - 'a' + 10;
                data->bytes[offset + i] = (unsigned char)((hi << 4) | lo);
            }
            free(hex);
        } else {
            if (e->next_slot >= CRUNCH_LOCAL_LIMIT) {
                fail(e, "too many locals for structure initializer");
                return 0;
            }
            unsigned source = e->next_slot++;
            fprintf(e->out, "local %u ptr\n", source);
            if (!expression(e, value))
                return 0;
            fprintf(e->out, "store %u ptr\n", source);
            emit_array_byte_loop(e, block, offset, index_slot, count, 1, source);
        }
        return 1;
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_ARRAY) {
        if (value->kind != EXPR_ARRAY_INIT) {
            if (data || type->kind != TYPE_STRUCT) {
                fail(e, "static aggregate initializer must be a constant list");
                return 0;
            }
            fprintf(e->out, "laddr %s\npush %zu\nptradd byte\n", block, offset);
            return emit_aggregate_address(e, value, 0) && emit_fixed_array_copy(e, type);
        }
        Decl *structure = NULL;
        size_t offsets[256], size, alignment, width = 0, count = type->array_length;
        if (type->kind == TYPE_STRUCT) {
            structure = find_struct(e->program, type->struct_name);
            if (!structure || structure->as.structure.field_count > 256 ||
                !struct_layout(e->program, structure, offsets, &size, &alignment)) {
                fail(e, "unsupported structure initializer layout");
                return 0;
            }
            count = structure->as.structure.is_union ? 1 : structure->as.structure.field_count;
        } else if (!type_layout_size(type->pointee, layout_find_struct, e->program,
                                     &width, &alignment)) {
            fail(e, "unsupported array initializer layout");
            return 0;
        }
        size_t cursor = 0;
        for (size_t i = 0; i < count && cursor < value->as.array_init.count; ++i) {
            const Type *element = structure ? structure->as.structure.fields[i]->as.variable.type
                                            : type->pointee;
            size_t pos = structure ? offsets[i] : i * width;
            if (!emit_initializer_item(e, element, &value->as.array_init, &cursor, block,
                                       offset + pos, index_slot, data, depth + 1))
                return 0;
        }
        if (cursor < value->as.array_init.count) {
            fail(e, "too many aggregate initializers");
            return 0;
        }
        return 1;
    }
    if (value->kind == EXPR_ARRAY_INIT) {
        if (value->as.array_init.count != 1) {
            fail(e, "invalid scalar initializer");
            return 0;
        }
        return emit_struct_init_value(e, type, value->as.array_init.items[0], block,
                                      offset, index_slot, data, depth + 1);
    }
    if (type->kind == TYPE_LONG_DOUBLE) {
        if (!data) {
            fprintf(e->out, "laddr %s\npush %zu\nptradd byte\n", block, offset);
            if (!expression_as(e, value, type))
                return 0;
            fputs("extcopy\ndrop\n", e->out);
            return 1;
        }
        long double number;
        if (sizeof(number) < 10 || !sema_numeric_constant(value, &number)) {
            fail(e, "static long double initializer must be a numeric constant");
            return 0;
        }
        if (!grow_struct_init_data(e, data, offset + 10))
            return 0;
        memcpy(data->bytes + offset, &number, 10);
        return 1;
    }
    if (!isnum(type) && !is_value_pointer(type)) {
        fail(e, "unsupported structure initializer field");
        return 0;
    }
    if (!data) {
        fprintf(e->out, "laddr %s\npush %zu\nptradd byte\n", block, offset);
        if (!expression_as(e, value, type))
            return 0;
        fprintf(e->out, "storeind %s\ndrop\n", tn(type));
        return 1;
    }
    if (is_value_pointer(type)) {
        value = static_pointer_value(value);
        if (!is_null_literal(value)) {
            char symbol[128];
            int64_t addend;
            if (!fixed_address_constant(e, value, symbol, sizeof symbol, &addend, 0)) {
                fail(e, "static structure pointer initializer must be an address constant");
                return 0;
            }
            if (data->relocation_count == SIZE_MAX ||
                data->relocation_count + 1 > SIZE_MAX / sizeof(*data->relocations)) {
                fail_oom(e);
                return 0;
            }
            InitializerRelocation *relocations =
                realloc(data->relocations, (data->relocation_count + 1) * sizeof(*relocations));
            if (!relocations) {
                fail_oom(e);
                return 0;
            }
            data->relocations = relocations;
            InitializerRelocation *relocation = &relocations[data->relocation_count++];
            relocation->offset = offset;
            memcpy(relocation->symbol, symbol, strlen(symbol) + 1);
            relocation->addend = addend;
        }
        return 1;
    }
    uint64_t bits;
    size_t width, alignment;
    if (!type_layout_size(type, layout_find_struct, e->program, &width, &alignment) ||
        width > sizeof bits || !fixed_array_numeric_bits(e, type, value, &bits)) {
        fail(e, "static structure initializer must contain numeric constants");
        return 0;
    }
    if (bits && !grow_struct_init_data(e, data, offset + width))
        return 0;
    for (size_t i = 0; bits && i < width; ++i)
        data->bytes[offset + i] = (unsigned char)(bits >> (i * 8));
    return 1;
}

/**
 * @brief Clears static initializer bytes and relocations in a subobject.
 * @param data Static block initialization data.
 * @param offset Start of the subobject.
 * @param size Size of the subobject.
 * @return No value.
 */
static void clear_struct_init_data(StructInitData *data, size_t offset, size_t size) {
    if (offset < data->length) {
        size_t count = data->length - offset;
        if (count > size)
            count = size;
        memset(data->bytes + offset, 0, count);
    }
    size_t kept = 0;
    for (size_t i = 0; i < data->relocation_count; ++i) {
        InitializerRelocation *relocation = &data->relocations[i];
        if (relocation->offset >= offset && relocation->offset - offset < size)
            continue;
        data->relocations[kept++] = *relocation;
    }
    data->relocation_count = kept;
}

/**
 * @brief Emits a checked sequence of designated initializer operations.
 * @param e IR emitter.
 * @param plan Semantic initializer plan.
 * @param block Local address alias or static block.
 * @param index_slot Local byte-loop counter.
 * @param data Static bytes, or NULL for automatic storage.
 * @return Nonzero on success.
 */
static int emit_initializer_plan(Emitter *e, const InitializerPlan *plan, const char *block,
                                 unsigned index_slot, StructInitData *data) {
    for (size_t i = 0; i < plan->count; ++i) {
        const InitializerOperation *operation = &plan->items[i];
        if (operation->bit_field) {
            if (operation->clear || !operation->value || !operation->bit_width ||
                operation->bit_width > 32) {
                fail(e, "invalid bit-field initializer");
                return 0;
            }
            if (data) {
                uint64_t bits;
                if (!fixed_array_numeric_bits(e, operation->type, operation->value, &bits) ||
                    !grow_struct_init_data(e, data, operation->offset + operation->size)) {
                    fail(e, "static bit-field initializer must be a numeric constant");
                    return 0;
                }
                uint64_t mask = ((UINT64_C(1) << operation->bit_width) - 1)
                                << operation->bit_offset;
                bits <<= operation->bit_offset;
                for (size_t j = 0; j < operation->size; ++j) {
                    unsigned byte_mask = (unsigned)((mask >> (8 * j)) & 255);
                    unsigned byte_bits = (unsigned)((bits >> (8 * j)) & 255);
                    data->bytes[operation->offset + j] =
                        (unsigned char)((data->bytes[operation->offset + j] & ~byte_mask) |
                                        (byte_bits & byte_mask));
                }
            } else {
                fprintf(e->out, "laddr %s\npush %zu\nptradd byte\n", block, operation->offset);
                if (!emit_bit_field_update(e, operation->type, operation->bit_offset,
                                           operation->bit_width, operation->value, ASSIGN_SIMPLE,
                                           0, 0))
                    return 0;
                fputs("drop\n", e->out);
            }
            continue;
        }
        if (data)
            clear_struct_init_data(data, operation->offset, operation->size);
        else if (operation->clear)
            emit_array_byte_loop(e, block, operation->offset, index_slot, operation->size, 0, 0);
        if (!operation->clear &&
            !emit_struct_init_value(e, operation->type, operation->value, block,
                                    operation->offset, index_slot, data, 0))
            return 0;
    }
    return 1;
}

/**
 * @brief Emits and initializes storage for a block-scope compound literal.
 * @param e IR emitter.
 * @param x Compound literal expression.
 * @return Nonzero on success, leaving its address on the IR stack.
 */
static int emit_compound_literal(Emitter *e, const Expr *x) {
    size_t size, alignment;
    if (!type_layout_size(x->as.cast.target_type, layout_find_struct, e->program,
                          &size, &alignment) || !size || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "unsupported compound literal layout or too many locals");
        return 0;
    }
    unsigned slot = e->next_slot++;
    char address[64];
    snprintf(address, sizeof address, "__compound_address_%u", slot);
    fprintf(e->out, "local %u long\nlocalblock __compound_%u %zu %zu\n"
                    "localalias %s __compound_%u 0 byte\n",
            slot, slot, size, alignment, address, slot);
    if (x->as.cast.target_type->kind == TYPE_ARRAY ||
        x->as.cast.target_type->kind == TYPE_STRUCT)
        emit_array_byte_loop(e, address, 0, slot, size, 0, 0);
    if (x->as.cast.semantic_plan
            ? !emit_initializer_plan(e, x->as.cast.semantic_plan, address, slot, NULL)
            : !emit_struct_init_value(e, x->as.cast.target_type, x->as.cast.operand,
                                      address, 0, slot, NULL, 0))
        return 0;
    fprintf(e->out, "laddr %s\n", address);
    return 1;
}

/**
 * @brief Emits constant bytes and relocations for an initialized static structure.
 * @param e IR emitter.
 * @param type Structure type.
 * @param value Initializer list.
 * @param plan Checked designated plan, or NULL for positional initialization.
 * @param block Static block name.
 * @param size Size of the block.
 * @return Nonzero on success.
 */
static int emit_global_struct_initializer(Emitter *e, const Type *type, const Expr *value,
                                          const InitializerPlan *plan, const char *block,
                                          size_t size) {
    StructInitData data = {0};
    int ok = plan ? emit_initializer_plan(e, plan, block, 0, &data)
                  : emit_struct_init_value(e, type, value, block, 0, 0, &data, 0);
    size_t used = data.length;
    while (used && !data.bytes[used - 1])
        --used;
    if (used > size) {
        fail(e, "structure initializer exceeds its block");
        ok = 0;
    }
    if (ok && used) {
        static const char digits[] = "0123456789abcdef";
        fprintf(e->out, "blockdata %s ", block);
        for (size_t i = 0; i < used; ++i) {
            fputc(digits[data.bytes[i] >> 4], e->out);
            fputc(digits[data.bytes[i] & 15], e->out);
        }
        fputc('\n', e->out);
    }
    if (ok)
        for (size_t i = 0; i < data.relocation_count; ++i)
            fprintf(e->out, "blockreloc %s %zu %s %" PRId64 "\n", block,
                    data.relocations[i].offset, data.relocations[i].symbol,
                    data.relocations[i].addend);
    free(data.bytes);
    free(data.relocations);
    return ok;
}

/**
 * @brief Names a file-scope literal without colliding with user declarations.
 * @param e IR emitter.
 * @param x Compound literal expression.
 * @param name Destination buffer.
 * @param capacity Destination buffer size.
 * @return Nonzero if the name fits.
 */
static int file_compound_symbol(Emitter *e, const Expr *x, char *name, size_t capacity) {
    unsigned suffix = 0;
    for (;;) {
        int written = snprintf(name, capacity, "_crunch_file_compound_%zu_%u",
                               x->location.offset, suffix);
        if (written < 0 || (size_t)written >= capacity)
            return 0;
        if (!find_global(e->program, name) && !find_function(e->program, name))
            return 1;
        if (suffix == UINT_MAX)
            return 0;
        ++suffix;
    }
}

/**
 * @brief Emits static-storage compound literals nested in a file-scope expression.
 * @param e IR emitter.
 * @param x Expression to traverse.
 * @param depth Current recursion depth.
 * @return Nonzero on success.
 */
static int emit_file_compound_literals(Emitter *e, const Expr *x, unsigned depth) {
    if (!x)
        return 1;
    if (depth >= IR_MAX_AST_RECURSION_DEPTH) {
        fail(e, "maximum compound literal nesting depth exceeded");
        return 0;
    }
    switch (x->kind) {
    case EXPR_CAST:
        if (!emit_file_compound_literals(e, x->as.cast.operand, depth + 1))
            return 0;
        if (x->as.cast.semantic_file_scope && x->as.cast.operand &&
            x->as.cast.operand->kind == EXPR_ARRAY_INIT) {
            size_t size, alignment;
            char name[64];
            const Type *type = x->as.cast.target_type;
            if (!type_layout_size(type, layout_find_struct, e->program, &size, &alignment) ||
                !size || size > UINT32_MAX) {
                fail(e, "unsupported file-scope compound literal layout");
                return 0;
            }
            if (!file_compound_symbol(e, x, name, sizeof name)) {
                fail(e, "cannot name file-scope compound literal");
                return 0;
            }
            fprintf(e->out, "internal %s\nglobalblock %s %zu %zu\n", name, name,
                    size, alignment);
            return emit_global_struct_initializer(e, type, x->as.cast.operand,
                                                  x->as.cast.semantic_plan, name, size);
        }
        return 1;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        return emit_file_compound_literals(e, x->as.size_of.operand, depth + 1);
    case EXPR_OFFSETOF:
        for (size_t i = 0; i < x->as.offset_of.count; ++i)
            if (!emit_file_compound_literals(e, x->as.offset_of.items[i].index, depth + 1))
                return 0;
        return 1;
    case EXPR_UNARY:
        return emit_file_compound_literals(e, x->as.unary.operand, depth + 1);
    case EXPR_BINARY:
        return emit_file_compound_literals(e, x->as.binary.left, depth + 1) &&
               emit_file_compound_literals(e, x->as.binary.right, depth + 1);
    case EXPR_ASSIGN:
        return emit_file_compound_literals(e, x->as.assign.target, depth + 1) &&
               emit_file_compound_literals(e, x->as.assign.value, depth + 1);
    case EXPR_CONDITIONAL:
        return emit_file_compound_literals(e, x->as.conditional.condition, depth + 1) &&
               emit_file_compound_literals(e, x->as.conditional.then_expr, depth + 1) &&
               emit_file_compound_literals(e, x->as.conditional.else_expr, depth + 1);
    case EXPR_CALL:
        if (!emit_file_compound_literals(e, x->as.call.callee, depth + 1))
            return 0;
        for (size_t i = 0; i < x->as.call.arguments.count; ++i)
            if (!emit_file_compound_literals(e, x->as.call.arguments.items[i], depth + 1))
                return 0;
        return 1;
    case EXPR_ARRAY_INIT:
        for (size_t i = 0; i < x->as.array_init.count; ++i)
            if (!emit_file_compound_literals(e, x->as.array_init.items[i], depth + 1))
                return 0;
        return 1;
    case EXPR_DESIGNATED:
        for (size_t i = 0; i < x->as.designated.count; ++i)
            if (!emit_file_compound_literals(e, x->as.designated.items[i].index, depth + 1))
                return 0;
        return emit_file_compound_literals(e, x->as.designated.value, depth + 1);
    case EXPR_INDEX:
        return emit_file_compound_literals(e, x->as.index.base, depth + 1) &&
               emit_file_compound_literals(e, x->as.index.index, depth + 1);
    case EXPR_MEMBER:
        return emit_file_compound_literals(e, x->as.member.base, depth + 1);
    default:
        return 1;
    }
}

/**
 * @brief Reserves a unique internal symbol for a static local declaration.
 * @param e IR emitter.
 * @param name Buffer receiving the symbol name.
 * @param capacity Size of the symbol buffer.
 * @return Unique static-local identifier.
 */
static unsigned static_local_symbol(Emitter *e, char *name, size_t capacity) {
    unsigned id;
    do {
        id = e->next_static_local++;
        snprintf(name, capacity, "_crunch_static_local_%u", id);
    } while (find_global(e->program, name) || find_function(e->program, name));
    return id;
}

/**
 * @brief Emits persistent storage and constant initialization for a local structure.
 * @param e IR emitter.
 * @param d Local structure declaration.
 * @return Nonzero on success, or zero with a diagnostic.
 */
static int emit_static_local_struct(Emitter *e, const Decl *d) {
    const Type *type = d->as.variable.type;
    Decl *sd = find_struct(e->program, type->struct_name);
    size_t offsets[256], size, alignment;
    if (!sd || sd->as.structure.field_count > 256 ||
        !struct_layout(e->program, sd, offsets, &size, &alignment) ||
        !size || size > UINT32_MAX) {
        fail(e, "unsupported or oversized static local structure layout");
        return 0;
    }
    if (d->semantic_alignment > alignment)
        alignment = d->semantic_alignment;
    if (e->count >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "too many local variables");
        return 0;
    }
    char name[64];
    unsigned id = static_local_symbol(e, name, sizeof name);
    e->locals[e->count++] = (Local){d->as.variable.name, id, type, LOCAL_STATIC_STRUCT};
    FILE *body = e->out;
    e->out = e->static_out;
    fprintf(e->out, "internal %s\nglobalblock %s %zu %zu\n", name, name, size, alignment);
    const Expr *init = d->as.variable.initializer.expression;
    int ok = 1;
    if (d->as.variable.initializer.kind != INIT_NONE) {
        if (!init || init->kind != EXPR_ARRAY_INIT) {
            fail(e, "static local structure initializer requires a constant list");
            ok = 0;
        } else
            ok = emit_global_struct_initializer(e, type, init,
                                                d->as.variable.initializer.semantic_plan,
                                                name, size);
    }
    if (ok && !register_local_fields(e, d->as.variable.name, name, sd, 0, 0, 1)) {
        fail(e, "unsupported or oversized nested structure fields");
        ok = 0;
    }
    e->out = body;
    return ok;
}

/**
 * @brief Emits stack storage and initialization for a local fixed array.
 * @param e IR emitter.
 * @param d Local array declaration.
 * @return One on success, or zero with a diagnostic.
 */
static int emit_fixed_local_array(Emitter *e, const Decl *d) {
    const Type *array = d->as.variable.type;
    const Type *element = array->pointee;
    size_t size, alignment;
    if (d->is_static) {
        if (e->count >= CRUNCH_LOCAL_LIMIT) {
            fail(e, "too many local variables");
            return 0;
        }
        char name[64];
        unsigned id = static_local_symbol(e, name, sizeof name);
        e->locals[e->count++] = (Local){d->as.variable.name, id, array, LOCAL_STATIC_ARRAY};
        FILE *body = e->out;
        e->out = e->static_out;
        fprintf(e->out, "internal %s\n", name);
        int ok = emit_fixed_global_array(e, d, name);
        e->out = body;
        return ok;
    }
    if (!type_layout_size(array, layout_find_struct, e->program, &size, &alignment)) {
        fail(e, "unsupported or oversized local fixed array layout");
        return 0;
    }
    if (d->semantic_alignment > alignment)
        alignment = d->semantic_alignment;
    if (!size || alignment > (size_t)INT32_MAX - 65536 ||
        size > (size_t)INT32_MAX - 65536 - alignment ||
        e->fixed_array_bytes > (size_t)INT32_MAX - 65536 - size - alignment) {
        fail(e, "unsupported or oversized local fixed array layout");
        return 0;
    }
    if (e->count >= CRUNCH_LOCAL_LIMIT || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "too many local variables");
        return 0;
    }
    e->fixed_array_bytes += size + alignment;
    unsigned slot = e->next_slot++;
    e->locals[e->count++] = (Local){d->as.variable.name, slot, array, 0};
    fprintf(e->out, "local %u int\nlocalblock __local_array_%u %zu %zu\n"
                    "localalias __local_array_address_%u __local_array_%u 0 byte\n",
            slot, slot, size, alignment, slot, slot);
    if (d->as.variable.initializer.kind == INIT_NONE)
        return 1;
    const Expr *init = d->as.variable.initializer.expression;
    if (!init || (init->kind != EXPR_ARRAY_INIT && init->kind != EXPR_STRING)) {
        fail(e, "unsupported fixed array initializer");
        return 0;
    }
    if (init->kind == EXPR_ARRAY_INIT && init->as.array_init.count == 1 &&
        string_element_matches(element, init->as.array_init.items[0]))
        init = init->as.array_init.items[0];
    if (e->next_slot >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "too many locals for fixed array initialization");
        return 0;
    }
    unsigned index_slot = e->next_slot++;
    fprintf(e->out, "local %u long\n", index_slot);
    char address[64];
    snprintf(address, sizeof address, "__local_array_address_%u", slot);
    emit_array_byte_loop(e, address, 0, index_slot, size, 0, 0);
    if (d->as.variable.initializer.semantic_plan)
        return emit_initializer_plan(e, d->as.variable.initializer.semantic_plan,
                                     address, index_slot, NULL);
    if (element && (element->kind == TYPE_STRUCT || element->kind == TYPE_ARRAY))
        return emit_struct_init_value(e, array, init, address, 0, index_slot, NULL, 0);
    if (init->kind == EXPR_STRING) {
        if (!string_element_matches(element, init) || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
            fail(e, "unsupported fixed array string initializer");
            return 0;
        }
        unsigned source_slot = e->next_slot++;
        fprintf(e->out, "local %u ptr\n", source_slot);
        if (!expression(e, init))
            return 0;
        fprintf(e->out, "store %u ptr\n", source_slot);
        size_t count = string_array_count(init);
        if (count > array->array_length)
            count = array->array_length;
        count *= init->string_literal_wide ? 4 : 1;
        emit_array_byte_loop(e, address, 0, index_slot, count, 1, source_slot);
        return 1;
    }
    for (size_t i = 0; i < init->as.array_init.count; i++) {
        const Expr *value = init->as.array_init.items[i];
        if (element->kind == TYPE_STRUCT) {
            Expr name = {0}, index = {0}, target = {0};
            name.kind = EXPR_NAME;
            name.as.name.name = d->as.variable.name;
            index.kind = EXPR_INTEGER;
            index.as.integer_value = (int64_t)i;
            target.kind = EXPR_INDEX;
            target.as.index.base = &name;
            target.as.index.index = &index;
            if (!emit_struct_assignment(e, &target, value, element)) {
                fail(e, "unsupported fixed structure-array initializer");
                return 0;
            }
            fputs("drop\n", e->out);
        } else if (element->kind == TYPE_LONG_DOUBLE) {
            if (!emit_struct_init_value(e, element, value, address, i * 16,
                                        index_slot, NULL, 0))
                return 0;
        } else {
            if (!isnum(element) && !is_value_pointer(element)) {
                fail(e, "unsupported fixed array element type");
                return 0;
            }
            fprintf(e->out, "laddr __local_array_address_%u\npush %zu\nptradd %s\n",
                    slot, i, tn(element));
            if (!expression_as(e, value, element))
                return 0;
            fprintf(e->out, "storeind %s\ndrop\n", tn(element));
        }
    }
    return 1;
}

/**
 * @brief Restores a previously saved dynamic stack position.
 * @param e IR emitter.
 * @param slot Local slot containing the saved position.
 * @return No value.
 */
static void emit_stack_restore(Emitter *e, unsigned slot) {
    if (!e->function_has_alloca)
        fprintf(e->out, "load %u\nstackrestore\n", slot);
}

/**
 * @brief Emits an automatic variable-length array and saves its byte size.
 * @param e IR emitter.
 * @param d Semantically validated array declaration.
 * @return One on success, or zero with a diagnostic.
 */
static int emit_local_vla(Emitter *e, const Decl *d) {
    size_t size, alignment;
    const Type *dimensions[64], *element = d->as.variable.type;
    unsigned count = 0;
    while (element && element->kind == TYPE_ARRAY) {
        if (count == sizeof dimensions / sizeof dimensions[0]) {
            fail(e, "too many variable-length array dimensions");
            return 0;
        }
        dimensions[count++] = element;
        element = element->pointee;
    }
    if (!count || e->count >= CRUNCH_LOCAL_LIMIT || e->next_slot > CRUNCH_LOCAL_LIMIT - (count + 2) ||
        e->active_vla_count >= 512 ||
        !type_layout_size(element, layout_find_struct, e->program, &size, &alignment)) {
        fail(e, "unsupported variable-length array layout or too many locals");
        return 0;
    }
    unsigned slot = e->next_slot;
    e->next_slot += count + 2;
    fprintf(e->out, "local %u ptr\nlocal %u ulong\nlocal %u ptr\n"
                    "stackmark\nstore %u ptr\n", slot, slot + 1, slot + 2, slot + 2);
    for (unsigned i = 1; i < count; ++i)
        fprintf(e->out, "local %u ulong\n", slot + i + 2);
    for (unsigned i = 0; i < count; ++i) {
        const Type *array = dimensions[i];
        unsigned size_slot = i ? slot + i + 2 : slot + 1;
        const Expr *bound = i ? (array->array_bound && array->array_bound->variable
                                     ? array->array_bound->expression : NULL)
                              : d->as.variable.array_size;
        if (bound) {
            if (!expression_as(e, bound, &TYPE_ULONG_VALUE))
                return 0;
        } else
            fprintf(e->out, "pushu %zu ulong\n", array->array_length);
        fprintf(e->out, "store %u ulong\n", size_slot);
    }
    for (unsigned i = count; i > 0; --i) {
        unsigned index = i - 1;
        unsigned size_slot = index ? slot + index + 2 : slot + 1;
        fprintf(e->out, "load %u\n", size_slot);
        if (i == count)
            fprintf(e->out, "pushu %zu ulong\n", size);
        else
            fprintf(e->out, "load %u\n", slot + i + 2);
        fprintf(e->out, "mul ulong\nstore %u ulong\n", size_slot);
    }
    fprintf(e->out, "load %u\nstackalloc\nstore %u ptr\n", slot + 1, slot);
    e->locals[e->count++] = (Local){d->as.variable.name, slot, d->as.variable.type, LOCAL_VLA};
    size_t id = 0;
    while (id < e->vla_definition_count && e->vla_definitions[id] != d)
        ++id;
    if (id == e->vla_definition_count) {
        fail(e, "variable-length array scope was not recorded");
        return 0;
    }
    e->active_vla_ids[e->active_vla_count] = (unsigned)id;
    e->active_vla_marks[e->active_vla_count++] = slot + 2;
    return 1;
}

/**
 * @brief Saves the runtime byte size of each pointed-to array dimension.
 * @param e IR emitter.
 * @param type Semantically validated pointer type.
 * @param slot First local slot reserved for sizes.
 * @return One on success, or zero with a diagnostic.
 */
static int emit_vla_pointer_sizes(Emitter *e, const Type *type, unsigned slot) {
    const Type *dimensions[64], *element = type->pointee;
    unsigned count = 0;
    size_t size, alignment;
    while (element && element->kind == TYPE_ARRAY) {
        if (count == sizeof dimensions / sizeof dimensions[0]) {
            fail(e, "too many variable-length array dimensions");
            return 0;
        }
        dimensions[count++] = element;
        element = element->pointee;
    }
    if (!count || e->next_slot > CRUNCH_LOCAL_LIMIT - count ||
        !type_layout_size(element, layout_find_struct, e->program, &size, &alignment)) {
        fail(e, "unsupported variable-length array layout or too many locals");
        return 0;
    }
    e->next_slot += count;
    for (unsigned i = 0; i < count; ++i) {
        const Type *array = dimensions[i];
        fprintf(e->out, "local %u ulong\n", slot + i);
        if (array->array_bound && array->array_bound->variable) {
            if (!expression_as(e, array->array_bound->expression, &TYPE_ULONG_VALUE))
                return 0;
        } else
            fprintf(e->out, "pushu %zu ulong\n", array->array_length);
        fprintf(e->out, "store %u ulong\n", slot + i);
    }
    for (unsigned i = count; i > 0; --i) {
        unsigned size_slot = slot + i - 1;
        fprintf(e->out, "load %u\n", size_slot);
        if (i == count)
            fprintf(e->out, "pushu %zu ulong\n", size);
        else
            fprintf(e->out, "load %u\n", size_slot + 1);
        fprintf(e->out, "mul ulong\nstore %u ulong\n", size_slot);
    }
    return 1;
}

/**
 * @brief Records active VLA declarations at each label before IR emission.
 * @param e IR emitter.
 * @param s Statement to inspect.
 * @param active Stack of active VLA identifiers.
 * @param count Number of active identifiers.
 * @param depth Current recursion depth.
 * @return One on success, or zero with a diagnostic.
 */
static int collect_vla_labels(Emitter *e, const Stmt *s, unsigned *active,
                              size_t *count, unsigned depth) {
    if (!s)
        return 1;
    if (depth >= IR_MAX_STATEMENT_ANALYSIS_DEPTH) {
        fail(e, "maximum VLA scope nesting depth exceeded");
        return 0;
    }
    size_t saved = *count;
    switch (s->kind) {
    case STMT_DECLARATION: {
        const Decl *d = s->as.declaration.declaration;
        if (d && d->kind == DECL_VARIABLE && d->as.variable.semantic_vla) {
            if (*count >= 512 || e->vla_definition_count >= 512) {
                fail(e, "too many variable-length arrays");
                return 0;
            }
            active[(*count)++] = (unsigned)e->vla_definition_count;
            e->vla_definitions[e->vla_definition_count++] = d;
        }
        break;
    }
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.statements.count; ++i)
            if (!collect_vla_labels(e, s->as.block.statements.items[i], active, count, depth + 1))
                return 0;
        *count = saved;
        break;
    case STMT_LABEL:
        if (s->as.label.id >= 512) {
            fail(e, "too many labels in a function with variable-length arrays");
            return 0;
        }
        for (size_t i = 0; i < *count; ++i)
            e->vla_label_bits[s->as.label.id][active[i] / 64] |= UINT64_C(1) << (active[i] % 64);
        return collect_vla_labels(e, s->as.label.statement, active, count, depth + 1);
    case STMT_IF:
        if (!collect_vla_labels(e, s->as.if_stmt.then_branch, active, count, depth + 1))
            return 0;
        *count = saved;
        if (!collect_vla_labels(e, s->as.if_stmt.else_branch, active, count, depth + 1))
            return 0;
        *count = saved;
        break;
    case STMT_FOR: {
        const Stmt *init = s->as.for_stmt.initializer;
        if (init && init->kind == STMT_BLOCK) {
            for (size_t i = 0; i < init->as.block.statements.count; ++i)
                if (!collect_vla_labels(e, init->as.block.statements.items[i], active, count,
                                        depth + 1))
                    return 0;
        } else if (!collect_vla_labels(e, init, active, count, depth + 1))
            return 0;
        if (!collect_vla_labels(e, s->as.for_stmt.body, active, count, depth + 1))
            return 0;
        *count = saved;
        break;
    }
    case STMT_LOOP:
    case STMT_DO:
        if (!collect_vla_labels(e, s->as.loop_stmt.body, active, count, depth + 1))
            return 0;
        *count = saved;
        break;
    case STMT_SWITCH:
        for (size_t i = 0; i < s->as.switch_stmt.prefix.count; ++i)
            if (!collect_vla_labels(e, s->as.switch_stmt.prefix.items[i], active, count, depth + 1))
                return 0;
        for (size_t i = 0; i < s->as.switch_stmt.cases.count; ++i) {
            if (*count != saved) {
                fail(e, "switch cannot jump into the scope of a variable-length array");
                return 0;
            }
            const SwitchCase *c = &s->as.switch_stmt.cases.items[i];
            for (size_t j = 0; j < c->statements.count; ++j)
                if (!collect_vla_labels(e, c->statements.items[j], active, count, depth + 1))
                    return 0;
        }
        *count = saved;
        break;
    default:
        break;
    }
    return 1;
}

/**
 * @brief Emits a static binary80 object and its constant initializer.
 * @param e IR emitter.
 * @param name IR global symbol.
 * @param initializer Constant initializer, or NULL for zero initialization.
 * @return Nonzero on success.
 */
static int emit_extended_static(Emitter *e, const char *name, const Expr *initializer) {
    fprintf(e->out, "globalblock %s 16 16\n", name);
    if (initializer) {
        long double value;
        unsigned char bytes[10];
        if (sizeof(value) < sizeof(bytes) || !sema_numeric_constant(initializer, &value)) {
            fail(e, "long double initializer must be a numeric constant");
            return 0;
        }
        memcpy(bytes, &value, sizeof(bytes));
        fprintf(e->out, "blockdata %s ", name);
        for (size_t i = 0; i < sizeof(bytes); ++i)
            fprintf(e->out, "%02x", bytes[i]);
        fputc('\n', e->out);
    }
    return 1;
}

/**
 * @brief Emits IR for a statement without applying the recursion guard wrapper.
 * @param e Emitter state containing output, locals, and diagnostic context.
 * @param s Statement to emit; a null statement emits nothing.
 * @return One on success, or zero when emission fails.
 */
static int statement_impl(Emitter *e, const Stmt *s) {
    if (!s)
        return 1;
    e->location = s->location;
    switch (s->kind) {
    case STMT_BLOCK: {
        size_t scope = e->count;
        size_t vla_scope = e->active_vla_count;
        for (size_t i = 0; i < s->as.block.statements.count; i++)
            if (!statement(e, s->as.block.statements.items[i])) {
                release_local_names(e, scope);
                e->count = scope;
                return 0;
            }
        if (e->active_vla_count > vla_scope)
            emit_stack_restore(e, e->active_vla_marks[vla_scope]);
        release_local_names(e, scope);
        e->count = scope;
        e->active_vla_count = vla_scope;
        return 1;
    }
    case STMT_DECLARATION: {
        Decl *d = s->as.declaration.declaration;
        unsigned slot;
        if (d && (d->kind == DECL_FUNCTION ||
                  (d->kind == DECL_VARIABLE && d->is_extern))) {
            if (e->count >= CRUNCH_LOCAL_LIMIT) {
                fail(e, "too many local declarations");
                return 0;
            }
            const char *name = d->kind == DECL_FUNCTION ? d->as.function.name
                                                         : d->as.variable.name;
            e->locals[e->count++] = (Local){name, 0, NULL, LOCAL_EXTERN};
            return 1;
        }
        if (d && (d->kind == DECL_TYPE_ALIAS || d->kind == DECL_ENUM ||
                  d->kind == DECL_STATIC_ASSERT ||
                  d->kind == DECL_STRUCT_SCOPE))
            return 1;
        if (!d || d->kind != DECL_VARIABLE) {
            fail(e, "invalid local declaration");
            return 0;
        }
        if (d->as.variable.type && d->as.variable.type->kind == TYPE_LONG_DOUBLE) {
            if (e->count >= CRUNCH_LOCAL_LIMIT || (!d->is_static && e->next_slot >= CRUNCH_LOCAL_LIMIT)) {
                fail(e, "too many long double locals");
                return 0;
            }
            if (d->is_static) {
                if (d->as.variable.initializer.kind != INIT_NONE &&
                    d->as.variable.initializer.kind != INIT_EXPRESSION) {
                    fail(e, "unsupported long double initializer");
                    return 0;
                }
                char name[64];
                unsigned id = static_local_symbol(e, name, sizeof(name));
                e->locals[e->count++] =
                    (Local){d->as.variable.name, id, d->as.variable.type, LOCAL_EXTENDED_STATIC};
                FILE *body = e->out;
                e->out = e->static_out;
                fprintf(e->out, "internal %s\n", name);
                int ok = emit_extended_static(e, name, d->as.variable.initializer.expression);
                e->out = body;
                return ok;
            }
            slot = e->next_slot++;
            e->locals[e->count++] =
                (Local){d->as.variable.name, slot, d->as.variable.type, LOCAL_EXTENDED};
            fprintf(e->out, "localblock __extended_local_%u 16 16\n"
                            "localalias __extended_local_address_%u __extended_local_%u 0 byte\n",
                    slot, slot, slot);
            if (d->as.variable.initializer.kind == INIT_EXPRESSION) {
                fprintf(e->out, "laddr __extended_local_address_%u\n", slot);
                if (!expression_as(e, d->as.variable.initializer.expression, d->as.variable.type))
                    return 0;
                fputs("extcopy\ndrop\n", e->out);
            } else if (d->as.variable.initializer.kind != INIT_NONE) {
                fail(e, "unsupported long double initializer");
                return 0;
            }
            return 1;
        }
        if (d->as.variable.semantic_vla)
            return emit_local_vla(e, d);
        if (d->as.variable.type && d->as.variable.type->kind == TYPE_ARRAY)
            return emit_fixed_local_array(e, d);
        if (d->is_static && d->as.variable.type && d->as.variable.type->kind == TYPE_STRUCT)
            return emit_static_local_struct(e, d);
        if (d->is_static)
            return emit_static_local_scalar(e, d);
        if (d->as.variable.type && d->as.variable.type->kind == TYPE_STRUCT) {
            if (d->as.variable.initializer.kind != INIT_NONE &&
                d->as.variable.initializer.kind != INIT_EXPRESSION) {
                fail(e, "unsupported structure initializer");
                return 0;
            }
            if (e->count >= CRUNCH_LOCAL_LIMIT || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
                fail(e, "too many local variables");
                return 0;
            }
            slot = e->next_slot++;
            e->locals[e->count++] = (Local){d->as.variable.name, slot, d->as.variable.type, 0};
            fprintf(e->out, "local %u int\n", slot);
            Decl *sd = find_struct(e->program, d->as.variable.type->struct_name);
            if (!sd) {
                fail(e, "unknown structure type");
                return 0;
            }
            size_t offsets[256], sz = 0, al = 0;
            char block[256];
            if (sd->as.structure.field_count > 256 ||
                !struct_layout(e->program, sd, offsets, &sz, &al) ||
                snprintf(block, sizeof block, "__local_struct_%u", slot) >= (int)sizeof block) {
                fail(e, "unsupported or oversized local structure layout");
                return 0;
            }
            if (d->semantic_alignment > al)
                al = d->semantic_alignment;
            fprintf(e->out, "localblock %s %zu %zu\n", block, sz, al);
            fprintf(e->out, "localalias __local_struct_address_%u %s 0 byte\n", slot, block);
            if (!register_local_fields(e, d->as.variable.name, block, sd, 0, 0, 0)) {
                fail(e, "unsupported or oversized nested structure fields");
                return 0;
            }
            if (d->as.variable.initializer.kind == INIT_EXPRESSION) {
                if (d->as.variable.initializer.expression &&
                    d->as.variable.initializer.expression->kind == EXPR_ARRAY_INIT) {
                    if (e->next_slot >= CRUNCH_LOCAL_LIMIT) {
                        fail(e, "too many locals for structure initializer");
                        return 0;
                    }
                    unsigned index = e->next_slot++;
                    char address[64];
                    snprintf(address, sizeof address, "__local_struct_address_%u", slot);
                    fprintf(e->out, "local %u long\n", index);
                    emit_array_byte_loop(e, address, 0, index, sz, 0, 0);
                    if (d->as.variable.initializer.semantic_plan)
                        return emit_initializer_plan(e, d->as.variable.initializer.semantic_plan,
                                                     address, index, NULL);
                    return emit_struct_init_value(e, d->as.variable.type,
                                                  d->as.variable.initializer.expression,
                                                  address, 0, index, NULL, 0);
                }
                Expr target = {0};
                target.kind = EXPR_NAME;
                target.as.name.name = d->as.variable.name;
                target.as.name.length = d->as.variable.name_length;
                if (!emit_struct_assignment(e, &target, d->as.variable.initializer.expression,
                                            d->as.variable.type)) {
                    fail(e, "structure initializer requires a matching structure value");
                    return 0;
                }
                fputs("drop\n", e->out);
            }
            return 1;
        }
        if (!isnum(d->as.variable.type) && !is_value_pointer(d->as.variable.type)) {
            fail(e,
                 "numeric scalar and supported pointer locals are supported by this IR milestone");
            return 0;
        }
        if (e->count >= CRUNCH_LOCAL_LIMIT || e->next_slot >= CRUNCH_LOCAL_LIMIT) {
            fail(e, "too many local variables");
            return 0;
        }
        slot = e->next_slot++;
        if (d->as.variable.semantic_vla_pointer &&
            !emit_vla_pointer_sizes(e, d->as.variable.type, slot + 1))
            return 0;
        if (d->semantic_alignment) {
            size_t size, alignment;
            if (!type_layout_size(d->as.variable.type, layout_find_struct, e->program,
                                  &size, &alignment)) {
                fail(e, "unsupported aligned local layout");
                return 0;
            }
            if (d->semantic_alignment > alignment)
                alignment = d->semantic_alignment;
            e->locals[e->count++] =
                (Local){d->as.variable.name, slot, d->as.variable.type, LOCAL_ALIGNED_SCALAR};
            fprintf(e->out, "localblock __aligned_local_%u %zu %zu\n"
                            "localalias __aligned_scalar_%u __aligned_local_%u 0 %s\n",
                    slot, size, alignment, slot, slot, tn(d->as.variable.type));
        } else {
            e->locals[e->count++] = (Local){d->as.variable.name, slot, d->as.variable.type, 0};
            fprintf(e->out, "local %u %s\n", slot, tn(d->as.variable.type));
        }
        if (d->as.variable.type->kind == TYPE_POINTER &&
            d->as.variable.initializer.kind == INIT_NONE) {
            fputs("push 0\n", e->out);
            emit_local_store(e, d->as.variable.name, slot, d->as.variable.type);
        }
        if (d->as.variable.initializer.kind == INIT_EXPRESSION) {
            if (!expression_as(e, d->as.variable.initializer.expression, d->as.variable.type))
                return 0;
            emit_local_store(e, d->as.variable.name, slot, d->as.variable.type);
        } else if (d->as.variable.initializer.kind != INIT_NONE) {
            fail(e, "initializer not yet supported by Crunch IR");
            return 0;
        }
        return 1;
    }
    case STMT_EXPRESSION:
        if (s->as.expression.expression) {
            const Expr *x = s->as.expression.expression;
            const Type *expression_type = expr_type(e, x);
            if (expression_type && expression_type->kind == TYPE_STRUCT)
                return emit_discarded_expression(e, x, 0);
            unsigned slot;
            const Type *t;
            if (x->kind == EXPR_UNARY &&
                (x->as.unary.op == UNARY_POST_INCREMENT ||
                 x->as.unary.op == UNARY_POST_DECREMENT) &&
                x->as.unary.operand && x->as.unary.operand->kind == EXPR_NAME &&
                lookup(e, x->as.unary.operand->as.name.name, &slot, &t) &&
                t->kind != TYPE_LONG_DOUBLE) {
                const Type *optype = t->kind == TYPE_BYTE || promoted_small_kind(t->kind)
                                         ? promote(t, t, e->program->c11_numeric) : t;
                emit_local_load(e, x->as.unary.operand->as.name.name, slot, t);
                if (t->kind == TYPE_POINTER) {
                    fputs("push 1\n", e->out);
                    if (!emit_pointer_offset(e, t, x->as.unary.op == UNARY_POST_DECREMENT))
                        return 0;
                } else {
                    if (t->kind != optype->kind)
                        fprintf(e->out, "cast %s %s\n", tn(t), tn(optype));
                    if (t->kind == TYPE_REAL)
                        fprintf(e->out, "pushr 0x3ff0000000000000\n");
                    else if (t->kind == TYPE_FLOAT)
                        fprintf(e->out, "pushf 0x3f800000\n");
                    else
                        fprintf(e->out, "push 1\n");
                    fprintf(e->out, "%s %s\n",
                            x->as.unary.op == UNARY_POST_DECREMENT ? "sub" : "add", tn(optype));
                }
                if (t->kind != optype->kind)
                    fprintf(e->out, "cast %s %s\n", tn(optype), tn(t));
                emit_local_store(e, x->as.unary.operand->as.name.name, slot, t);
                return 1;
            }
            if (!expression(e, x))
                return 0;
            if (e->module || !expression_is_noreturn(e->program, x, e->module))
                fprintf(e->out, "drop\n");
        }
        return 1;
    case STMT_IF: {
        unsigned other = label(e), end = label(e);
        if (!expression(e, s->as.if_stmt.condition))
            return 0;
        emit_truth(e, expr_type(e, s->as.if_stmt.condition));
        fprintf(e->out, "jz L%u\n", other);
        if (!statement(e, s->as.if_stmt.then_branch))
            return 0;
        if (s->as.if_stmt.else_branch) {
            fprintf(e->out, "jmp L%u\n", end);
            mark(e, other);
            if (!statement(e, s->as.if_stmt.else_branch))
                return 0;
            mark(e, end);
        } else
            mark(e, other);
        return 1;
    }
    case STMT_SWITCH:
        return switch_statement(e, s);
    case STMT_FOR: {
        size_t scope = e->count;
        size_t vla_scope = e->active_vla_count;
        unsigned cond = label(e), end = label(e), step = label(e);
        if (s->as.for_stmt.initializer) {
            const Stmt *init = s->as.for_stmt.initializer;
            if (init->kind == STMT_BLOCK) {
                for (size_t i = 0; i < init->as.block.statements.count; i++)
                    if (!statement(e, init->as.block.statements.items[i]))
                        return 0;
            } else if (!statement(e, init))
                return 0;
        }
        mark(e, cond);
        if (s->as.for_stmt.condition) {
            if (!expression(e, s->as.for_stmt.condition))
                return 0;
            emit_truth(e, expr_type(e, s->as.for_stmt.condition));
            fprintf(e->out, "jz L%u\n", end);
        }
        if (e->break_depth >= 128) {
            fail(e, "loop nesting exceeds IR limit");
            return 0;
        }
        e->break_vla_counts[e->break_depth] = e->active_vla_count;
        e->break_labels[e->break_depth++] = end;
        e->continue_vla_counts[e->continue_depth] = e->active_vla_count;
        e->continue_labels[e->continue_depth++] = step;
        if (!statement(e, s->as.for_stmt.body)) {
            e->break_depth--;
            e->continue_depth--;
            return 0;
        }
        e->break_depth--;
        e->continue_depth--;
        mark(e, step);
        if (s->as.for_stmt.update) {
            if (!expression(e, s->as.for_stmt.update))
                return 0;
            fprintf(e->out, "drop\n");
        }
        fprintf(e->out, "jmp L%u\n", cond);
        mark(e, end);
        if (e->active_vla_count > vla_scope)
            emit_stack_restore(e, e->active_vla_marks[vla_scope]);
        release_local_names(e, scope);
        e->count = scope;
        e->active_vla_count = vla_scope;
        return 1;
    }
    case STMT_LOOP: {
        unsigned cond = label(e), end = label(e);
        mark(e, cond);
        if (!expression(e, s->as.loop_stmt.condition))
            return 0;
        emit_truth(e, expr_type(e, s->as.loop_stmt.condition));
        fprintf(e->out, "jz L%u\n", end);
        if (e->break_depth >= 128) {
            fail(e, "loop nesting exceeds IR limit");
            return 0;
        }
        e->break_vla_counts[e->break_depth] = e->active_vla_count;
        e->break_labels[e->break_depth++] = end;
        e->continue_vla_counts[e->continue_depth] = e->active_vla_count;
        e->continue_labels[e->continue_depth++] = cond;
        if (!statement(e, s->as.loop_stmt.body)) {
            e->break_depth--;
            e->continue_depth--;
            return 0;
        }
        e->break_depth--;
        e->continue_depth--;
        fprintf(e->out, "jmp L%u\n", cond);
        mark(e, end);
        return 1;
    }
    case STMT_DO: {
        unsigned body = label(e), cond = label(e), end = label(e);
        if (e->break_depth >= 128) {
            fail(e, "loop nesting exceeds IR limit");
            return 0;
        }
        mark(e, body);
        e->break_vla_counts[e->break_depth] = e->active_vla_count;
        e->break_labels[e->break_depth++] = end;
        e->continue_vla_counts[e->continue_depth] = e->active_vla_count;
        e->continue_labels[e->continue_depth++] = cond;
        if (!statement(e, s->as.loop_stmt.body)) {
            e->break_depth--;
            e->continue_depth--;
            return 0;
        }
        e->break_depth--;
        e->continue_depth--;
        mark(e, cond);
        if (!expression(e, s->as.loop_stmt.condition))
            return 0;
        emit_truth(e, expr_type(e, s->as.loop_stmt.condition));
        fprintf(e->out, "jz L%u\njmp L%u\n", end, body);
        mark(e, end);
        return 1;
    }
    case STMT_CONTINUE:
        if (!e->continue_depth) {
            fail(e, "continue outside loop");
            return 0;
        }
        if (e->active_vla_count > e->continue_vla_counts[e->continue_depth - 1])
            emit_stack_restore(e, e->active_vla_marks[e->continue_vla_counts[e->continue_depth - 1]]);
        fprintf(e->out, "jmp L%u\n", e->continue_labels[e->continue_depth - 1]);
        return 1;
    case STMT_BREAK:
        if (!e->break_depth) {
            fail(e, "break outside loop or switch");
            return 0;
        }
        if (e->active_vla_count > e->break_vla_counts[e->break_depth - 1])
            emit_stack_restore(e, e->active_vla_marks[e->break_vla_counts[e->break_depth - 1]]);
        fprintf(e->out, "jmp L%u\n", e->break_labels[e->break_depth - 1]);
        return 1;
    case STMT_GOTO:
    case STMT_LABEL:
        if (s->as.label.id >= e->goto_label_count) {
            fail(e, "unresolved goto label");
            return 0;
        }
        if (s->kind == STMT_GOTO) {
            if (e->function_has_vla) {
                uint64_t active[8] = {0};
                for (size_t i = 0; i < e->active_vla_count; ++i) {
                    unsigned id = e->active_vla_ids[i];
                    active[id / 64] |= UINT64_C(1) << (id % 64);
                }
                for (size_t i = 0; i < 8; ++i) {
                    if (e->vla_label_bits[s->as.label.id][i] & ~active[i]) {
                        fail(e, "goto cannot enter the scope of a variable-length array");
                        return 0;
                    }
                }
                for (size_t i = 0; i < e->active_vla_count; ++i) {
                    unsigned id = e->active_vla_ids[i];
                    if (!(e->vla_label_bits[s->as.label.id][id / 64] &
                          (UINT64_C(1) << (id % 64)))) {
                        emit_stack_restore(e, e->active_vla_marks[i]);
                        break;
                    }
                }
            }
            fprintf(e->out, "jmp L%u\n", e->goto_label_base + (unsigned)s->as.label.id);
            return 1;
        }
        mark(e, e->goto_label_base + (unsigned)s->as.label.id);
        return statement(e, s->as.label.statement);
    case STMT_RETURN:
        if (e->return_type && e->return_type->kind == TYPE_LONG_DOUBLE) {
            if (!s->as.return_stmt.expression ||
                !expression_as(e, s->as.return_stmt.expression, e->return_type))
                return 0;
            fputs("retagg ext\n", e->out);
            return 1;
        }
        if (e->return_type && e->return_type->kind == TYPE_STRUCT) {
            size_t size = aggregate_parameter_size(e->program, e->return_type);
            if (s->as.return_stmt.expression) {
                if (!emit_aggregate_argument(e, s->as.return_stmt.expression, size))
                    return 0;
            } else if (!emit_aggregate_result_buffer(e, size))
                return 0;
            fputs("retagg ", e->out);
            emit_parameter_type(e->out, e->program, e->return_type);
            fputc('\n', e->out);
            return 1;
        }
        if (s->as.return_stmt.expression) {
            if (!expression_as(e, s->as.return_stmt.expression, e->return_type))
                return 0;
        } else
            fprintf(e->out, "push 0\n");
        fprintf(e->out, "ret %s\n", tn(e->return_type));
        return 1;
    default:
        fail(e, "statement kind not yet supported by Crunch IR");
        return 0;
    }
}
/**
 * @brief Emits a statement while enforcing the IR traversal depth limit.
 * @param e IR emitter state containing output and diagnostic context.
 * @param s Statement node to emit.
 * @return One on success; zero on unsupported input or excessive nesting.
 */
static int statement(Emitter *e, const Stmt *s) {
    if (e->statement_depth >= IR_MAX_STATEMENT_ANALYSIS_DEPTH) {
        e->recursion_limit_exceeded = 1;
        fail(e, "maximum IR statement nesting depth exceeded");
        return 0;
    }

    ++e->statement_depth;
    int result = statement_impl(e, s);
    --e->statement_depth;
    return result;
}

static int constant_real_value(Emitter *e, const Expr *x, double *value, unsigned depth);
static int constant_integer_initializer(Emitter *e, const Expr *x, int64_t *value,
                                        unsigned depth);

typedef struct {
    Emitter *emitter;
    unsigned depth;
} ConstantComparisonContext;

/**
 * @brief Resolves real comparisons and real conditions in integer expressions.
 * @param x Expression to inspect.
 * @param value Receives the integer comparison result.
 * @param context Emitter and recursion context.
 * @return True when the comparison was evaluated successfully.
 */
static bool resolve_real_comparison(const Expr *x, int64_t *value, void *context) {
    ConstantComparisonContext *ctx = context;
    if (x->kind == EXPR_CONDITIONAL) {
        const Type *type = expr_type(ctx->emitter, x->as.conditional.condition);
        if (!type || type->kind != TYPE_REAL)
            return false;
        double condition;
        if (!constant_real_value(ctx->emitter, x->as.conditional.condition,
                                 &condition, ctx->depth + 1))
            return false;
        const Expr *selected = condition != 0.0 ? x->as.conditional.then_expr
                                                : x->as.conditional.else_expr;
        return constant_integer_initializer(ctx->emitter, selected, value, ctx->depth + 1);
    }
    if (x->kind != EXPR_BINARY)
        return false;
    BinaryOp op = x->as.binary.op;
    if (op < BINARY_EQUAL || op > BINARY_GREATER_EQUAL)
        return false;
    const Type *left_type = expr_type(ctx->emitter, x->as.binary.left);
    const Type *right_type = expr_type(ctx->emitter, x->as.binary.right);
    if (!left_type || !right_type || !isnum(left_type) || !isnum(right_type) ||
        (left_type->kind != TYPE_REAL && right_type->kind != TYPE_REAL))
        return false;
    double left, right;
    if (!constant_real_value(ctx->emitter, x->as.binary.left, &left, ctx->depth + 1) ||
        !constant_real_value(ctx->emitter, x->as.binary.right, &right, ctx->depth + 1))
        return false;
    switch (op) {
    case BINARY_EQUAL: *value = left == right; break;
    case BINARY_NOT_EQUAL: *value = left != right; break;
    case BINARY_LESS: *value = left < right; break;
    case BINARY_LESS_EQUAL: *value = left <= right; break;
    case BINARY_GREATER: *value = left > right; break;
    case BINARY_GREATER_EQUAL: *value = left >= right; break;
    default: return false;
    }
    return true;
}

/**
 * @brief Evaluates an integer initializer including real comparisons and conditions.
 * @param e Emitter containing expression type information.
 * @param x Expression to evaluate.
 * @param value Receives the evaluated integer.
 * @param depth Current recursion depth.
 * @return Nonzero on success, or zero for invalid or unsupported expressions.
 */
static int constant_integer_initializer(Emitter *e, const Expr *x, int64_t *value,
                                        unsigned depth) {
    ConstantComparisonContext context = {e, depth};
    return integer_initializer_expression(x, depth) &&
           constant_expr_integer_with_resolver(x, value, resolve_real_comparison, &context);
}

/**
 * @brief Interprets an integer bit pattern as a signed two's-complement value.
 * @param bits Canonical 64-bit integer representation.
 * @return Corresponding signed value.
 */
static int64_t signed_constant_bits(uint64_t bits) {
    return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
}

static int integer_initializer_bits(Emitter *, const Expr *, TypeKind, uint64_t *, unsigned);

/**
 * @brief Evaluates a supported numeric initializer as a binary64 value.
 * @param e Emitter containing expression type information.
 * @param x Expression to evaluate.
 * @param value Receives the evaluated value.
 * @param depth Current recursion depth.
 * @return Nonzero on success, or zero for invalid or unsupported expressions.
 */
static int constant_real_value(Emitter *e, const Expr *x, double *value, unsigned depth) {
    if (!x || depth >= IR_MAX_AST_RECURSION_DEPTH)
        return 0;
    const Type *type = expr_type(e, x);
    if (!type || !isnum(type))
        return 0;
    if (type->kind != TYPE_REAL) {
        if (e->program->c11_numeric) {
            long double number;
            if (sema_numeric_constant(x, &number)) {
                *value = (double)number;
                return 1;
            }
            if (type->kind != TYPE_FLOAT) {
                uint64_t bits;
                if (integer_initializer_bits(e, x, type->kind, &bits, depth + 1)) {
                    *value = type->kind == TYPE_BYTE || type->kind == TYPE_UNSIGNED_CHAR ||
                                     type->kind == TYPE_UNSIGNED_SHORT ||
                                     type->kind == TYPE_UNSIGNED_INT ||
                                     type->kind == TYPE_UNSIGNED_LONG ||
                                     type->kind == TYPE_UNSIGNED_LONG_LONG
                                 ? (double)bits : (double)signed_constant_bits(bits);
                    return 1;
                }
            }
        }
        if (x->semantic_unsigned || type->kind == TYPE_UNSIGNED_INT ||
            type->kind == TYPE_UNSIGNED_LONG || type->kind == TYPE_UNSIGNED_LONG_LONG) {
            uint64_t bits;
            TypeKind kind;
            if (!sema_integer_constant(x, &bits, &kind))
                return 0;
            *value = kind == TYPE_UNSIGNED_CHAR || kind == TYPE_UNSIGNED_SHORT || kind == TYPE_UNSIGNED_INT ||
                             kind == TYPE_UNSIGNED_LONG || kind == TYPE_UNSIGNED_LONG_LONG || kind == TYPE_BYTE
                         ? (double)bits : (double)signed_constant_bits(bits);
            return 1;
        }
        int64_t integer;
        if (!constant_integer_initializer(e, x, &integer, depth))
            return 0;
        *value = (double)integer;
        return 1;
    }
    switch (x->kind) {
    case EXPR_REAL:
        *value = x->as.real_value;
        return 1;
    case EXPR_CAST:
        return x->as.cast.target_type && x->as.cast.target_type->kind == TYPE_REAL &&
               constant_real_value(e, x->as.cast.operand, value, depth + 1);
    case EXPR_CONDITIONAL: {
        double condition;
        if (!numeric_initializer_expression(x, depth) ||
            !constant_real_value(e, x->as.conditional.condition, &condition, depth + 1))
            return 0;
        return constant_real_value(e, condition != 0.0 ? x->as.conditional.then_expr
                                                        : x->as.conditional.else_expr,
                                   value, depth + 1);
    }
    case EXPR_UNARY:
        if (x->as.unary.op != UNARY_PLUS && x->as.unary.op != UNARY_MINUS)
            return 0;
        if (!constant_real_value(e, x->as.unary.operand, value, depth + 1))
            return 0;
        if (x->as.unary.op == UNARY_MINUS)
            *value = -*value;
        return 1;
    case EXPR_BINARY: {
        double left, right;
        switch (x->as.binary.op) {
        case BINARY_ADD:
        case BINARY_SUBTRACT:
        case BINARY_MULTIPLY:
        case BINARY_DIVIDE:
        case BINARY_POWER:
            break;
        default:
            return 0;
        }
        if (!constant_real_value(e, x->as.binary.left, &left, depth + 1) ||
            !constant_real_value(e, x->as.binary.right, &right, depth + 1))
            return 0;
        switch (x->as.binary.op) {
        case BINARY_ADD: *value = left + right; break;
        case BINARY_SUBTRACT: *value = left - right; break;
        case BINARY_MULTIPLY: *value = left * right; break;
        case BINARY_DIVIDE: *value = left / right; break;
        case BINARY_POWER: *value = _crunch_ir_pow_helper(left, right); break;
        default: return 0;
        }
        return 1;
    }
    default:
        return 0;
    }
}

static int fixed_address_constant(Emitter *, const Expr *, char *, size_t, int64_t *, unsigned);

/**
 * @brief Evaluates a C11-mode integer initializer using supported IR constant extensions.
 * @param e Emitter containing expression type information.
 * @param x Source constant expression.
 * @param target Destination integer type.
 * @param bits Receives the normalized destination bits.
 * @param depth Current recursion depth.
 * @return Nonzero for a supported constant conversion.
 */
static int integer_initializer_bits(Emitter *e, const Expr *x, TypeKind target, uint64_t *bits,
                                    unsigned depth) {
    if (!x || depth >= IR_MAX_AST_RECURSION_DEPTH)
        return 0;
    const Expr *pointer = x;
    if (x->kind == EXPR_CAST && x->as.cast.target_type &&
        x->as.cast.target_type->kind == TYPE_BOOL)
        pointer = x->as.cast.operand;
    const Type *pointer_type = expr_type(e, pointer);
    if (target == TYPE_BOOL && pointer_type && pointer_type->kind == TYPE_POINTER) {
        if (is_null_literal(pointer)) {
            *bits = 0;
            return 1;
        }
        char symbol[128];
        int64_t addend;
        if (fixed_address_constant(e, pointer, symbol, sizeof symbol, &addend, 0)) {
            *bits = 1;
            return 1;
        }
    }
    if (sema_constant_integer_conversion(x, target, bits))
        return 1;
    int64_t integer;
    if (constant_integer_initializer(e, x, &integer, depth)) {
        Expr value = {0};
        value.kind = EXPR_INTEGER;
        value.semantic_type = TYPE_LONG;
        value.as.integer_value = integer;
        return sema_constant_integer_conversion(&value, target, bits);
    }
    const Type *from = expr_type(e, x);
    if (from && (from->kind == TYPE_REAL || from->kind == TYPE_FLOAT)) {
        double number;
        if (constant_real_value(e, x, &number, depth + 1)) {
            Expr value = {0};
            value.kind = EXPR_REAL;
            value.semantic_type = TYPE_REAL;
            value.semantic_c11_numeric = true;
            value.as.real_value = number;
            return sema_constant_integer_conversion(&value, target, bits);
        }
    }
    if (x->kind == EXPR_CAST && x->as.cast.target_type &&
        (x->as.cast.operand->semantic_type == TYPE_REAL ||
         x->as.cast.operand->semantic_type == TYPE_FLOAT)) {
        uint64_t converted;
        if (integer_initializer_bits(e, x->as.cast.operand, type_effective_kind(x->as.cast.target_type),
                                     &converted, depth + 1)) {
            Expr value = {0};
            value.kind = EXPR_INTEGER;
            value.semantic_type = type_effective_kind(x->as.cast.target_type);
            value.semantic_c11_numeric = true;
            value.as.integer_value = signed_constant_bits(converted);
            return sema_constant_integer_conversion(&value, target, bits);
        }
    }
    return 0;
}

/**
 * @brief Evaluates one numeric static initializer as target-format bits.
 * @param e IR emitter.
 * @param type Destination element type.
 * @param value Initializer expression.
 * @param bits Receives the destination bit pattern.
 * @return Nonzero for a supported constant initializer.
 */
static int fixed_array_numeric_bits(Emitter *e, const Type *type, const Expr *value,
                                    uint64_t *bits) {
    if (e->program->c11_numeric && type->kind != TYPE_FLOAT && type->kind != TYPE_REAL)
        return integer_initializer_bits(e, value, type_effective_kind(type), bits, 0);
    if (promoted_small_kind(type->kind))
        return sema_constant_integer_conversion(value, type->kind, bits);
    if (type->kind != TYPE_FLOAT && type->kind != TYPE_REAL && value->semantic_float)
        return sema_constant_integer_conversion(value, type->kind, bits);
    if (type->kind == TYPE_FLOAT || value->semantic_float) {
        long double result;
        uint64_t integer_bits;
        TypeKind integer_kind;
        const Type *from = expr_type(e, value);
        if (from && from->kind != TYPE_FLOAT && from->kind != TYPE_REAL &&
            sema_integer_constant(value, &integer_bits, &integer_kind))
            result = integer_kind == TYPE_UNSIGNED_CHAR || integer_kind == TYPE_UNSIGNED_SHORT || integer_kind == TYPE_UNSIGNED_INT ||
                             integer_kind == TYPE_UNSIGNED_LONG || integer_kind == TYPE_UNSIGNED_LONG_LONG ||
                             integer_kind == TYPE_BYTE
                         ? (long double)integer_bits
                         : (long double)signed_constant_bits(integer_bits);
        else if (!sema_numeric_constant(value, &result))
            return 0;
        if (type->kind == TYPE_FLOAT) {
            *bits = sema_binary32_bits(result);
        } else if (type->kind == TYPE_REAL) {
            double number = (double)result;
            memcpy(bits, &number, sizeof number);
        }
        return 1;
    }
    if (type->kind == TYPE_REAL) {
        double number;
        if (!constant_real_value(e, value, &number, 0))
            return 0;
        memcpy(bits, &number, sizeof number);
        return 1;
    }
    if (value->semantic_unsigned || type->kind == TYPE_UNSIGNED_INT ||
        type->kind == TYPE_UNSIGNED_LONG || type->kind == TYPE_UNSIGNED_LONG_LONG) {
        TypeKind kind;
        return sema_integer_constant(value, bits, &kind) ||
               sema_constant_integer_conversion(value, type->kind, bits);
    }
    int64_t number;
    if (!constant_integer_initializer(e, value, &number, 0))
        return sema_constant_integer_conversion(value, type->kind, bits);
    *bits = (uint64_t)number;
    return 1;
}

/**
 * @brief Adds a constant scaled index to an address relocation.
 * @param e IR emitter.
 * @param pointer Pointer type determining the element stride.
 * @param index Constant integer expression.
 * @param subtract Nonzero to subtract the offset.
 * @param addend Existing relocation addend, updated on success.
 * @return Nonzero when the offset is representable.
 */
static int fixed_address_offset(Emitter *e, const Type *pointer, const Expr *index,
                                int subtract, int64_t *addend) {
    size_t stride;
    int64_t count;
    if (!array_element_size(e->program, pointer, &stride) || !stride || stride > INT64_MAX)
        return 0;
    if (index && index->semantic_unsigned) {
        uint64_t bits;
        TypeKind kind;
        if (!sema_integer_constant(index, &bits, &kind))
            return 0;
        if (kind == TYPE_UNSIGNED_CHAR || kind == TYPE_UNSIGNED_SHORT || kind == TYPE_UNSIGNED_INT ||
            kind == TYPE_UNSIGNED_LONG || kind == TYPE_UNSIGNED_LONG_LONG || kind == TYPE_BYTE) {
            if (bits > INT64_MAX)
                return 0;
            count = (int64_t)bits;
        } else
            count = signed_constant_bits(bits);
    } else if (!constant_integer_initializer(e, index, &count, 0))
        return 0;
    int64_t scale = (int64_t)stride;
    if (count > INT64_MAX / scale || count < INT64_MIN / scale)
        return 0;
    int64_t offset = count * scale;
    if (subtract) {
        if (offset == INT64_MIN)
            return 0;
        offset = -offset;
    }
    if ((offset > 0 && *addend > INT64_MAX - offset) ||
        (offset < 0 && *addend < INT64_MIN - offset))
        return 0;
    *addend += offset;
    return 1;
}

/**
 * @brief Resolves a supported static pointer expression to a symbol and addend.
 * @param e IR emitter.
 * @param x Pointer-valued initializer expression.
 * @param symbol Buffer receiving the assembler-safe symbol.
 * @param capacity Size of the symbol buffer.
 * @param addend Receives the byte offset from the symbol.
 * @param depth Current expression recursion depth.
 * @return Nonzero for a supported address constant.
 */
static int fixed_address_constant(Emitter *e, const Expr *x, char *symbol, size_t capacity,
                                  int64_t *addend, unsigned depth) {
    if (!x || depth >= IR_MAX_AST_RECURSION_DEPTH)
        return 0;
    if (x->kind == EXPR_CONDITIONAL) {
        const Expr *selected = static_pointer_value(x);
        return selected != x &&
               fixed_address_constant(e, selected, symbol, capacity, addend, depth + 1);
    }
    if (x->kind == EXPR_CAST && x->as.cast.semantic_file_scope &&
        x->as.cast.target_type && x->as.cast.target_type->kind == TYPE_ARRAY &&
        x->as.cast.operand && x->as.cast.operand->kind == EXPR_ARRAY_INIT) {
        *addend = 0;
        return file_compound_symbol(e, x, symbol, capacity);
    }
    if (x->kind == EXPR_CAST && x->as.cast.target_type &&
        x->as.cast.target_type->kind == TYPE_POINTER)
        return fixed_address_constant(e, x->as.cast.operand, symbol, capacity, addend, depth + 1);
    if (x->kind == EXPR_STRING) {
        size_t count = string_array_count(x);
        if (x->string_literal_wide && count > SIZE_MAX / 4)
            return 0;
        count *= x->string_literal_wide ? 4 : 1;
        if (!count || count > (SIZE_MAX - 1) / 2)
            return 0;
        char *hex = malloc(count * 2 + 1);
        if (!hex) {
            fail_oom(e);
            return 0;
        }
        if (!string_hex(x, hex, count * 2 + 1)) {
            free(hex);
            return 0;
        }
        int written = snprintf(symbol, capacity, "__crunch_string_%u", e->next_string++);
        if (written < 0 || (size_t)written >= capacity) {
            free(hex);
            return 0;
        }
        fprintf(e->out, "stringlit %s %s%s\n", symbol, hex,
                x->string_literal_wide ? " 4" : "");
        free(hex);
        *addend = 0;
        return 1;
    }
    if (x->kind == EXPR_NAME) {
        unsigned slot;
        const Type *local_type;
        if (lookup(e, x->as.name.name, &slot, &local_type)) {
            if (!local_type || local_type->kind != TYPE_ARRAY ||
                local_alias(e, x->as.name.name) != LOCAL_STATIC_ARRAY)
                return 0;
            int written = snprintf(symbol, capacity, "_crunch_static_local_%u", slot);
            *addend = 0;
            return written >= 0 && (size_t)written < capacity;
        }
        Decl *global = find_global(e->program, x->as.name.name);
        if (!global && find_function(e->program, x->as.name.name)) {
            int written = snprintf(symbol, capacity, "%s", x->as.name.name);
            *addend = 0;
            return written >= 0 && (size_t)written < capacity;
        }
        if (!global || !global->as.variable.type ||
            global->as.variable.type->kind != TYPE_ARRAY)
            return 0;
        int written = snprintf(symbol, capacity, "%s", x->as.name.name);
        *addend = 0;
        return written >= 0 && (size_t)written < capacity;
    }
    if (x->kind == EXPR_MEMBER) {
        const Type *type = expr_type_raw(e, x);
        if (type && type->kind == TYPE_ARRAY) {
            Expr address = {0};
            address.kind = EXPR_UNARY;
            address.as.unary.op = UNARY_ADDRESS;
            address.as.unary.operand = (Expr *)x;
            return fixed_address_constant(e, &address, symbol, capacity, addend, depth + 1);
        }
    }
    if (x->kind == EXPR_UNARY && x->as.unary.op == UNARY_ADDRESS) {
        const Expr *operand = x->as.unary.operand;
        if (!operand)
            return 0;
        if (operand->kind == EXPR_CAST && operand->as.cast.semantic_file_scope &&
            operand->as.cast.operand && operand->as.cast.operand->kind == EXPR_ARRAY_INIT) {
            *addend = 0;
            return file_compound_symbol(e, operand, symbol, capacity);
        }
        if (operand->kind == EXPR_UNARY && operand->as.unary.op == UNARY_DEREFERENCE)
            return fixed_address_constant(e, operand->as.unary.operand, symbol, capacity,
                                          addend, depth + 1);
        if (operand->kind == EXPR_INDEX) {
            const Expr *base = operand->as.index.base;
            return fixed_address_constant(e, base, symbol, capacity, addend, depth + 1) &&
                   fixed_address_offset(e, expr_type(e, base), operand->as.index.index, 0, addend);
        }
        if (operand->kind == EXPR_NAME) {
            unsigned slot;
            const Type *local_type;
            if (lookup(e, operand->as.name.name, &slot, &local_type)) {
                if (!local_type ||
                    (local_alias(e, operand->as.name.name) != LOCAL_STATIC_ARRAY &&
                     local_alias(e, operand->as.name.name) != LOCAL_STATIC_SCALAR &&
                     local_alias(e, operand->as.name.name) != LOCAL_STATIC_STRUCT))
                    return 0;
                int written = snprintf(symbol, capacity, "_crunch_static_local_%u", slot);
                *addend = 0;
                return written >= 0 && (size_t)written < capacity;
            }
            Decl *global = find_global(e->program, operand->as.name.name);
            if (!global && !find_function(e->program, operand->as.name.name))
                return 0;
            int written = snprintf(symbol, capacity, "%s", operand->as.name.name);
            *addend = 0;
            return written >= 0 && (size_t)written < capacity;
        }
        if (operand->kind == EXPR_MEMBER) {
            size_t offset;
            if (!struct_field_offset(e->program, member_base_type(e, operand),
                                     operand->as.member.name, &offset, NULL) ||
                offset > INT64_MAX)
                return 0;
            Expr address = {0};
            address.kind = EXPR_UNARY;
            address.as.unary.op = UNARY_ADDRESS;
            address.as.unary.operand = operand->as.member.base;
            const Expr *base = operand->as.member.through_pointer
                                   ? operand->as.member.base : &address;
            if (!fixed_address_constant(e, base, symbol, capacity, addend, depth + 1) ||
                *addend > INT64_MAX - (int64_t)offset)
                return 0;
            *addend += (int64_t)offset;
            return 1;
        }
        return 0;
    }
    if (x->kind == EXPR_BINARY &&
        (x->as.binary.op == BINARY_ADD || x->as.binary.op == BINARY_SUBTRACT)) {
        const Expr *base = x->as.binary.left, *index = x->as.binary.right;
        const Type *pointer = expr_type(e, base);
        if ((!pointer || pointer->kind != TYPE_POINTER) && x->as.binary.op == BINARY_ADD) {
            base = x->as.binary.right;
            index = x->as.binary.left;
            pointer = expr_type(e, base);
        }
        return pointer && pointer->kind == TYPE_POINTER &&
               fixed_address_constant(e, base, symbol, capacity, addend, depth + 1) &&
               fixed_address_offset(e, pointer, index, x->as.binary.op == BINARY_SUBTRACT,
                                    addend);
    }
    return 0;
}

/**
 * @brief Emits a static local scalar using internal global storage.
 * @param e IR emitter.
 * @param d Local scalar declaration.
 * @return Nonzero on success, or zero with a diagnostic.
 */
static int emit_static_local_scalar(Emitter *e, const Decl *d) {
    const Type *type = d->as.variable.type;
    if (!isnum(type) && !is_value_pointer(type)) {
        fail(e, "static local requires a supported scalar type");
        return 0;
    }
    if (e->count >= CRUNCH_LOCAL_LIMIT) {
        fail(e, "too many local variables");
        return 0;
    }
    char name[64];
    unsigned id = static_local_symbol(e, name, sizeof name);
    e->locals[e->count++] = (Local){d->as.variable.name, id, type, LOCAL_STATIC_SCALAR};
    FILE *body = e->out;
    e->out = e->static_out;
    fprintf(e->out, "internal %s\n", name);
    const Expr *init = d->as.variable.initializer.expression;
    if (type->kind == TYPE_POINTER)
        init = static_pointer_value(init);
    int ok = 1;
    if (type->kind == TYPE_POINTER) {
        if (init && !is_null_literal(init)) {
            char symbol[128];
            int64_t addend;
            if (!fixed_address_constant(e, init, symbol, sizeof symbol, &addend, 0)) {
                fail(e, "static local pointer initializer must be an address constant");
                ok = 0;
            } else {
                fprintf(e->out, "global %s ptr 0\nglobalreloc %s %s %" PRId64 "\n",
                        name, name, symbol, addend);
            }
        } else
            fprintf(e->out, "global %s ptr 0\n", name);
    } else {
        uint64_t bits = 0;
        if (init && !fixed_array_numeric_bits(e, type, init, &bits)) {
            fail(e, "static local initializer must be a numeric constant");
            ok = 0;
        } else if (type->kind == TYPE_FLOAT)
            fprintf(e->out, "global %s float 0x%08" PRIx32 "\n", name, (uint32_t)bits);
        else if (type->kind == TYPE_REAL)
            fprintf(e->out, "global %s real 0x%016" PRIx64 "\n", name, bits);
        else if (type->kind == TYPE_UNSIGNED_LONG || type->kind == TYPE_UNSIGNED_LONG_LONG)
            fprintf(e->out, "global %s ulong %" PRIu64 "\n", name, bits);
        else if (type_effective_kind(type) == TYPE_UNSIGNED_INT)
            fprintf(e->out, "global %s uint %" PRIu32 "\n", name, (uint32_t)bits);
        else if (type->kind == TYPE_UNSIGNED_SHORT)
            fprintf(e->out, "global %s ushort %u\n", name, (unsigned)(bits & UINT64_C(0xffff)));
        else if (type->kind == TYPE_BYTE || type->kind == TYPE_UNSIGNED_CHAR)
            fprintf(e->out, "global %s %s %u\n", name, tn(type), (unsigned)(bits & UINT64_C(0xff)));
        else {
            if (type->kind == TYPE_INT || type->kind == TYPE_ENUM) {
                bits &= UINT64_C(0xffffffff);
                if (bits > INT32_MAX)
                    bits |= UINT64_C(0xffffffff00000000);
            }
            fprintf(e->out, "global %s %s %" PRId64 "\n", name, tn(type),
                    signed_constant_bits(bits));
        }
    }
    e->out = body;
    return ok;
}

/**
 * @brief Emits static storage and constant initializers for a global fixed array.
 * @param e IR emitter.
 * @param d Global array declaration.
 * @param name Linker symbol for the array storage.
 * @return Nonzero on success, or zero with a diagnostic.
 */
static int emit_fixed_global_array(Emitter *e, const Decl *d, const char *name) {
    const Type *array = d->as.variable.type, *element = array->pointee;
    size_t size, alignment, width, element_alignment;
    if (!type_layout_size(array, layout_find_struct, e->program, &size, &alignment) ||
        !type_layout_size(element, layout_find_struct, e->program, &width, &element_alignment) ||
        !size || size > UINT32_MAX) {
        fail(e, "unsupported or oversized global fixed array layout");
        return 0;
    }
    if (d->semantic_alignment > alignment)
        alignment = d->semantic_alignment;
    fprintf(e->out, "globalblock %s %zu %zu\n", name, size, alignment);
    const Expr *init = d->as.variable.initializer.expression;
    if (d->as.variable.initializer.kind == INIT_NONE)
        return 1;
    if (!init) {
        fail(e, "unsupported global fixed array initializer");
        return 0;
    }
    if (init->kind == EXPR_ARRAY_INIT && init->as.array_init.count == 1 &&
        string_element_matches(element, init->as.array_init.items[0]))
        init = init->as.array_init.items[0];
    if (d->as.variable.initializer.semantic_plan ||
        (element && (element->kind == TYPE_STRUCT || element->kind == TYPE_ARRAY ||
                     element->kind == TYPE_LONG_DOUBLE)))
        return emit_global_struct_initializer(e, array, init,
                                              d->as.variable.initializer.semantic_plan, name, size);
    if (string_element_matches(element, init)) {
        size_t count = string_array_count(init);
        size_t width = init->string_literal_wide ? 4 : 1;
        if (count > SIZE_MAX / width) {
            fail(e, "invalid global fixed array string initializer");
            return 0;
        }
        count *= width;
        if (!count || count > (SIZE_MAX - 1) / 2) {
            fail(e, "invalid global fixed array string initializer");
            return 0;
        }
        char *hex = malloc(count * 2 + 1);
        if (!hex) {
            fail_oom(e);
            return 0;
        }
        if (!string_hex(init, hex, count * 2 + 1)) {
            free(hex);
            fail(e, "invalid global fixed array string initializer");
            return 0;
        }
        if (count > array->array_length * width)
            hex[array->array_length * width * 2] = '\0';
        fprintf(e->out, "blockdata %s %s\n", name, hex);
        free(hex);
        return 1;
    }
    if (init->kind != EXPR_ARRAY_INIT ||
        (!isnum(element) && !is_value_pointer(element))) {
        fail(e, "global fixed array initializer requires constant scalar elements");
        return 0;
    }
    if (is_value_pointer(element)) {
        for (size_t i = 0; i < init->as.array_init.count; i++) {
            const Expr *value = static_pointer_value(init->as.array_init.items[i]);
            if (is_null_literal(value))
                continue;
            char symbol[128];
            int64_t addend;
            if (!fixed_address_constant(e, value, symbol, sizeof symbol, &addend, 0)) {
                fail(e, "global fixed array pointer initializer must be an address constant");
                return 0;
            }
            fprintf(e->out, "blockreloc %s %zu %s %" PRId64 "\n",
                    name, i * width, symbol, addend);
        }
        return 1;
    }
    size_t count = init->as.array_init.count;
    if (!count)
        return 1;
    if (count > (SIZE_MAX - 1) / (width * 2)) {
        fail(e, "global fixed array initializer is too large");
        return 0;
    }
    char *hex = malloc(count * width * 2 + 1);
    if (!hex) {
        fail_oom(e);
        return 0;
    }
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < count; i++) {
        uint64_t bits;
        if (!fixed_array_numeric_bits(e, element, init->as.array_init.items[i], &bits)) {
            free(hex);
            fail(e, "global fixed array initializer must contain numeric constants");
            return 0;
        }
        for (size_t j = 0; j < width; j++) {
            unsigned byte = (unsigned)((bits >> (j * 8)) & 255);
            hex[(i * width + j) * 2] = digits[byte >> 4];
            hex[(i * width + j) * 2 + 1] = digits[byte & 15];
        }
    }
    hex[count * width * 2] = '\0';
    fprintf(e->out, "blockdata %s %s\n", name, hex);
    free(hex);
    return 1;
}

/**
 * @brief Checks whether a function return type is supported by the IR backend.
 * @param t Crunch type being checked, copied, or emitted.
 * @return One on success or when the condition holds; zero on failure or when it does not.
 */
static int supported_type(const Type *t) {
    return t && (t->kind == TYPE_VOID || t->kind == TYPE_LONG_DOUBLE ||
                 isnum(t) || is_value_pointer(t));
}
/**
 * @brief Generates executable or module IR from an AST program.
 * @param p AST program being searched, analyzed, emitted, or destroyed.
 * @param out FILE stream receiving output.
 * @param error Output buffer receiving an error message on failure.
 * @param n Number of elements, bytes, or records.
 * @param module Nonzero to emit a separately compilable module.
 * @return Compiler status indicating success or the failure category.
 */
static CompilerStatus emit_ir(const AstProgram *p, FILE *out, char *error, size_t n, int module) {
    Emitter e = {0};
    e.program = p;
    e.out = out;
    e.static_out = out;
    e.error = error;
    e.error_size = n;
    e.module = module;
    if (error && n)
        error[0] = 0;
    int fixed_array_field = 0;
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (!d)
            continue;
        if (d->kind == DECL_STRUCT) {
            for (size_t j = 0; j < d->as.structure.field_count; j++) {
                Decl *field = d->as.structure.fields[j];
                if (field && field->kind == DECL_VARIABLE && field->as.variable.type &&
                    field->as.variable.type->kind == TYPE_ARRAY)
                    fixed_array_field = 1;
            }
        }
    }
    Decl *mainfn = find_function(p, "main");
    if (mainfn)
        e.location = mainfn->location;
    if ((!mainfn || !mainfn->as.function.body || mainfn->as.function.semantic_inline_only) &&
        !module) {
        fail(&e, "a defined main function is required");
        return COMPILER_CODEGEN_ERROR;
    }
    if (mainfn && mainfn->as.function.body &&
        (!mainfn->as.function.return_type || mainfn->as.function.return_type->kind != TYPE_INT ||
         mainfn->as.function.parameters.count > 3)) {
        fail(&e, "entry point must return int and accept at most argc, argv, and envp");
        return COMPILER_CODEGEN_ERROR;
    }
    if (mainfn && mainfn->as.function.body && mainfn->as.function.parameters.count) {
        Parameter *params = mainfn->as.function.parameters.items;
        if (params[0].type->kind != TYPE_INT ||
            (mainfn->as.function.parameters.count > 1 && !is_byte_string_vector(params[1].type)) ||
            (mainfn->as.function.parameters.count > 2 && !is_byte_string_vector(params[2].type))) {
            fail(&e, "entry point parameters must be int argc followed by byte argv[][] and byte "
                     "envp[][]");
            return COMPILER_CODEGEN_ERROR;
        }
    }
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (d && d->kind == DECL_FUNCTION && d->as.function.semantic_inline_only && !module) {
            e.location = d->location;
            fail(&e, "inline-only function '%s' requires module mode", d->as.function.name);
            return COMPILER_CODEGEN_ERROR;
        }
        if (!d || d->kind != DECL_FUNCTION ||
            (!d->is_static && (module || (!d->is_extern &&
              (!crunch_is_runtime_name(d->as.function.name) ||
               crunch_is_runtime_declaration(d))))))
            continue;
        Decl *definition = find_function(p, d->as.function.name);
        if (!definition || !definition->as.function.body) {
            e.location = d->location;
            if (d->is_static)
                fail(&e, "static function '%s' has no definition", d->as.function.name);
            else
                fail(&e, "external function '%s' requires module mode", d->as.function.name);
            return COMPILER_CODEGEN_ERROR;
        }
    }
    int aggregate_ir = 0, sse_aggregate_ir = 0, extended_aggregate_ir = 0;
    int runtime_guard_ir = 0;
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (module && d && d->kind == DECL_FUNCTION && d->as.function.runtime_referenced &&
            find_function(p, d->as.function.name) == d &&
            crunch_incompatible_runtime_declaration(d))
            runtime_guard_ir = 1;
        if (!d || d->kind != DECL_FUNCTION ||
            (!d->as.function.body &&
             (!module || module_runtime_declaration(d))))
            continue;
        e.location = d->location;
        const Type *result_type = d->as.function.return_type;
        if (result_type && result_type->kind == TYPE_STRUCT &&
            aggregate_parameter_size(p, result_type)) {
            aggregate_ir = extended_aggregate_ir = 1;
        } else if (!supported_type(result_type)) {
            fail(&e, "function '%s' has unsupported return type", d->as.function.name);
            return COMPILER_CODEGEN_ERROR;
        }
        for (size_t j = 0; j < d->as.function.parameters.count; j++) {
            const Type *parameter = d->as.function.parameters.items[j].type;
            unsigned classes[2];
            size_t size = parameter && parameter->kind == TYPE_STRUCT
                              ? aggregate_parameter_class(p, parameter, classes) : 0;
            if (size) {
                aggregate_ir = 1;
                if (size > 16)
                    extended_aggregate_ir = 1;
                if (classes[0] == 1 || classes[1] == 1)
                    sse_aggregate_ir = 1;
            } else if (!is_supported_parameter(parameter)) {
                e.location = d->as.function.parameters.items[j].location;
                fail(&e, "function '%s' has unsupported parameter type", d->as.function.name);
                return COMPILER_CODEGEN_ERROR;
            }
        }
        if (d->as.function.parameters.count > 128) {
            fail(&e, "function '%s' has too many parameters", d->as.function.name);
            return COMPILER_CODEGEN_ERROR;
        }
    }
    int static_storage_ir = 0;
    for (size_t i = 0; i < p->block_externals.count; ++i) {
        Decl *d = p->block_externals.items[i];
        if (d->kind != DECL_FUNCTION)
            continue;
        e.location = d->location;
        if (d->as.function.return_type && d->as.function.return_type->kind == TYPE_STRUCT &&
            aggregate_parameter_size(p, d->as.function.return_type))
            aggregate_ir = extended_aggregate_ir = 1;
        else if (!supported_type(d->as.function.return_type)) {
            fail(&e, "function '%s' has unsupported return type", d->as.function.name);
            return COMPILER_CODEGEN_ERROR;
        }
        for (size_t j = 0; j < d->as.function.parameters.count; ++j) {
            Type *type = d->as.function.parameters.items[j].type;
            unsigned classes[2];
            size_t size = type && type->kind == TYPE_STRUCT
                              ? aggregate_parameter_class(p, type, classes) : 0;
            if (size) {
                aggregate_ir = 1;
                if (size > 16)
                    extended_aggregate_ir = 1;
                if (classes[0] == 1 || classes[1] == 1)
                    sse_aggregate_ir = 1;
            } else if (!is_supported_parameter(type)) {
                e.location = d->as.function.parameters.items[j].location;
                fail(&e, "function '%s' has unsupported parameter type", d->as.function.name);
                return COMPILER_CODEGEN_ERROR;
            }
        }
        if (d->as.function.parameters.count > 128) {
            fail(&e, "function '%s' has too many parameters", d->as.function.name);
            return COMPILER_CODEGEN_ERROR;
        }
    }
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (d && d->kind == DECL_VARIABLE && d->as.variable.type &&
            (d->as.variable.type->kind == TYPE_ARRAY ||
             (d->as.variable.type->kind == TYPE_STRUCT &&
              d->as.variable.initializer.expression &&
              d->as.variable.initializer.expression->kind == EXPR_ARRAY_INIT) ||
             (d->as.variable.type->kind == TYPE_POINTER &&
              d->as.variable.initializer.expression &&
              !is_null_literal(d->as.variable.initializer.expression))))
            static_storage_ir = 1;
    }
    static_storage_ir |= p->contains_static_local || p->contains_file_compound_literal ||
                         p->contains_alignas || p->contains_vla;
    if (p->contains_thread_local && !module) {
        fail(&e, "thread-local storage requires module mode and a TLS-capable C runtime");
        return COMPILER_CODEGEN_ERROR;
    }
    e.typed_ir = p->contains_wide_string || p->contains_pointer_to_integer || p->contains_extended_fixed_precision || p->contains_dynamic_string_precision || p->contains_pointer_difference || p->contains_long_double || p->contains_formatted_output || p->contains_c_format_integer || p->contains_function_types || p->contains_variadic || aggregate_ir || p->c11_numeric || static_storage_ir || fixed_array_field || p->contains_unsigned || p->contains_bit_field || p->contains_float_ir ||
                 p->contains_short || p->contains_character_types || p->contains_thread_local;
    fprintf(out, "CRUNCH-IR %d\n",
            p->contains_wide_string ? (module ? 61 : 60)
            : p->contains_alloca ? (module ? 59 : 58)
            : p->contains_pointer_to_integer ? (module ? 57 : 56)
            : p->contains_pack ? (module ? 55 : 54)
            : p->contains_thread_local ? 53
            : p->contains_vla ? (module ? 51 : 50)
            : p->contains_alignas ? (module ? 49 : 48)
            : p->contains_flexible_array ? (module ? 47 : 46)
            : p->contains_extended_fixed_precision ? (module ? 43 : 42)
            : (module && p->contains_variadic) ? 41
            : (module && (p->contains_runtime_dispatch || runtime_guard_ir)) ? 39
            : (p->contains_pointer_difference || p->contains_dynamic_string_precision) ? (module ? 37 : 36)
            : p->contains_long_double ? (module ? 35 : 34)
            : p->contains_va_aggregate ? (module ? 33 : 32)
            : p->contains_va_start ? (module ? 31 : 30)
            : p->contains_formatted_output ? (module ? 29 : 28)
            : p->contains_c_format_integer ? (module ? 27 : 26)
            : (p->contains_function_types || p->contains_variadic ||
               (module && p->contains_runtime_dispatch)) ? (module ? 25 : 24)
            : extended_aggregate_ir ? (module ? 23 : 22)
            : sse_aggregate_ir ? (module ? 21 : 20)
            : aggregate_ir ? (module ? 19 : 18)
            : p->contains_character_types ? (module ? 17 : 16)
            : p->contains_short ? (module ? 15 : 14)
            : (static_storage_ir || fixed_array_field) ? (module ? 13 : 12)
            : p->contains_float_ir ? (module ? 9 : 8)
            : (p->contains_unsigned || p->contains_bit_field) ? (module ? 7 : 6)
            : (module ? 3 : 2));
    for (size_t i = 0; i < p->declarations.count; ++i) {
        const Decl *d = p->declarations.items[i];
        if (d && d->kind == DECL_VARIABLE &&
            !emit_file_compound_literals(&e, d->as.variable.initializer.expression, 0))
            return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
    }
    if (module) {
        for (size_t i = 0; i < p->declarations.count; i++) {
            Decl *d = p->declarations.items[i];
            if (!d || d->kind != DECL_FUNCTION ||
                (d->as.function.body && !d->as.function.semantic_inline_only))
                continue;
            Decl *definition = find_function(p, d->as.function.name);
            int dispatch = crunch_is_runtime_declaration(definition) &&
                           !crunch_is_linkable_runtime_declaration(definition) &&
                           definition->as.function.runtime_referenced;
            if (!definition || definition != d ||
                (definition->as.function.body && !definition->as.function.semantic_inline_only) ||
                (module_runtime_declaration(definition) && !dispatch))
                continue;
            int internal = 0;
            for (size_t j = 0; j < p->declarations.count; j++) {
                Decl *prior = p->declarations.items[j];
                if (prior && prior->kind == DECL_FUNCTION &&
                    !strcmp(prior->as.function.name, d->as.function.name)) {
                    if (prior->is_static)
                        internal = 1;
                }
            }
            if (internal)
                continue;
            if (definition->as.function.runtime_referenced &&
                crunch_incompatible_runtime_declaration(definition))
                fprintf(out, "require_user_runtime %s\n", d->as.function.name);
            if (p->contains_variadic && d->as.function.variadic)
                fprintf(out, "abi_variadic %s\n", d->as.function.name);
            fprintf(out, "extern_func %s %zu ", d->as.function.name,
                    d->as.function.parameters.count);
            emit_parameter_type(out, p, d->as.function.return_type);
            for (size_t j = 0; j < d->as.function.parameters.count; j++) {
                fputc(' ', out);
                emit_parameter_type(out, p, d->as.function.parameters.items[j].type);
            }
            fputc('\n', out);
        }
    }
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (!d)
            continue;
        if (d->kind == DECL_VARIABLE) {
            if (d->semantic_global_definition && d->semantic_internal_linkage)
                fprintf(out, "internal %s\n", d->as.variable.name);
            if (d->semantic_global_definition && d->is_thread_local)
                fprintf(out, "thread_local %s\n", d->as.variable.name);
        } else if (d->kind == DECL_FUNCTION && d->is_static) {
            bool emitted = false;
            for (size_t j = 0; j < i; j++) {
                Decl *prior = p->declarations.items[j];
                if (prior && prior->kind == DECL_FUNCTION && prior->is_static &&
                    !strcmp(prior->as.function.name, d->as.function.name))
                    emitted = true;
            }
            Decl *definition = find_function(p, d->as.function.name);
            if (!emitted && definition && definition->as.function.body)
                fprintf(out, "internal %s\n", d->as.function.name);
        }
    }
    for (size_t i = 0; i < p->block_externals.count; ++i) {
        Decl *d = p->block_externals.items[i];
        const char *name = d->kind == DECL_FUNCTION ? d->as.function.name : d->as.variable.name;
        int seen = 0;
        for (size_t j = 0; j < p->declarations.count; ++j) {
            Decl *other = p->declarations.items[j];
            if (other && ((other->kind == DECL_FUNCTION &&
                           !strcmp(name, other->as.function.name)) ||
                          (other->kind == DECL_VARIABLE &&
                           !strcmp(name, other->as.variable.name))))
                seen = 1;
        }
        for (size_t j = 0; j < i; ++j) {
            Decl *other = p->block_externals.items[j];
            const char *other_name = other->kind == DECL_FUNCTION ? other->as.function.name
                                                                   : other->as.variable.name;
            if (!strcmp(name, other_name))
                seen = 1;
        }
        if (seen)
            continue;
        if (d->kind == DECL_FUNCTION) {
            if (!module) {
                e.location = d->location;
                fail(&e, "external function '%s' requires module mode", name);
                return COMPILER_CODEGEN_ERROR;
            }
            if (d->as.function.variadic)
                fprintf(out, "abi_variadic %s\n", name);
            fprintf(out, "extern_func %s %zu ", name, d->as.function.parameters.count);
            emit_parameter_type(out, p, d->as.function.return_type);
            for (size_t j = 0; j < d->as.function.parameters.count; ++j) {
                fputc(' ', out);
                emit_parameter_type(out, p, d->as.function.parameters.items[j].type);
            }
            fputc('\n', out);
        } else {
            Type *type = d->as.variable.type;
            fprintf(out, "extern_global %s %s\n", name,
                    type && (type->kind == TYPE_ARRAY || type->kind == TYPE_STRUCT ||
                             type->kind == TYPE_LONG_DOUBLE) ? "byte" : tn(type));
        }
    }
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *g = p->declarations.items[i];
        if (!g || g->kind != DECL_VARIABLE)
            continue;
        e.location = g->location;
        const Type *gt = g->as.variable.type;
        if (!g->semantic_global_definition) {
            Decl *definition = find_global(p, g->as.variable.name);
            if (definition && definition->semantic_global_definition)
                continue;
            if (gt && (gt->kind == TYPE_ARRAY || gt->kind == TYPE_LONG_DOUBLE ||
                       gt->kind == TYPE_STRUCT)) {
                fprintf(out, "extern_global %s byte\n", g->as.variable.name);
            } else if (!isnum(gt) && !is_value_pointer(gt)) {
                fail(&e, "external global '%s' has unsupported type", g->as.variable.name);
                return COMPILER_CODEGEN_ERROR;
            } else
                fprintf(out, "extern_global %s %s\n", g->as.variable.name, tn(gt));
            continue;
        }
        if (gt && gt->kind == TYPE_LONG_DOUBLE) {
            if ((g->as.variable.initializer.kind != INIT_NONE &&
                 g->as.variable.initializer.kind != INIT_EXPRESSION) ||
                !emit_extended_static(&e, g->as.variable.name,
                                      g->as.variable.initializer.expression)) {
                fail(&e, "unsupported long double global initializer");
                return COMPILER_CODEGEN_ERROR;
            }
            continue;
        }
        if (gt && gt->kind == TYPE_ARRAY) {
            if (!emit_fixed_global_array(&e, g, g->as.variable.name))
                return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
            continue;
        }
        if (gt && gt->kind == TYPE_STRUCT) {
            Decl *sd = find_struct(p, gt->struct_name);
            if (!sd) {
                fail(&e, "unknown global structure type");
                return COMPILER_CODEGEN_ERROR;
            }
            size_t offsets[256], sz = 0, al = 0;
            if (sd->as.structure.field_count > 256 || !struct_layout(p, sd, offsets, &sz, &al)) {
                fail(&e, "unsupported field type or oversized structure");
                return COMPILER_CODEGEN_ERROR;
            }
            if (g->semantic_alignment > al)
                al = g->semantic_alignment;
            if (g->as.variable.initializer.kind != INIT_NONE && (!sz || sz > UINT32_MAX)) {
                fail(&e, "unsupported or oversized initialized structure");
                return COMPILER_CODEGEN_ERROR;
            }
            fprintf(out, "globalblock %s %zu %zu\n", g->as.variable.name, sz, al);
            if (g->as.variable.initializer.kind != INIT_NONE) {
                if (!g->as.variable.initializer.expression ||
                    g->as.variable.initializer.expression->kind != EXPR_ARRAY_INIT) {
                    fail(&e, "global structure initializer requires a constant list");
                    return COMPILER_CODEGEN_ERROR;
                }
                if (!emit_global_struct_initializer(&e, gt, g->as.variable.initializer.expression,
                                                    g->as.variable.initializer.semantic_plan,
                                                    g->as.variable.name, sz))
                    return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
            }
            if (!emit_global_aliases(&e, sd, g->as.variable.name, g->as.variable.name, 0, 0)) {
                fail(&e, "cannot emit aliases for nested structure fields");
                return COMPILER_CODEGEN_ERROR;
            }
            continue;
        }
        if (!isnum(gt) && !is_value_pointer(gt)) {
            fail(&e, "global '%s' has unsupported type in this IR milestone", g->as.variable.name);
            return COMPILER_CODEGEN_ERROR;
        }
        if (g->as.variable.initializer.expression &&
            g->as.variable.initializer.expression->kind == EXPR_ARRAY_INIT) {
            fail(&e, "array literal initializers are not yet supported");
            return COMPILER_CODEGEN_ERROR;
        }
        const Expr *init = g->as.variable.initializer.expression;
        if (gt->kind == TYPE_POINTER)
            init = static_pointer_value(init);
        if ((static_storage_ir || fixed_array_field) && gt->kind == TYPE_POINTER && init &&
            !is_null_literal(init)) {
            char symbol[128];
            int64_t addend;
            if (!fixed_address_constant(&e, init, symbol, sizeof symbol, &addend, 0)) {
                fail(&e, "global pointer initializer must be an address constant");
                return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
            }
            fprintf(out, "global %s ptr 0\nglobalreloc %s %s %" PRId64 "\n",
                    g->as.variable.name, g->as.variable.name, symbol, addend);
            continue;
        }
        char value[128] = "0";
        if (gt->kind == TYPE_FLOAT)
            snprintf(value, sizeof value, "0x00000000");
        if (init) {
            int64_t constant;
            if (promoted_small_kind(gt->kind) ||
                (p->c11_numeric && isnum(gt) && gt->kind != TYPE_FLOAT && gt->kind != TYPE_REAL)) {
                uint64_t bits;
                if (!(p->c11_numeric
                          ? integer_initializer_bits(&e, init, type_effective_kind(gt), &bits, 0)
                          : sema_constant_integer_conversion(init, type_effective_kind(gt), &bits))) {
                    fail(&e, "global initializer must be a numeric constant in this IR milestone");
                    return COMPILER_CODEGEN_ERROR;
                }
                if (type_effective_kind(gt) == TYPE_UNSIGNED_INT ||
                    gt->kind == TYPE_BYTE || gt->kind == TYPE_UNSIGNED_SHORT ||
                    gt->kind == TYPE_UNSIGNED_CHAR || gt->kind == TYPE_UNSIGNED_INT ||
                    gt->kind == TYPE_UNSIGNED_LONG || gt->kind == TYPE_UNSIGNED_LONG_LONG)
                    snprintf(value, sizeof value, "%" PRIu64, bits);
                else
                    snprintf(value, sizeof value, "%" PRId64, signed_constant_bits(bits));
            } else if (gt->kind == TYPE_POINTER && is_null_literal(init))
                snprintf(value, sizeof value, "0");
            else if (init->semantic_float && isnum(gt) &&
                     gt->kind != TYPE_FLOAT && gt->kind != TYPE_REAL) {
                uint64_t bits;
                if (!sema_constant_integer_conversion(init, gt->kind, &bits)) {
                    fail(&e, "global initializer must be a numeric constant in this IR milestone");
                    return COMPILER_CODEGEN_ERROR;
                }
                if (gt->kind == TYPE_UNSIGNED_INT || gt->kind == TYPE_UNSIGNED_LONG ||
                    gt->kind == TYPE_UNSIGNED_LONG_LONG ||
                    gt->kind == TYPE_BYTE)
                    snprintf(value, sizeof value, "%" PRIu64, bits);
                else
                    snprintf(value, sizeof value, "%" PRId64, signed_constant_bits(bits));
            }
            else if (gt->kind == TYPE_FLOAT || (init->semantic_float && isnum(gt))) {
                long double result;
                uint64_t integer_bits;
                TypeKind integer_kind;
                const Type *initializer_type = expr_type(&e, init);
                if (initializer_type && initializer_type->kind != TYPE_FLOAT &&
                    initializer_type->kind != TYPE_REAL &&
                    sema_integer_constant(init, &integer_bits, &integer_kind)) {
                    result = integer_kind == TYPE_UNSIGNED_CHAR || integer_kind == TYPE_UNSIGNED_SHORT || integer_kind == TYPE_UNSIGNED_INT ||
                             integer_kind == TYPE_UNSIGNED_LONG || integer_kind == TYPE_UNSIGNED_LONG_LONG ||
                             integer_kind == TYPE_BYTE
                                 ? (long double)integer_bits
                                 : (long double)signed_constant_bits(integer_bits);
                } else if (!sema_numeric_constant(init, &result)) {
                    fail(&e, "global initializer must be a numeric constant in this IR milestone");
                    return COMPILER_CODEGEN_ERROR;
                }
                if (gt->kind == TYPE_FLOAT) {
                    uint32_t bits = sema_binary32_bits(result);
                    snprintf(value, sizeof value, "0x%08" PRIx32, bits);
                } else if (gt->kind == TYPE_REAL) {
                    double real = (double)result;
                    uint64_t bits;
                    memcpy(&bits, &real, sizeof bits);
                    snprintf(value, sizeof value, "0x%016" PRIx64, bits);
                } else if (gt->kind == TYPE_UNSIGNED_INT || gt->kind == TYPE_UNSIGNED_LONG ||
                           gt->kind == TYPE_UNSIGNED_LONG_LONG)
                    snprintf(value, sizeof value, "%" PRIu64, (uint64_t)result);
                else
                    snprintf(value, sizeof value, "%" PRId64, (int64_t)result);
            }
            else if (gt->kind == TYPE_REAL) {
                double result;
                if (!constant_real_value(&e, init, &result, 0)) {
                    fail(&e, "global initializer must be a numeric constant in this IR milestone");
                    return COMPILER_CODEGEN_ERROR;
                }
                uint64_t bits;
                memcpy(&bits, &result, sizeof bits);
                snprintf(value, sizeof value, "0x%016" PRIx64, bits);
            } else if (gt->kind != TYPE_POINTER &&
                       (init->semantic_unsigned || gt->kind == TYPE_UNSIGNED_INT ||
                        gt->kind == TYPE_UNSIGNED_LONG || gt->kind == TYPE_UNSIGNED_LONG_LONG)) {
                uint64_t bits;
                TypeKind kind;
                if (!sema_integer_constant(init, &bits, &kind) &&
                    !sema_constant_integer_conversion(init, gt->kind, &bits)) {
                    fail(&e, "global initializer must be a numeric constant in this IR milestone");
                    return COMPILER_CODEGEN_ERROR;
                }
                if (gt->kind == TYPE_BYTE)
                    bits &= UINT64_C(0xff);
                else if (gt->kind == TYPE_UNSIGNED_INT || gt->kind == TYPE_INT ||
                         gt->kind == TYPE_ENUM) {
                    bits &= UINT64_C(0xffffffff);
                    if ((gt->kind == TYPE_INT || gt->kind == TYPE_ENUM) && bits > INT32_MAX)
                        bits |= UINT64_C(0xffffffff00000000);
                }
                if (gt->kind == TYPE_UNSIGNED_INT || gt->kind == TYPE_UNSIGNED_LONG ||
                    gt->kind == TYPE_UNSIGNED_LONG_LONG)
                    snprintf(value, sizeof value, "%" PRIu64, bits);
                else
                    snprintf(value, sizeof value, "%" PRId64, signed_constant_bits(bits));
            } else if (gt->kind != TYPE_REAL && gt->kind != TYPE_POINTER &&
                       constant_integer_initializer(&e, init, &constant, 0))
                snprintf(value, sizeof value, "%" PRId64, constant);
            else if (gt->kind != TYPE_REAL && gt->kind != TYPE_POINTER) {
                uint64_t bits;
                if (!sema_constant_integer_conversion(init, gt->kind, &bits)) {
                    fail(&e, "global initializer must be a numeric constant in this IR milestone");
                    return COMPILER_CODEGEN_ERROR;
                }
                if (gt->kind == TYPE_UNSIGNED_INT || gt->kind == TYPE_UNSIGNED_LONG ||
                    gt->kind == TYPE_UNSIGNED_LONG_LONG ||
                    gt->kind == TYPE_BYTE)
                    snprintf(value, sizeof value, "%" PRIu64, bits);
                else
                    snprintf(value, sizeof value, "%" PRId64, signed_constant_bits(bits));
            }
            else if (init->kind == EXPR_INTEGER && !init->integer_literal_out_of_range)
                snprintf(value, sizeof value, "%" PRId64, init->as.integer_value);
            else if (init->kind == EXPR_CHAR)
                snprintf(value, sizeof value, "%d", (int)init->as.char_value);
            else if (init->kind == EXPR_REAL) {
                uint64_t bits;
                memcpy(&bits, &init->as.real_value, sizeof bits);
                snprintf(value, sizeof value, "0x%016" PRIx64, bits);
            } else {
                fail(&e, "global initializer must be a numeric constant in this IR milestone");
                return COMPILER_CODEGEN_ERROR;
            }
        }
        fprintf(out, "global %s %s %s\n", g->as.variable.name, tn(gt), value);
    }
    for (size_t i = 0; i < p->declarations.count; i++) {
        Decl *d = p->declarations.items[i];
        if (!d || d->kind != DECL_FUNCTION || !d->as.function.body ||
            d->as.function.semantic_inline_only)
            continue;
        e.location = d->location;
        e.count = 0;
        e.pointer_count = 0;
        e.next_slot = 0;
        memset(e.vla_parameter_sizes, 0, sizeof e.vla_parameter_sizes);
        e.next_extended = 0;
        e.fixed_array_bytes = 0;
        e.fixed_array_copy_slot = 0;
        e.volatile_read_slot = 0;
        e.next_field_alias = 0;
        memset(e.numeric_postfix_slots, 0, sizeof e.numeric_postfix_slots);
        e.break_depth = 0;
        e.continue_depth = 0;
        e.function_has_vla = d->as.function.semantic_has_vla;
        e.function_has_alloca = d->as.function.semantic_has_alloca;
        e.active_vla_count = 0;
        e.vla_definition_count = 0;
        memset(e.vla_label_bits, 0, sizeof e.vla_label_bits);
        if (e.function_has_vla) {
            unsigned active[512];
            size_t active_count = 0;
            if (d->as.function.label_count > 512 ||
                !collect_vla_labels(&e, d->as.function.body, active, &active_count, 0)) {
                if (!e.error || !e.error[0])
                    fail(&e, "too many labels in a function with variable-length arrays");
                return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
            }
        }
        e.return_type = d->as.function.return_type;
        e.function_name = d->as.function.name;
        e.function_name_type = (Type){0};
        e.function_name_type.kind = TYPE_ARRAY;
        e.function_name_type.pointee = (Type *)&TYPE_BYTE_VALUE;
        e.function_name_type.array_length = d->as.function.name_length + 1;
        e.function_name_string = 0;
        if (d->as.function.label_count > UINT_MAX - e.next_label) {
            fail(&e, "too many goto labels");
            return COMPILER_CODEGEN_ERROR;
        }
        e.goto_label_base = e.next_label;
        e.goto_label_count = d->as.function.label_count;
        e.next_label += (unsigned)e.goto_label_count;
        FILE *body = p->contains_static_local ? tmpfile() : out;
        if (!body) {
            fail(&e, "cannot create temporary stream for function IR");
            return COMPILER_IO_ERROR;
        }
        e.out = body;
        if (module && p->contains_variadic && d->as.function.variadic)
            fprintf(body, "abi_variadic %s\n", d->as.function.name);
        fprintf(body, "func %s %zu ", d->as.function.name, d->as.function.parameters.count);
        emit_parameter_type(body, p, d->as.function.return_type);
        fputc('\n', body);
        unsigned parameter_slots[128];
        for (size_t j = 0; j < d->as.function.parameters.count; j++) {
            Parameter *pa = &d->as.function.parameters.items[j];
            unsigned classes[2];
            size_t size = aggregate_parameter_class(p, pa->type, classes);
            unsigned words = size > 8 && size <= 16 && classes[0] != 3 && classes[0] != 4 &&
                             classes[0] != 5 &&
                             !aggregate_one_aligned_word(p, pa->type, size, classes) ? 2 : 1;
            if (e.count >= CRUNCH_LOCAL_LIMIT || e.next_slot > CRUNCH_LOCAL_LIMIT - words) {
                e.location = pa->location;
                fail(&e, "too many locals");
                if (body != out)
                    fclose(body);
                return COMPILER_CODEGEN_ERROR;
            }
            unsigned slot = e.next_slot;
            e.next_slot += words;
            parameter_slots[j] = slot;
            e.locals[e.count++] = (Local){pa->name, slot, pa->type,
                                          pa->type && pa->type->kind == TYPE_LONG_DOUBLE
                                              ? LOCAL_EXTENDED_PARAM : 0};
            fprintf(body, "param %zu %u ", j, slot);
            emit_parameter_type(body, p, pa->type);
            fputc('\n', body);
        }
        if (p->contains_va_start && d->as.function.variadic)
            fputs("variadic\n", body);
        for (size_t j = 0; j < d->as.function.parameters.count; j++) {
            Parameter *pa = &d->as.function.parameters.items[j];
            unsigned classes[2];
            size_t size = aggregate_parameter_class(p, pa->type, classes);
            if (!size)
                continue;
            unsigned slot = parameter_slots[j];
            unsigned words = size > 8 && size <= 16 && classes[0] != 3 && classes[0] != 4 &&
                             classes[0] != 5 &&
                             !aggregate_one_aligned_word(p, pa->type, size, classes) ? 2 : 1;
            const Decl *sd = find_struct(p, pa->type->struct_name);
            size_t offsets[256], layout_size, alignment;
            char block[64];
            snprintf(block, sizeof block, "__local_struct_%u", slot);
            if (!sd || sd->as.structure.field_count > 256 ||
                !struct_layout(p, sd, offsets, &layout_size, &alignment)) {
                fail(&e, "unsupported structure parameter layout");
                release_local_names(&e, 0);
                if (body != out)
                    fclose(body);
                return COMPILER_CODEGEN_ERROR;
            }
            fprintf(body, "localblock %s %zu %zu\n"
                          "localalias __local_struct_address_%u %s 0 byte\n",
                    block, size, alignment, slot, block);
            if (!register_local_fields(&e, pa->name, block, sd, 0, 0, 0)) {
                fail(&e, "unsupported structure parameter fields");
                release_local_names(&e, 0);
                if (body != out)
                    fclose(body);
                return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
            }
            if (size > 16 || classes[0] == 3 || classes[0] == 4 || classes[0] == 5) {
                fprintf(body, "laddr __local_struct_address_%u\nload %u\n", slot, slot);
                if (!emit_fixed_array_copy(&e, pa->type)) {
                    release_local_names(&e, 0);
                    if (body != out)
                        fclose(body);
                    return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
                }
            } else {
                size_t copy_size = aggregate_one_aligned_word(p, pa->type, size, classes) ? 8 : size;
                for (size_t k = 0; k < copy_size; k++)
                    fprintf(body, "laddr __local_struct_address_%u\npush %zu\nptradd byte\n"
                                  "addr %u\npush %zu\nptradd byte\nloadind byte\nstoreind byte\ndrop\n",
                            slot, k, slot + words - 1, k);
            }
        }
        for (size_t j = 0; j < d->as.function.parameters.count; ++j) {
            Parameter *pa = &d->as.function.parameters.items[j];
            Type *type = pa->type;
            if (!type || type->kind != TYPE_POINTER)
                continue;
            if (type->array_bound && type->array_bound->variable) {
                if (!expression(&e, type->array_bound->expression)) {
                    release_local_names(&e, 0);
                    if (body != out)
                        fclose(body);
                    return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
                }
                fputs("drop\n", body);
            }
            int variable = 0;
            for (const Type *part = type->pointee; part && part->kind == TYPE_ARRAY;
                 part = part->pointee)
                if (part->array_bound && part->array_bound->variable)
                    variable = 1;
            if (variable) {
                e.vla_parameter_sizes[parameter_slots[j]] = e.next_slot + 1;
                if (!emit_vla_pointer_sizes(&e, type, e.next_slot)) {
                    release_local_names(&e, 0);
                    if (body != out)
                        fclose(body);
                    return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
                }
            }
        }
        if (!statement(&e, d->as.function.body)) {
            release_local_names(&e, 0);
            if (body != out)
                fclose(body);
            return e.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_CODEGEN_ERROR;
        }
        int returns = always_returns(p, d->as.function.body, 0, module);
        if (returns < 0) {
            e.recursion_limit_exceeded = 1;
            fail(&e, "maximum IR return-analysis nesting depth exceeded");
            if (body != out)
                fclose(body);
            return COMPILER_CODEGEN_ERROR;
        }
        if (!returns || module || e.goto_label_count) {
            if (d->as.function.return_type->kind == TYPE_LONG_DOUBLE) {
                if (!emit_extended_temporary(&e)) {
                    release_local_names(&e, 0);
                    if (body != out)
                        fclose(body);
                    return COMPILER_CODEGEN_ERROR;
                }
                fputs("push 0\nextfrom int\nretagg ext\n", body);
            } else if (d->as.function.return_type->kind == TYPE_STRUCT) {
                if (!emit_aggregate_result_buffer(&e, aggregate_parameter_size(p, e.return_type))) {
                    release_local_names(&e, 0);
                    if (body != out)
                        fclose(body);
                    return COMPILER_CODEGEN_ERROR;
                }
                fputs("retagg ", body);
                emit_parameter_type(body, p, e.return_type);
                fputc('\n', body);
            } else
                fprintf(body, "%s\nret %s\n",
                    d->as.function.return_type->kind == TYPE_FLOAT
                        ? "pushf 0x00000000" : "push 0",
                    tn(d->as.function.return_type));
        }
        fprintf(body, "end\n");
        if (body != out) {
            int failed = fflush(body) != 0 || fseek(body, 0, SEEK_SET) != 0;
            char buffer[8192];
            size_t count;
            while (!failed && (count = fread(buffer, 1, sizeof buffer, body)) != 0)
                if (fwrite(buffer, 1, count, out) != count)
                    failed = 1;
            if (ferror(body))
                failed = 1;
            if (fclose(body) != 0)
                failed = 1;
            e.out = out;
            if (failed) {
                fail(&e, "cannot write function IR");
                return COMPILER_IO_ERROR;
            }
        }
    }
    return e.recursion_limit_exceeded ? COMPILER_CODEGEN_ERROR
                                      : (ferror(out) ? COMPILER_IO_ERROR : COMPILER_OK);
}

/**
 * @brief Generates executable-oriented IR from an AST program.
 * @param program AST program to emit.
 * @param output Output stream receiving IR.
 * @param error Optional diagnostic message buffer.
 * @param error_size Capacity of error in bytes.
 * @return Compiler status indicating success or the failure category.
 */
CompilerStatus crunch_ir_emit(const AstProgram *program, FILE *output, char *error,
                              size_t error_size) {
    return emit_ir(program, output, error, error_size, 0);
}

/**
 * @brief Generates IR and preserves structured diagnostics on failure.
 * @param program AST program being searched, analyzed, emitted, or destroyed.
 * @param output FILE stream receiving generated output.
 * @param diagnostic Diagnostic record to read or populate.
 * @param message Output diagnostic-message buffer.
 * @param message_size Capacity of the diagnostic-message buffer.
 * @param module Nonzero to emit a separately compilable module.
 * @return Compiler status indicating success or the failure category.
 */
static CompilerStatus emit_with_diagnostic(const AstProgram *program, FILE *output,
                                           Diagnostic *diagnostic, char *message,
                                           size_t message_size, int module) {
    Diagnostic *previous = active_ir_diagnostic;
    CompilerStatus status;

    if (diagnostic) {
        *diagnostic = (Diagnostic){0};
        diagnostic->severity = DIAGNOSTIC_ERROR;
        diagnostic->code = "E_IR_GENERATION";
    }
    if (message && message_size)
        message[0] = '\0';
    if (!diagnostic || !message || message_size == 0)
        return COMPILER_INVALID_INPUT;

    active_ir_diagnostic = diagnostic;
    status = emit_ir(program, output, message, message_size, module);
    active_ir_diagnostic = previous;

    if (status == COMPILER_IO_ERROR && !diagnostic->message) {
        diagnostic->code = "E_IO";
        diagnostic->message = "failed to write IR";
    } else if (status == COMPILER_OUT_OF_MEMORY && !diagnostic->message) {
        diagnostic->code = "E_OUT_OF_MEMORY";
        diagnostic->message = "out of memory generating IR";
    } else if (status != COMPILER_OK && !diagnostic->message) {
        diagnostic->message = "IR generation failed";
    }
    return status;
}

/**
 * @brief Generates executable-oriented IR with structured diagnostics.
 * @param program AST program to emit.
 * @param output Output stream receiving IR.
 * @param diagnostic Diagnostic record receiving error context.
 * @param message Diagnostic message buffer.
 * @param message_size Capacity of message in bytes.
 * @return Compiler status indicating success or the failure category.
 */
CompilerStatus crunch_ir_emit_with_diagnostic(const AstProgram *program, FILE *output,
                                              Diagnostic *diagnostic, char *message,
                                              size_t message_size) {
    return emit_with_diagnostic(program, output, diagnostic, message, message_size, 0);
}

/**
 * @brief Generates separately compilable module IR with structured diagnostics.
 * @param program AST program to emit.
 * @param output Output stream receiving IR.
 * @param diagnostic Diagnostic record receiving error context.
 * @param message Diagnostic message buffer.
 * @param message_size Capacity of message in bytes.
 * @return Compiler status indicating success or the failure category.
 */
CompilerStatus crunch_ir_emit_module_with_diagnostic(const AstProgram *program, FILE *output,
                                                     Diagnostic *diagnostic, char *message,
                                                     size_t message_size) {
    return emit_with_diagnostic(program, output, diagnostic, message, message_size, 1);
}
