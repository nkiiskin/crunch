#include "common/constant_expr.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

static bool layout_type(const Type *, LayoutStructLookup, const void *, size_t *, size_t *, unsigned);

/**
 * @brief Identifies a structure field declared as an incomplete array.
 * @param field Field declaration to inspect.
 * @return True when the field has an omitted array bound.
 */
bool field_is_flexible_array(const Decl *field) {
    const Type *type = field && field->kind == DECL_VARIABLE ? field->as.variable.type : NULL;
    return type && type->kind == TYPE_ARRAY && !type->array_length && !type->array_bound &&
           !field->as.variable.array_size;
}

/**
 * @brief Identifies a field with no array element storage.
 * @param field Field declaration to inspect.
 * @return True for a flexible or zero-length array member.
 */
bool field_is_zero_storage(const Decl *field) {
    const Type *type = field && field->kind == DECL_VARIABLE ? field->as.variable.type : NULL;
    return field_is_flexible_array(field) ||
           (type && type->kind == TYPE_ARRAY && type->is_zero_length_array);
}

/**
 * @brief Counts decoded string-literal elements including the terminator.
 * @param x String literal to inspect.
 * @return Element count, or zero when the expression is not a string literal.
 */
size_t string_array_count(const Expr *x) {
    if (!x || x->kind != EXPR_STRING || x->as.string.length < 2)
        return 0;
    if (x->string_literal_wide)
        return x->as.string.wide_count + 1;
    const unsigned char *b = x->as.string.bytes;
    size_t n = x->as.string.length, i = 1, count = 0;
    while (i + 1 < n) {
        if (b[i] == '\\') {
            if (i + 1 < n - 1 && b[i + 1] == 'x') {
                i += 2;
                while (i < n - 1 && isxdigit(b[i])) ++i;
            }
            else if (i + 1 < n - 1 && b[i + 1] >= '0' && b[i + 1] <= '7') {
                i += 2;
                for (int digits = 1; digits < 3 && i < n - 1 &&
                     b[i] >= '0' && b[i] <= '7'; ++digits)
                    ++i;
            }
            else
                i += 2;
        } else
            i++;
        count++;
    }
    return count + 1;
}

/**
 * @brief Calculates structure layout recursively.
 * @param structure Structure declaration.
 * @param lookup Structure lookup callback.
 * @param context Callback context.
 * @param offsets Optional output field offsets.
 * @param fields Optional output field layouts.
 * @param size Output size.
 * @param alignment Output alignment.
 * @param depth Current recursion depth.
 * @return True for a complete supported layout.
 */
