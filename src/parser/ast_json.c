#include "parser/ast_json.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

enum { AST_JSON_MAX_RECURSION_DEPTH = 256 };

/**
 * @brief Writes a JSON-escaped string.
 * @param o Output FILE stream receiving JSON.
 * @param s NUL-terminated string to encode as a JSON string.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int str(FILE *o, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    if (fputc('"', o) == EOF)
        return 0;
    for (; *p; ++p) {
        switch (*p) {
        case '"':
            if (fputs("\\\"", o) == EOF)
                return 0;
            break;
        case '\\':
            if (fputs("\\\\", o) == EOF)
                return 0;
            break;
        case '\b':
            if (fputs("\\b", o) == EOF)
                return 0;
            break;
        case '\f':
            if (fputs("\\f", o) == EOF)
                return 0;
            break;
        case '\n':
            if (fputs("\\n", o) == EOF)
                return 0;
            break;
        case '\r':
            if (fputs("\\r", o) == EOF)
                return 0;
            break;
        case '\t':
            if (fputs("\\t", o) == EOF)
                return 0;
            break;
        default:
            if (*p < 0x20) {
                if (fprintf(o, "\\u%04x", (unsigned)*p) < 0)
                    return 0;
            } else if (fputc(*p, o) == EOF)
                return 0;
        }
    }
    return fputc('"', o) != EOF;
}
/**
 * @brief Writes a byte sequence as a JSON string.
 * @param o Output FILE stream receiving JSON.
 * @param p Input parser, node, or data pointer.
 * @param n Number of bytes to encode.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int bytes(FILE *o, const unsigned char *p, size_t n)
{
    size_t i;
    if (fputc('"', o) == EOF)
        return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = p[i];
        switch (c) {
        case '"':
            if (fputs("\\\"", o) == EOF)
                return 0;
            break;
        case '\\':
            if (fputs("\\\\", o) == EOF)
                return 0;
            break;
        case '\n':
            if (fputs("\\n", o) == EOF)
                return 0;
            break;
        case '\r':
            if (fputs("\\r", o) == EOF)
                return 0;
            break;
        case '\t':
            if (fputs("\\t", o) == EOF)
                return 0;
            break;
        default:
            if (c < 0x20) {
                if (fprintf(o, "\\u%04x", (unsigned)c) < 0)
                    return 0;
            } else if (fputc(c, o) == EOF)
                return 0;
        }
    }
    return fputc('"', o) != EOF;
}
/**
 * @brief Serializes a source location.
 * @param o Output FILE stream receiving JSON.
 * @param l Source location associated with the token or diagnostic.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int loc(FILE *o, SourceLocation l)
{
    return fprintf(o, "{\"offset\":%zu,\"line\":%u,\"column\":%u}", l.offset, l.line, l.column) >=
           0;
}
/**
 * @brief Returns the printable name of an AST type kind.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return Static type name, or "unknown" for an unrecognized kind.
 */
static const char *type_name(TypeKind k)
{
    switch (k) {
    case TYPE_VOID:
        return "void";
    case TYPE_BOOL:
        return "bool";
    case TYPE_ENUM:
        return "enum";
    case TYPE_INT:
        return "int";
    case TYPE_SHORT:
        return "short";
    case TYPE_UNSIGNED_SHORT:
        return "unsigned_short";
    case TYPE_LONG:
        return "long";
    case TYPE_LONG_LONG:
        return "long_long";
    case TYPE_UNSIGNED_INT:
        return "unsigned_int";
    case TYPE_UNSIGNED_LONG:
        return "unsigned_long";
    case TYPE_UNSIGNED_LONG_LONG:
        return "unsigned_long_long";
    case TYPE_BYTE:
        return "byte";
    case TYPE_SIGNED_CHAR:
        return "signed_char";
    case TYPE_UNSIGNED_CHAR:
        return "unsigned_char";
    case TYPE_REAL:
        return "real";
    case TYPE_LONG_DOUBLE:
        return "long_double";
    case TYPE_FLOAT:
        return "float";
    case TYPE_POINTER:
        return "pointer";
    case TYPE_ARRAY:
        return "array";
    case TYPE_STRUCT:
        return "struct";
    case TYPE_FUNCTION:
        return "function";
    }
    return "unknown";
}
/**
 * @brief Returns the printable name of a unary operator.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return Static operator name, or "unknown" for an out-of-range kind.
 */