static bool layout_struct(const Decl *structure, LayoutStructLookup lookup, const void *context,
                          size_t *offsets, FieldLayout *fields, size_t *size, size_t *alignment,
                          unsigned depth) {
    if (!structure || structure->kind != DECL_STRUCT || depth > 24 ||
        structure->as.structure.field_count > 256)
        return false;
    size_t off = 0, max_align = 1;
    unsigned bit = 0;
    for (size_t i = 0; i < structure->as.structure.field_count; ++i) {
        const Decl *field = structure->as.structure.fields[i];
        size_t field_size, field_align;
        if (!field || field->kind != DECL_VARIABLE || !field->as.variable.type)
            return false;
        if (field->as.variable.type->kind == TYPE_POINTER &&
                   (!field->as.variable.type->pointee ||
                    (field->as.variable.type->pointee->kind != TYPE_BYTE &&
                    field->as.variable.type->pointee->kind != TYPE_BOOL &&
                     field->as.variable.type->pointee->kind != TYPE_SIGNED_CHAR &&
                     field->as.variable.type->pointee->kind != TYPE_UNSIGNED_CHAR &&
                     field->as.variable.type->pointee->kind != TYPE_SHORT &&
                     field->as.variable.type->pointee->kind != TYPE_UNSIGNED_SHORT &&
                     field->as.variable.type->pointee->kind != TYPE_INT &&
                     field->as.variable.type->pointee->kind != TYPE_ENUM &&
                     field->as.variable.type->pointee->kind != TYPE_LONG &&
                     field->as.variable.type->pointee->kind != TYPE_LONG_LONG &&
                     field->as.variable.type->pointee->kind != TYPE_UNSIGNED_INT &&
                     field->as.variable.type->pointee->kind != TYPE_UNSIGNED_LONG &&
                     field->as.variable.type->pointee->kind != TYPE_UNSIGNED_LONG_LONG &&
                     field->as.variable.type->pointee->kind != TYPE_REAL &&
                     field->as.variable.type->pointee->kind != TYPE_FLOAT &&
                     field->as.variable.type->pointee->kind != TYPE_LONG_DOUBLE &&
                     field->as.variable.type->pointee->kind != TYPE_VOID &&
                     field->as.variable.type->pointee->kind != TYPE_STRUCT &&
                     field->as.variable.type->pointee->kind != TYPE_FUNCTION &&
                     field->as.variable.type->pointee->kind != TYPE_ARRAY &&
                     field->as.variable.type->pointee->kind != TYPE_POINTER))) {
            return false;
        } else if (field_is_flexible_array(field)) {
            if (structure->as.structure.is_union || i + 1 != structure->as.structure.field_count ||
                !i || !layout_type(field->as.variable.type->pointee, lookup, context,
                                   &field_size, &field_align, depth + 1))
                return false;
            field_size = 0;
        } else if (!layout_type(field->as.variable.type, lookup, context,
                                &field_size, &field_align, depth + 1)) {
            return false;
        }
        if (field->semantic_alignment > field_align)
            field_align = field->semantic_alignment;
        if (structure->as.structure.pack_alignment &&
            (!field->as.variable.is_bit_field ||
             field->as.variable.resolved_bit_field_width != 0) &&
            field_align > structure->as.structure.pack_alignment)
            field_align = structure->as.structure.pack_alignment;
        if (field->as.variable.is_bit_field) {
            unsigned width = field->as.variable.resolved_bit_field_width;
            if (!field->as.variable.bit_field_width_resolved || field_size > SIZE_MAX / 8 ||
                width > field_size * 8)
                return false;
            if (field->as.variable.name && field_align > max_align)
                max_align = field_align;
            if (structure->as.structure.is_union) {
                if (offsets) offsets[i] = 0;
                if (fields) fields[i] = (FieldLayout){0, 0, width};
                size_t bytes = (width + 7) / 8;
                if (bytes > off) off = bytes;
                continue;
            }
            if (!width || (!structure->as.structure.pack_alignment &&
                           (off % field_align) * 8 + bit + width > field_size * 8)) {
                if (bit) {
                    if (off == SIZE_MAX) return false;
                    ++off;
                    bit = 0;
                }
                if (off > SIZE_MAX - (field_align - 1)) return false;
                off = (off + field_align - 1) / field_align * field_align;
            }
            if (offsets) offsets[i] = off;
            if (fields) fields[i] = (FieldLayout){off, bit, width};
            unsigned total = bit + width;
            if (off > SIZE_MAX - total / 8) return false;
            off += total / 8;
            bit = total % 8;
            continue;
        }
        if (field_align > max_align) max_align = field_align;
        if (structure->as.structure.is_union) {
            if (offsets) offsets[i] = 0;
            if (fields) fields[i] = (FieldLayout){0, 0, 0};
            if (field_size > off) off = field_size;
        } else {
            if (bit) {
                if (off == SIZE_MAX) return false;
                ++off;
                bit = 0;
            }
            if (off > SIZE_MAX - (field_align - 1)) return false;
            off = (off + field_align - 1) / field_align * field_align;
            if (offsets) offsets[i] = off;
            if (fields) fields[i] = (FieldLayout){off, 0, 0};
            if (off > SIZE_MAX - field_size) return false;
            off += field_size;
        }
    }
    if (bit) {
        if (off == SIZE_MAX) return false;
        ++off;
    }
    if (off > SIZE_MAX - (max_align - 1)) return false;
    off = (off + max_align - 1) / max_align * max_align;
    *size = off ? off : 1;
    *alignment = max_align;
    return true;
}

/**
 * @brief Calculates a type layout recursively.
 * @param type Type to inspect.
 * @param lookup Structure lookup callback.
 * @param context Callback context.
 * @param size Output size.
 * @param alignment Output alignment.
 * @param depth Current recursion depth.
 * @return True for a complete supported layout.
 */
static bool layout_type(const Type *type, LayoutStructLookup lookup, const void *context,
                        size_t *size, size_t *alignment, unsigned depth) {
    if (!type || depth > 24) return false;
    switch (type->kind) {
    case TYPE_BYTE: case TYPE_BOOL: case TYPE_SIGNED_CHAR: case TYPE_UNSIGNED_CHAR:
        *size = *alignment = 1; return true;
    case TYPE_SHORT: case TYPE_UNSIGNED_SHORT: *size = *alignment = 2; return true;
    case TYPE_INT: case TYPE_ENUM: case TYPE_UNSIGNED_INT: case TYPE_FLOAT: *size = *alignment = 4; return true;
    case TYPE_LONG: case TYPE_UNSIGNED_LONG: case TYPE_LONG_LONG: case TYPE_UNSIGNED_LONG_LONG:
    case TYPE_REAL: case TYPE_POINTER:
        *size = *alignment = 8; return true;
    case TYPE_LONG_DOUBLE:
        *size = *alignment = 16; return true;
    case TYPE_ARRAY: {
        size_t element_size, element_alignment;
        if ((!type->array_length && !type->is_zero_length_array) ||
            !layout_type(type->pointee, lookup, context, &element_size, &element_alignment,
                         depth + 1) || !element_size ||
            type->array_length > SIZE_MAX / element_size)
            return false;
        *size = type->array_length * element_size;
        *alignment = element_alignment;
        return true;
    }
    case TYPE_STRUCT:
        return type->struct_name && lookup &&
               layout_struct(lookup(context, type->struct_name), lookup, context,
                             NULL, NULL, size, alignment, depth + 1);
    case TYPE_VOID: case TYPE_FUNCTION: return false;
    }
    return false;
}

/**
 * @brief Computes a type's size and alignment.
 * @param type Type to lay out.
 * @param lookup Structure lookup callback.
 * @param context Callback context.
 * @param size Output size in bytes.
 * @param alignment Output alignment in bytes.
 * @return True for a supported complete layout.
 */
bool type_layout_size(const Type *type, LayoutStructLookup lookup, const void *context,
                      size_t *size, size_t *alignment) {
    return size && alignment && layout_type(type, lookup, context, size, alignment, 0);
}

/**
 * @brief Computes a structure's field offsets, size, and alignment.
 * @param structure Structure declaration.
 * @param lookup Structure lookup callback.
 * @param context Callback context.
 * @param offsets Optional output field offsets.
 * @param size Output size in bytes.
 * @param alignment Output alignment in bytes.
 * @return True for a supported complete layout.
 */
bool type_layout_struct(const Decl *structure, LayoutStructLookup lookup, const void *context,
                        size_t *offsets, size_t *size, size_t *alignment) {
    return size && alignment &&
           layout_struct(structure, lookup, context, offsets, NULL, size, alignment, 0);
}

/**
 * @brief Computes aggregate layout with bit positions for all fields.
 * @param structure Structure declaration to lay out.
 * @param lookup Resolves structure names.
 * @param context State passed to lookup.
 * @param fields Optional field layout output.
 * @param size Output size in bytes.
 * @param alignment Output alignment in bytes.
 * @return True for a supported complete layout.
 */
bool type_layout_struct_fields(const Decl *structure, LayoutStructLookup lookup, const void *context,
                               FieldLayout *fields, size_t *size, size_t *alignment) {
    return size && alignment &&
           layout_struct(structure, lookup, context, NULL, fields, size, alignment, 0);
}

/**
 * @brief Returns the integer evaluation width of a type.
 * @param type Type whose integer width is queried.
 * @return Width in bits; unsupported and missing types use 64 bits.
 */
static unsigned type_width(const Type *type) {
    if (!type)
        return 64;
    switch (type->kind) {
    case TYPE_BYTE:
    case TYPE_BOOL:
    case TYPE_SIGNED_CHAR:
    case TYPE_UNSIGNED_CHAR:
        return 8;
    case TYPE_SHORT:
    case TYPE_UNSIGNED_SHORT:
        return 16;
    case TYPE_INT:
    case TYPE_ENUM:
        return 32;
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_POINTER:
        return 64;
    default:
        return 64;
    }
}

/**
 * @brief Identifies expressions whose character type undergoes integer promotion.
 * @param expression Expression to inspect.
 * @return True for explicitly signed or unsigned characters, or plain char in C11 mode.
 */
static bool promoted_character(const Expr *expression) {
    if (!expression)
        return false;
    TypeKind kind = expression->kind == EXPR_CAST && expression->as.cast.target_type
                        ? expression->as.cast.target_type->kind : expression->semantic_type;
    return kind == TYPE_BOOL || kind == TYPE_SIGNED_CHAR || kind == TYPE_UNSIGNED_CHAR ||
           (expression->semantic_c11_numeric && kind == TYPE_BYTE);
}