static const char *unary_name(UnaryOp k)
{
    static const char *n[] = {"plus",           "minus",         "logical_not",   "bitwise_not",
                              "address",        "dereference",   "pre_increment", "pre_decrement",
                              "post_increment", "post_decrement"};
    return (unsigned)k < sizeof(n) / sizeof(n[0]) ? n[k] : "unknown";
}
/**
 * @brief Returns the printable name of a binary operator.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return Static operator name, or "unknown" for an out-of-range kind.
 */
static const char *binary_name(BinaryOp k)
{
    static const char *n[] = {
        "add",        "subtract",    "multiply",   "divide",      "remainder",     "power", "equal",
        "not_equal",  "less",        "less_equal", "greater",     "greater_equal", "logical_and",
        "logical_or", "bitwise_and", "bitwise_or", "bitwise_xor", "shift_left",    "shift_right",
        "comma"};
    return (unsigned)k < sizeof(n) / sizeof(n[0]) ? n[k] : "unknown";
}
/**
 * @brief Returns the printable name of an assignment operator.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return Static operator name, or "unknown" for an out-of-range kind.
 */
static const char *assign_name(AssignOp k)
{
    static const char *n[] = {"assign",
                              "add_assign",
                              "subtract_assign",
                              "multiply_assign",
                              "divide_assign",
                              "remainder_assign",
                              "bitwise_and_assign",
                              "bitwise_or_assign",
                              "bitwise_xor_assign",
                              "shift_left_assign",
                              "shift_right_assign"};
    return (unsigned)k < sizeof(n) / sizeof(n[0]) ? n[k] : "unknown";
}
static int type(FILE *, unsigned, const Type *);
static int expr(FILE *, unsigned, const Expr *);
static int stmt(FILE *, unsigned, const Stmt *);
static int decl(FILE *, unsigned, const Decl *);
/**
 * @brief Serializes a type node to JSON.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param t Crunch type node being inspected or emitted.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int type_impl(FILE *o, unsigned depth, const Type *t)
{
    if (!t)
        return fputs("null", o) != EOF;
    if (fputs("{\"kind\":", o) == EOF ||
        !str(o, t->kind == TYPE_STRUCT && t->is_union ? "union" : type_name(t->kind)))
        return 0;
    if (fprintf(o, ",\"const\":%s", t->is_const ? "true" : "false") < 0)
        return 0;
    if (t->is_volatile && fputs(",\"volatile\":true", o) == EOF)
        return 0;
    if (t->kind == TYPE_POINTER) {
        if (fputs(",\"pointee\":", o) == EOF || !type(o, depth, t->pointee))
            return 0;
    }
    if (t->array_bound &&
        (fputs(",\"array_bound\":", o) == EOF ||
         !expr(o, depth, t->array_bound->expression)))
        return 0;
    if (t->kind == TYPE_FUNCTION) {
        const Decl *signature = t->function_signature;
        if (!signature || fputs(",\"return_type\":", o) == EOF ||
            !type(o, depth, signature->as.function.return_type) || fputs(",\"parameters\":[", o) == EOF)
            return 0;
        for (size_t i = 0; i < signature->as.function.parameters.count; ++i) {
            if ((i && fputc(',', o) == EOF) ||
                !type(o, depth, signature->as.function.parameters.items[i].type))
                return 0;
        }
        if (fputc(']', o) == EOF ||
            (signature->as.function.variadic && fputs(",\"variadic\":true", o) == EOF))
            return 0;
    }
    if (t->kind == TYPE_ARRAY) {
        if (fputs(",\"element\":", o) == EOF || !type(o, depth, t->pointee) ||
            (t->array_length_from_typedef &&
             fprintf(o, ",\"length\":\"%zu\"", t->array_length) < 0))
            return 0;
    }
    if (t->kind == TYPE_STRUCT) {
        if (fputs(",\"name\":", o) == EOF || !str(o, t->struct_name))
            return 0;
    }
    if (t->kind == TYPE_ENUM) {
        if (fputs(",\"tag\":", o) == EOF ||
            (t->struct_name ? !str(o, t->struct_name) : fputs("null", o) == EOF) ||
            fprintf(o, ",\"id\":%zu", t->array_length) < 0)
            return 0;
    }
    return fputc('}', o) != EOF;
}
/**
 * @brief Serializes a type while limiting recursive AST depth.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param t Type node to serialize.
 * @return One when the JSON data is written successfully; zero on depth or output failure.
 */