/**
 * @brief Determines the integer width used to evaluate an expression.
 * @param expression Expression whose result width is inferred.
 * @return Evaluation width in bits, defaulting to 64 for unsupported expressions.
 */
static unsigned expression_width(const Expr *expression) {
    if (!expression)
        return 64;
    switch (expression->kind) {
    case EXPR_OFFSETOF:
        return 64;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        return 64;
    case EXPR_INTEGER:
        if (expression->integer_literal_out_of_range)
            return 64;
        if (expression->integer_suffix == INTEGER_SUFFIX_L ||
            expression->integer_suffix == INTEGER_SUFFIX_LL ||
            expression->integer_suffix == INTEGER_SUFFIX_ULL)
            return 64;
        if (expression->integer_suffix != INTEGER_SUFFIX_NONE ||
            expression->integer_literal_unsigned)
            return expression->integer_suffix == INTEGER_SUFFIX_UL ||
                           expression->unsigned_integer_value > UINT32_MAX ? 64 : 32;
        return expression->as.integer_value >= INT32_MIN &&
                       expression->as.integer_value <= INT32_MAX
                   ? 32
                   : 64;
    case EXPR_CHAR:
        return expression->char_literal_wide || expression->semantic_c11_numeric ? 32 : 8;
    case EXPR_UNARY:
        if (expression->as.unary.op == UNARY_LOGICAL_NOT)
            return 32;
        {
            unsigned width = expression_width(expression->as.unary.operand);
            return width == 16 || (width == 8 && promoted_character(expression->as.unary.operand))
                       ? 32 : width;
        }
    case EXPR_BINARY:
        switch (expression->as.binary.op) {
        case BINARY_EQUAL:
        case BINARY_NOT_EQUAL:
        case BINARY_LESS:
        case BINARY_LESS_EQUAL:
        case BINARY_GREATER:
        case BINARY_GREATER_EQUAL:
        case BINARY_LOGICAL_AND:
        case BINARY_LOGICAL_OR:
            return 32;
        default: {
            unsigned left = expression_width(expression->as.binary.left);
            unsigned right = expression_width(expression->as.binary.right);
            if (left == 16 || (left == 8 && promoted_character(expression->as.binary.left))) left = 32;
            if (right == 16 || (right == 8 && promoted_character(expression->as.binary.right))) right = 32;
            if (expression->semantic_c11_numeric &&
                (expression->as.binary.op == BINARY_SHIFT_LEFT ||
                 expression->as.binary.op == BINARY_SHIFT_RIGHT))
                return left;
            return left > right ? left : right;
        }
        }
    case EXPR_CONDITIONAL: {
        unsigned then_width = expression_width(expression->as.conditional.then_expr);
        unsigned else_width = expression_width(expression->as.conditional.else_expr);
        if (then_width == 16 || (then_width == 8 && promoted_character(expression->as.conditional.then_expr))) then_width = 32;
        if (else_width == 16 || (else_width == 8 && promoted_character(expression->as.conditional.else_expr))) else_width = 32;
        return then_width > else_width ? then_width : else_width;
    }
    case EXPR_CAST:
        return type_width(expression->as.cast.target_type);
    default:
        return 64;
    }
}

/**
 * @brief Normalizes bits to Crunch's unsigned byte or signed integer representation.
 * @param bits Bit pattern to normalize.
 * @param width Integer width in bits (8, 16, 32, or 64).
 * @return Zero-extended byte for width 8, or the signed value for other widths.
 */
static int64_t normalize(uint64_t bits, unsigned width) {
    if (width == 8)
        return (int64_t)(bits & UINT64_C(0xff));
    if (width == 16) {
        uint16_t low = (uint16_t)bits;
        return low <= INT16_MAX ? (int64_t)low : (int64_t)low - INT64_C(65536);
    }
    if (width == 32) {
        uint32_t low = (uint32_t)bits;
        return low <= INT32_MAX ? (int64_t)low : (int64_t)low - INT64_C(0x100000000);
    }
    return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
}

/**
 * @brief Evaluates a supported integer constant expression.
 * @param expression Expression tree to evaluate.
 * @param value Output receiving the evaluated value on success.
 * @param resolver Optional callback for additional expression forms.
 * @param context State passed to the resolver.
 * @return Nonzero on success; zero for unsupported expressions or invalid operations.
 */