static int type(FILE *o, unsigned depth, const Type *t)
{
    if (depth >= AST_JSON_MAX_RECURSION_DEPTH)
        return 0;
    return type_impl(o, depth + 1, t);
}
/**
 * @brief Serializes an expression array to JSON.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param a Array of expressions to serialize.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int expr_array(FILE *o, unsigned depth, const ExprArray *a)
{
    size_t i;
    if (fputc('[', o) == EOF)
        return 0;
    for (i = 0; i < a->count; i++) {
        if (i && fputc(',', o) == EOF)
            return 0;
        if (!expr(o, depth, a->items[i]))
            return 0;
    }
    return fputc(']', o) != EOF;
}
/**
 * @brief Serializes an expression tree to JSON.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param e Expression node being inspected or emitted.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int expr_impl(FILE *o, unsigned depth, const Expr *e)
{
    if (!e)
        return fputs("null", o) != EOF;
    if (fputs("{\"kind\":", o) == EOF)
        return 0;
    static const char *n[] = {
        "integer",     "real",        "string",     "char",       "name",
        "unary",       "binary",      "assignment", "call",       "index",
        "member",      "conditional", "cast",       "array_initializer", "sizeof",
        "designated_initializer", "alignof", "offsetof"};
    if (!str(o, (unsigned)e->kind < sizeof(n) / sizeof(n[0]) ? n[e->kind] : "unknown") ||
        fputs(",\"location\":", o) == EOF || !loc(o, e->location))
        return 0;
    switch (e->kind) {
    case EXPR_INTEGER:
        if ((e->integer_suffix == INTEGER_SUFFIX_NONE ||
             e->integer_suffix == INTEGER_SUFFIX_L ||
             e->integer_suffix == INTEGER_SUFFIX_LL) && !e->integer_literal_unsigned) {
            if (fprintf(o, ",\"value\":\"%" PRId64 "\"", e->as.integer_value) < 0)
                return 0;
        } else if (fprintf(o, ",\"value\":\"%" PRIu64 "\"", e->unsigned_integer_value) < 0)
            return 0;
        if (fprintf(o, ",\"suffix\":\"%s\",\"out_of_range\":%s",
                    e->integer_suffix == INTEGER_SUFFIX_U ? "u" :
                    e->integer_suffix == INTEGER_SUFFIX_UL ? "ul" :
                    e->integer_suffix == INTEGER_SUFFIX_ULL ? "ull" :
                    e->integer_suffix == INTEGER_SUFFIX_LL ? "ll" :
                    e->integer_suffix == INTEGER_SUFFIX_L ? "l" : "none",
                    e->integer_literal_out_of_range ? "true" : "false") < 0)
            return 0;
        break;
    case EXPR_REAL:
        if (e->real_literal_extended_text) {
            if (fputs(",\"type\":\"long_double\",\"value\":", o) == EOF ||
                !str(o, e->real_literal_extended_text))
                return 0;
            break;
        }
        if (e->real_literal_float && fputs(",\"type\":\"float\"", o) == EOF)
            return 0;
        if (!isfinite(e->as.real_value)) {
            if (fputs(",\"value\":null", o) == EOF)
                return 0;
        } else if (fprintf(o, ",\"value\":%.17g", e->as.real_value) < 0)
            return 0;
        break;
    case EXPR_STRING:
        if ((e->string_literal_wide && fputs(",\"wide\":true", o) == EOF) ||
            fputs(",\"value\":", o) == EOF || !bytes(o, e->as.string.bytes, e->as.string.length) ||
            fprintf(o, ",\"length\":%zu", e->as.string.length) < 0)
            return 0;
        break;
    case EXPR_CHAR:
        if (e->char_literal_wide && fputs(",\"wide\":true", o) == EOF)
            return 0;
        if (fprintf(o, ",\"value\":%d", (int)e->as.char_value) < 0)
            return 0;
        break;
    case EXPR_NAME:
        if (fputs(",\"name\":", o) == EOF || !str(o, e->as.name.name))
            return 0;
        break;
    case EXPR_UNARY:
        if (fputs(",\"operator\":", o) == EOF ||
            !str(o, unary_name(e->as.unary.op)) ||
            fputs(",\"operand\":", o) == EOF || !expr(o, depth, e->as.unary.operand))
            return 0;
        break;
    case EXPR_BINARY:
        if (fputs(",\"operator\":", o) == EOF || !str(o, binary_name(e->as.binary.op)) ||
            fputs(",\"left\":", o) == EOF || !expr(o, depth, e->as.binary.left) ||
            fputs(",\"right\":", o) == EOF || !expr(o, depth, e->as.binary.right))
            return 0;
        break;
    case EXPR_ASSIGN:
        if (fputs(",\"operator\":", o) == EOF || !str(o, assign_name(e->as.assign.op)) ||
            fputs(",\"target\":", o) == EOF || !expr(o, depth, e->as.assign.target) ||
            fputs(",\"value\":", o) == EOF || !expr(o, depth, e->as.assign.value))
            return 0;
        break;
    case EXPR_CALL:
        if (fputs(",\"callee\":", o) == EOF || !expr(o, depth, e->as.call.callee) ||
            fputs(",\"arguments\":", o) == EOF || !expr_array(o, depth, &e->as.call.arguments))
            return 0;
        break;
    case EXPR_ARRAY_INIT:
        if (fputs(",\"elements\":", o) == EOF || !expr_array(o, depth, &e->as.array_init))
            return 0;
        break;
    case EXPR_DESIGNATED:
        if (fputs(",\"designators\":[", o) == EOF)
            return 0;
        for (size_t i = 0; i < e->as.designated.count; ++i) {
            const InitializerDesignator *d = &e->as.designated.items[i];
            if ((i && fputc(',', o) == EOF) || fputs("{\"kind\":", o) == EOF ||
                !str(o, d->field ? "field" : "index") ||
                fputs(",\"location\":", o) == EOF || !loc(o, d->location))
                return 0;
            if (d->field) {
                if (fputs(",\"name\":", o) == EOF || !str(o, d->field))
                    return 0;
            } else if (fputs(",\"index\":", o) == EOF || !expr(o, depth, d->index))
                return 0;
            if (fputc('}', o) == EOF)
                return 0;
        }
        if (fputs("],\"value\":", o) == EOF || !expr(o, depth, e->as.designated.value))
            return 0;
        break;
    case EXPR_OFFSETOF:
        if (fputs(",\"target_type\":", o) == EOF || !type(o, depth, e->as.offset_of.type) ||
            (e->as.offset_of.enum_definition &&
             (fputs(",\"enum_definition\":", o) == EOF ||
              !decl(o, depth, e->as.offset_of.enum_definition))) ||
            fputs(",\"designators\":[", o) == EOF)
            return 0;
        for (size_t i = 0; i < e->as.offset_of.count; ++i) {
            const InitializerDesignator *d = &e->as.offset_of.items[i];
            if ((i && fputc(',', o) == EOF) || fputs("{\"kind\":", o) == EOF ||
                !str(o, d->field ? "field" : "index") ||
                fputs(",\"location\":", o) == EOF || !loc(o, d->location))
                return 0;
            if (d->field) {
                if (fputs(",\"name\":", o) == EOF || !str(o, d->field))
                    return 0;
            } else if (fputs(",\"index\":", o) == EOF || !expr(o, depth, d->index))
                return 0;
            if (fputc('}', o) == EOF)
                return 0;
        }
        if (fputc(']', o) == EOF)
            return 0;
        break;
    case EXPR_INDEX:
        if (fputs(",\"base\":", o) == EOF || !expr(o, depth, e->as.index.base) ||
            fputs(",\"index\":", o) == EOF || !expr(o, depth, e->as.index.index))
            return 0;
        break;
    case EXPR_MEMBER:
        if (fputs(",\"base\":", o) == EOF || !expr(o, depth, e->as.member.base) ||
            fputs(",\"name\":", o) == EOF || !str(o, e->as.member.name) ||
            fprintf(o, ",\"through_pointer\":%s", e->as.member.through_pointer ? "true" : "false") <
                0)
            return 0;
        break;
    case EXPR_CONDITIONAL:
        if (fputs(",\"condition\":", o) == EOF || !expr(o, depth, e->as.conditional.condition) ||
            fputs(",\"then\":", o) == EOF || !expr(o, depth, e->as.conditional.then_expr) ||
            fputs(",\"else\":", o) == EOF || !expr(o, depth, e->as.conditional.else_expr))
            return 0;
        break;
    case EXPR_CAST:
        if (fputs(",\"target_type\":", o) == EOF || !type(o, depth, e->as.cast.target_type) ||
            (e->as.cast.enum_definition &&
             (fputs(",\"enum_definition\":", o) == EOF ||
              !decl(o, depth, e->as.cast.enum_definition))) ||
            fputs(",\"operand\":", o) == EOF || !expr(o, depth, e->as.cast.operand))
            return 0;
        break;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (fputs(",\"target_type\":", o) == EOF || !type(o, depth, e->as.size_of.type) ||
            (e->as.size_of.enum_definition &&
             (fputs(",\"enum_definition\":", o) == EOF ||
              !decl(o, depth, e->as.size_of.enum_definition))) ||
            fputs(",\"operand\":", o) == EOF || !expr(o, depth, e->as.size_of.operand))
            return 0;
        break;
    }
    return fputc('}', o) != EOF;
}
/**
 * @brief Serializes an expression while limiting recursive AST depth.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param e Expression node to serialize.
 * @return One when the JSON data is written successfully; zero on depth or output failure.
 */
static int expr(FILE *o, unsigned depth, const Expr *e)
{
    if (depth >= AST_JSON_MAX_RECURSION_DEPTH)
        return 0;
    return expr_impl(o, depth + 1, e);
}
/**
 * @brief Serializes a declaration initializer to JSON.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param i Initializer value to serialize.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int initializer(FILE *o, unsigned depth, const Initializer *i)
{
    if (fputs("{\"kind\":", o) == EOF)
        return 0;
    const char *n = i->kind == INIT_NONE ? "none" : "expression";
    if (!str(o, n) || fputs(",\"expression\":", o) == EOF || !expr(o, depth, i->expression))
        return 0;
    return fputc('}', o) != EOF;
}
/**
 * @brief Serializes a declaration to JSON.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param d Declaration node being inspected or emitted.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int decl_impl(FILE *o, unsigned depth, const Decl *d)
{
    if (!d)
        return fputs("null", o) != EOF;
    if (fputs("{\"kind\":", o) == EOF)
        return 0;
    const char *n = d->kind == DECL_VARIABLE   ? "variable"
                    : d->kind == DECL_FUNCTION ? "function"
                    : d->kind == DECL_TYPE_ALIAS ? "type_alias"
                    : d->kind == DECL_ENUM ? "enum"
                    : d->kind == DECL_STATIC_ASSERT ? "static_assert"
                    : d->kind == DECL_STRUCT_SCOPE ? (d->as.structure.is_union ? "union_scope" : "struct_scope")
                    : d->as.structure.is_union ? "union" : "struct";
    if (!str(o, n) || fputs(",\"location\":", o) == EOF || !loc(o, d->location))
        return 0;
    if (d->alignment_specifiers) {
        if (fputs(",\"alignment_specifiers\":[", o) == EOF)
            return 0;
        for (size_t i = 0; i < d->alignment_specifiers->expressions.count; ++i) {
            if ((i && fputc(',', o) == EOF) ||
                !expr(o, depth, d->alignment_specifiers->expressions.items[i]))
                return 0;
        }
        if (fputc(']', o) == EOF)
            return 0;
    }
    if (d->kind == DECL_VARIABLE) {
        if (fprintf(o, ",\"static\":%s,\"extern\":%s", d->is_static ? "true" : "false",
                    d->is_extern ? "true" : "false") < 0 ||
            (d->is_thread_local && fputs(",\"thread_local\":true", o) == EOF) ||
            fputs(",\"name\":", o) == EOF ||
            (!d->as.variable.name ? fputs("null", o) == EOF
                                             : !str(o, d->as.variable.name)) ||
            fputs(",\"type\":", o) == EOF || !type(o, depth, d->as.variable.type) ||
            fputs(",\"dynamic_array\":false", o) == EOF ||
            fputs(",\"array_size\":", o) == EOF || !expr(o, depth, d->as.variable.array_size) ||
            fputs(",\"anonymous_member\":", o) == EOF ||
            fputs(d->as.variable.anonymous_member ? "true" : "false", o) == EOF ||
            (d->as.variable.is_bit_field &&
             (fputs(",\"bit_field_width\":", o) == EOF ||
              !expr(o, depth, d->as.variable.bit_field_width))) ||
            fputs(",\"initializer\":", o) == EOF || !initializer(o, depth, &d->as.variable.initializer))
            return 0;
    } else if (d->kind == DECL_FUNCTION) {
        size_t i;
        if (fprintf(o, ",\"static\":%s,\"extern\":%s", d->is_static ? "true" : "false",
                    d->is_extern ? "true" : "false") < 0 ||
            (d->as.function.is_inline && !d->is_static &&
             fputs(",\"inline\":true", o) == EOF) ||
            fputs(",\"name\":", o) == EOF || !str(o, d->as.function.name) ||
            fputs(",\"return_type\":", o) == EOF || !type(o, depth, d->as.function.return_type) ||
            fprintf(o, ",\"noreturn\":%s,\"parameters\":[",
                    d->as.function.noreturn ? "true" : "false") < 0)
            return 0;
        for (i = 0; i < d->as.function.parameters.count; i++) {
            Parameter *p = &d->as.function.parameters.items[i];
            if (i && fputc(',', o) == EOF)
                return 0;
            if (fputs("{\"name\":", o) == EOF ||
                (p->name ? !str(o, p->name) : fputs("null", o) == EOF) ||
                fputs(",\"type\":", o) == EOF || !type(o, depth, p->type) ||
                fputs(",\"location\":", o) == EOF || !loc(o, p->location) ||
                (p->enum_definition &&
                 (fputs(",\"enum_definition\":", o) == EOF ||
                  !decl(o, depth, p->enum_definition))) || fputc('}', o) == EOF)
                return 0;
        }
        if (fputc(']', o) == EOF ||
            (d->as.function.variadic && fputs(",\"variadic\":true", o) == EOF) ||
            fputs(",\"body\":", o) == EOF || !stmt(o, depth, d->as.function.body))
            return 0;
    } else if (d->kind == DECL_TYPE_ALIAS) {
        if (fputs(",\"name\":", o) == EOF || !str(o, d->as.type_alias.name) ||
            fputs(",\"type\":", o) == EOF || !type(o, depth, d->as.type_alias.type))
            return 0;
    } else if (d->kind == DECL_STATIC_ASSERT) {
        if (fputs(",\"condition\":", o) == EOF ||
            !expr(o, depth, d->as.static_assertion.condition) ||
            fputs(",\"message\":", o) == EOF ||
            !expr(o, depth, d->as.static_assertion.message))
            return 0;
    } else if (d->kind == DECL_STRUCT_SCOPE) {
        if (fputs(",\"name\":", o) == EOF || !str(o, d->as.structure.name))
            return 0;
    } else if (d->kind == DECL_ENUM) {
        if (fputs(",\"tag\":", o) == EOF ||
            (d->as.enumeration.tag ? !str(o, d->as.enumeration.tag)
                                   : fputs("null", o) == EOF) ||
            fprintf(o, ",\"id\":%zu,\"enumerators\":[", d->as.enumeration.identity) < 0)
            return 0;
        for (size_t i = 0; i < d->as.enumeration.count; ++i) {
            const Enumerator *item = &d->as.enumeration.items[i];
            if ((i && fputc(',', o) == EOF) || fputs("{\"name\":", o) == EOF ||
                !str(o, item->name) || fputs(",\"location\":", o) == EOF ||
                !loc(o, item->location) || fputs(",\"value\":", o) == EOF ||
                !expr(o, depth, item->value) || fputc('}', o) == EOF)
                return 0;
        }
        if (fputc(']', o) == EOF)
            return 0;
    } else {
        size_t i;
        if (fputs(",\"name\":", o) == EOF || !str(o, d->as.structure.name) ||
            fprintf(o, ",\"anonymous\":%s", d->as.structure.anonymous ? "true" : "false") < 0)
            return 0;
        if (d->as.structure.pack_alignment &&
            fprintf(o, ",\"pack_alignment\":%u", d->as.structure.pack_alignment) < 0)
            return 0;
        if (fputs(",\"fields\":[", o) == EOF)
            return 0;
        for (i = 0; i < d->as.structure.field_count; i++) {
            if (i && fputc(',', o) == EOF)
                return 0;
            if (!decl(o, depth, d->as.structure.fields[i]))
                return 0;
        }
        if (fputc(']', o) == EOF)
            return 0;
        if (d->as.structure.assertion_count) {
            if (fputs(",\"assertions\":[", o) == EOF)
                return 0;
            for (i = 0; i < d->as.structure.assertion_count; ++i) {
                Decl *assertion = d->as.structure.assertions[i];
                if ((i && fputc(',', o) == EOF) ||
                    fprintf(o, "{\"preceding_fields\":%zu,\"declaration\":",
                            assertion->as.static_assertion.preceding_fields) < 0 ||
                    !decl(o, depth, assertion) || fputc('}', o) == EOF)
                    return 0;
            }
            if (fputc(']', o) == EOF)
                return 0;
        }
    }
    return fputc('}', o) != EOF;
}
/**
 * @brief Serializes a declaration while limiting recursive AST depth.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param d Declaration node to serialize.
 * @return One when the JSON data is written successfully; zero on depth or output failure.
 */