static int evaluate_integer(const Expr *expression, int64_t *value,
                            IntegerConstantResolver resolver, void *context) {
    if (!expression || !value)
        return 0;
    unsigned width = expression_width(expression);
    switch (expression->kind) {
    case EXPR_NAME:
        if (!expression->semantic_enum_constant)
            return 0;
        *value = expression->semantic_enum_value;
        return 1;
    case EXPR_OFFSETOF:
        if (!expression->as.offset_of.resolved || !expression->as.offset_of.constant)
            return 0;
        *value = (int64_t)expression->as.offset_of.value;
        return 1;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (!expression->as.size_of.resolved)
            return 0;
        *value = expression->as.size_of.size;
        return 1;
    case EXPR_INTEGER:
        if (expression->integer_literal_out_of_range ||
            (expression->integer_suffix != INTEGER_SUFFIX_NONE &&
             expression->integer_suffix != INTEGER_SUFFIX_L &&
             expression->integer_suffix != INTEGER_SUFFIX_LL) ||
            expression->integer_literal_unsigned)
            return 0;
        *value = normalize((uint64_t)expression->as.integer_value, width);
        return 1;
    case EXPR_CHAR:
        *value = (int64_t)expression->as.char_value;
        return 1;
    case EXPR_UNARY: {
        int64_t operand;
        if (!evaluate_integer(expression->as.unary.operand, &operand, resolver, context))
            return 0;
        switch (expression->as.unary.op) {
        case UNARY_PLUS:
            *value = normalize((uint64_t)operand, width);
            return 1;
        case UNARY_MINUS:
            *value = normalize(UINT64_C(0) - (uint64_t)operand, width);
            return 1;
        case UNARY_LOGICAL_NOT:
            *value = operand == 0;
            return 1;
        case UNARY_BITWISE_NOT:
            *value = normalize(~(uint64_t)operand, width);
            return 1;
        default:
            return 0;
        }
    }
    case EXPR_BINARY: {
        if (resolver && resolver(expression, value, context))
            return 1;
        int64_t left;
        if (!evaluate_integer(expression->as.binary.left, &left, resolver, context))
            return 0;
        if (expression->as.binary.op == BINARY_LOGICAL_AND && left == 0) {
            *value = 0;
            return 1;
        }
        if (expression->as.binary.op == BINARY_LOGICAL_OR && left != 0) {
            *value = 1;
            return 1;
        }
        int64_t right;
        if (!evaluate_integer(expression->as.binary.right, &right, resolver, context))
            return 0;
        unsigned left_width = expression_width(expression->as.binary.left);
        unsigned right_width = expression_width(expression->as.binary.right);
        if (left_width == 16 || (left_width == 8 && promoted_character(expression->as.binary.left)))
            left_width = 32;
        if (right_width == 16 || (right_width == 8 && promoted_character(expression->as.binary.right)))
            right_width = 32;
        bool c11_shift = expression->semantic_c11_numeric &&
                         (expression->as.binary.op == BINARY_SHIFT_LEFT ||
                          expression->as.binary.op == BINARY_SHIFT_RIGHT);
        unsigned operand_width = c11_shift ? left_width :
                                 left_width > right_width ? left_width : right_width;
        left = normalize((uint64_t)left, operand_width);
        if (!c11_shift)
            right = normalize((uint64_t)right, operand_width);
        uint64_t left_bits = (uint64_t)left;
        uint64_t right_bits = (uint64_t)right;
        switch (expression->as.binary.op) {
        case BINARY_ADD:
            *value = normalize(left_bits + right_bits, width);
            return 1;
        case BINARY_SUBTRACT:
            *value = normalize(left_bits - right_bits, width);
            return 1;
        case BINARY_MULTIPLY:
            *value = normalize(left_bits * right_bits, width);
            return 1;
        case BINARY_DIVIDE:
        case BINARY_REMAINDER:
            if (right == 0 || (right == -1 &&
                               ((operand_width == 32 && left == INT32_MIN) ||
                                (operand_width == 64 && left == INT64_MIN))))
                return 0;
            *value = expression->as.binary.op == BINARY_DIVIDE
                         ? left / right
                         : left % right;
            return 1;
        case BINARY_EQUAL:
            *value = left == right;
            return 1;
        case BINARY_NOT_EQUAL:
            *value = left != right;
            return 1;
        case BINARY_LESS:
            *value = left < right;
            return 1;
        case BINARY_LESS_EQUAL:
            *value = left <= right;
            return 1;
        case BINARY_GREATER:
            *value = left > right;
            return 1;
        case BINARY_GREATER_EQUAL:
            *value = left >= right;
            return 1;
        case BINARY_LOGICAL_AND:
            *value = left != 0 && right != 0;
            return 1;
        case BINARY_LOGICAL_OR:
            *value = left != 0 || right != 0;
            return 1;
        case BINARY_BITWISE_AND:
            *value = normalize(left_bits & right_bits, width);
            return 1;
        case BINARY_BITWISE_OR:
            *value = normalize(left_bits | right_bits, width);
            return 1;
        case BINARY_BITWISE_XOR:
            *value = normalize(left_bits ^ right_bits, width);
            return 1;
        case BINARY_SHIFT_LEFT:
            if (right < 0 || (uint64_t)right >= operand_width)
                return 0;
            *value = normalize(left_bits << (unsigned)right, width);
            return 1;
        case BINARY_SHIFT_RIGHT:
            if (right < 0 || (uint64_t)right >= operand_width)
                return 0;
            if (right == 0) {
                *value = normalize(left_bits, width);
            } else {
                uint64_t shifted = left_bits >> (unsigned)right;
                if (left < 0)
                    shifted |= UINT64_MAX << (64U - (unsigned)right);
                *value = normalize(shifted, width);
            }
            return 1;
        default:
            return 0;
        }
    }
    case EXPR_CONDITIONAL: {
        if (resolver && resolver(expression, value, context)) {
            *value = normalize((uint64_t)*value, width);
            return 1;
        }
        int64_t condition, branch;
        if (!evaluate_integer(expression->as.conditional.condition, &condition, resolver, context) ||
            !evaluate_integer(condition != 0 ? expression->as.conditional.then_expr
                                             : expression->as.conditional.else_expr,
                             &branch, resolver, context))
            return 0;
        *value = normalize((uint64_t)branch, width);
        return 1;
    }
    case EXPR_CAST: {
        Type *target = expression->as.cast.target_type;
        if (!target || (target->kind != TYPE_BYTE && target->kind != TYPE_BOOL &&
                        target->kind != TYPE_SIGNED_CHAR &&
                        target->kind != TYPE_UNSIGNED_CHAR && target->kind != TYPE_SHORT &&
                        target->kind != TYPE_UNSIGNED_SHORT && target->kind != TYPE_INT &&
                        target->kind != TYPE_ENUM &&
                        target->kind != TYPE_LONG && target->kind != TYPE_LONG_LONG))
            return 0;
        int64_t operand;
        if (!evaluate_integer(expression->as.cast.operand, &operand, resolver, context))
            return 0;
        uint64_t byte = (uint64_t)operand & UINT64_C(0xff);
        *value = target->kind == TYPE_BOOL ? operand != 0
               : target->kind == TYPE_SIGNED_CHAR ? (byte <= INT8_MAX ? (int64_t)byte : (int64_t)byte - 256)
               : target->kind == TYPE_UNSIGNED_CHAR ? (int64_t)byte
               : target->kind == TYPE_UNSIGNED_SHORT ? (int64_t)((uint64_t)operand & 65535)
                                                    : normalize((uint64_t)operand, width);
        return 1;
    }
    default:
        return 0;
    }
}