static int decl(FILE *o, unsigned depth, const Decl *d)
{
    if (depth >= AST_JSON_MAX_RECURSION_DEPTH)
        return 0;
    return decl_impl(o, depth + 1, d);
}
/**
 * @brief Serializes a statement array to JSON.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param a Array of statements to serialize.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int stmt_array(FILE *o, unsigned depth, const StmtArray *a)
{
    size_t i;
    if (fputc('[', o) == EOF)
        return 0;
    for (i = 0; i < a->count; i++) {
        if (i && fputc(',', o) == EOF)
            return 0;
        if (!stmt(o, depth, a->items[i]))
            return 0;
    }
    return fputc(']', o) != EOF;
}
/**
 * @brief Serializes a statement tree to JSON.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param s Statement node being inspected or emitted.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
static int stmt_impl(FILE *o, unsigned depth, const Stmt *s)
{
    if (!s)
        return fputs("null", o) != EOF;
    if (fputs("{\"kind\":", o) == EOF)
        return 0;
    static const char *n[] = {"block", "declaration", "expression", "if",     "switch", "for",
                              "loop",  "break",       "continue",   "return", "goto", "label",
                              "do_while"};
    if (!str(o, (unsigned)s->kind < sizeof(n) / sizeof(n[0]) ? n[s->kind] : "unknown") ||
        fputs(",\"location\":", o) == EOF || !loc(o, s->location))
        return 0;
    switch (s->kind) {
    case STMT_BLOCK:
        if (fputs(",\"statements\":", o) == EOF || !stmt_array(o, depth, &s->as.block.statements))
            return 0;
        break;
    case STMT_DECLARATION:
        if (fputs(",\"declaration\":", o) == EOF || !decl(o, depth, s->as.declaration.declaration))
            return 0;
        break;
    case STMT_EXPRESSION:
        if (fputs(",\"expression\":", o) == EOF || !expr(o, depth, s->as.expression.expression))
            return 0;
        break;
    case STMT_IF:
        if (fputs(",\"condition\":", o) == EOF || !expr(o, depth, s->as.if_stmt.condition) ||
            fputs(",\"then\":", o) == EOF || !stmt(o, depth, s->as.if_stmt.then_branch) ||
            fputs(",\"else\":", o) == EOF || !stmt(o, depth, s->as.if_stmt.else_branch))
            return 0;
        break;
    case STMT_SWITCH:
        if (fputs(",\"expression\":", o) == EOF || !expr(o, depth, s->as.switch_stmt.expression))
            return 0;
        if (s->as.switch_stmt.prefix.count &&
            (fputs(",\"prefix\":", o) == EOF || !stmt_array(o, depth, &s->as.switch_stmt.prefix)))
            return 0;
        if (fputs(",\"cases\":[", o) == EOF)
            return 0;
        for (size_t i = 0; i < s->as.switch_stmt.cases.count; i++) {
            const SwitchCase *c = &s->as.switch_stmt.cases.items[i];
            if (i && fputc(',', o) == EOF)
                return 0;
            if (fputs("{\"value\":", o) == EOF || !expr(o, depth, c->value) ||
                fputs(",\"statements\":", o) == EOF || !stmt_array(o, depth, &c->statements))
                return 0;
            if (fputc('}', o) == EOF)
                return 0;
        }
        if (fputc(']', o) == EOF)
            return 0;
        break;
    case STMT_FOR:
        if (fputs(",\"initializer\":", o) == EOF || !stmt(o, depth, s->as.for_stmt.initializer) ||
            fputs(",\"condition\":", o) == EOF || !expr(o, depth, s->as.for_stmt.condition) ||
            fputs(",\"update\":", o) == EOF || !expr(o, depth, s->as.for_stmt.update) ||
            fputs(",\"body\":", o) == EOF || !stmt(o, depth, s->as.for_stmt.body))
            return 0;
        break;
    case STMT_LOOP:
    case STMT_DO:
        if (fputs(",\"condition\":", o) == EOF || !expr(o, depth, s->as.loop_stmt.condition) ||
            fputs(",\"body\":", o) == EOF || !stmt(o, depth, s->as.loop_stmt.body))
            return 0;
        break;
    case STMT_RETURN:
        if (fputs(",\"expression\":", o) == EOF || !expr(o, depth, s->as.return_stmt.expression))
            return 0;
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
        break;
    case STMT_GOTO:
    case STMT_LABEL:
        if (fputs(",\"name\":", o) == EOF || !str(o, s->as.label.name))
            return 0;
        if (s->kind == STMT_LABEL &&
            (fputs(",\"statement\":", o) == EOF || !stmt(o, depth, s->as.label.statement)))
            return 0;
        break;
    }
    return fputc('}', o) != EOF;
}
/**
 * @brief Serializes a statement while limiting recursive AST depth.
 * @param o Output FILE stream receiving JSON.
 * @param depth Current recursive AST depth.
 * @param s Statement node to serialize.
 * @return One when the JSON data is written successfully; zero on depth or output failure.
 */
static int stmt(FILE *o, unsigned depth, const Stmt *s)
{
    if (depth >= AST_JSON_MAX_RECURSION_DEPTH)
        return 0;
    return stmt_impl(o, depth + 1, s);
}
/**
 * @brief Writes an AST program as JSON.
 * @param out Output FILE stream receiving the JSON document.
 * @param p AST program being searched or emitted.
 * @return One when the JSON data is written successfully; zero on output failure.
 */
int ast_write_json(FILE *out, const AstProgram *p)
{
    size_t i;
    if (!out || !p)
        return 0;
    if (fprintf(out, "{\"format\":\"CRUNCH-AST\",\"version\":%d,\"declarations\":[",
                p->contains_variable_hex_escape ? 49 : p->contains_wide_string ? 48 : p->contains_external_inline ? 47 : p->contains_wide_char ? 46 : p->contains_static_assert_without_message ? 45 : p->contains_offsetof ? 44 : p->contains_pack ? 43 : p->contains_thread_local ? 42 : p->contains_alignas ? 41 : p->contains_scalar_compound_literal ? 40 : p->contains_array_compound_literal ? 39 : p->contains_alignof ? 38 : p->contains_bit_field ? 37 : p->contains_volatile ? 36 : p->contains_static_assert ? 35 : p->contains_nested_array ? 34 : p->contains_long_double ? 33 : p->contains_array_typedef ? 32 : p->contains_variadic ? 31 : p->contains_comma_expression ? 30 : p->contains_do_while ? 29 : (p->contains_function_types || p->contains_unnamed_parameters) ? 28 : p->contains_union ? 27 : p->contains_named_labels ? 26 : p->contains_designator ? 25 : p->contains_embedded_enum ? 24 : p->contains_enum ? 23 : p->contains_bool ? 22 : p->contains_long_long ? 21 : p->contains_long_suffix ? 20 : p->contains_character_types ? 19 : p->contains_short ? 18 : p->contains_fixed_array ? 17 : p->contains_float ? 16 : 15) < 0)
        return 0;
    for (i = 0; i < p->declarations.count; i++) {
        if (i && fputc(',', out) == EOF)
            return 0;
        if (!decl(out, 0, p->declarations.items[i]))
            return 0;
    }
    if (fputs("]}\n", out) == EOF)
        return 0;
    return !ferror(out);
}