/**
 * @brief Evaluates a supported integer constant expression.
 * @param expression Expression tree to evaluate.
 * @param value Receives the normalized integer value.
 * @return True on success, false for unsupported or invalid expressions.
 */
bool constant_expr_integer(const Expr *expression, int64_t *value) {
    return value && evaluate_integer(expression, value, NULL, NULL);
}

/**
 * @brief Evaluates an integer constant expression with an optional resolver.
 * @param expression Expression tree to evaluate.
 * @param value Receives the normalized integer value.
 * @param resolver Callback for additional expression forms.
 * @param context State passed to the resolver.
 * @return True on success, false for unsupported or invalid expressions.
 */
bool constant_expr_integer_with_resolver(const Expr *expression, int64_t *value,
                                         IntegerConstantResolver resolver, void *context) {
    return value && evaluate_integer(expression, value, resolver, context);
}

/**
 * @brief Tests whether an expression is a supported constant integer zero.
 * @param expression Expression tree to evaluate.
 * @return True when it evaluates to zero, including pointer casts of zero.
 */
bool constant_expr_is_zero(const Expr *expression) {
    if (!expression)
        return false;
    if (expression->kind == EXPR_CAST && expression->as.cast.target_type &&
        expression->as.cast.target_type->kind == TYPE_POINTER)
        return constant_expr_is_zero(expression->as.cast.operand);
    int64_t value;
    return evaluate_integer(expression, &value, NULL, NULL) && value == 0;
}
