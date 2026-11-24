#ifndef CRUNCH_COMMON_CONSTANT_EXPR_H
#define CRUNCH_COMMON_CONSTANT_EXPR_H

#include <stdbool.h>
#include <stdint.h>
#include "parser/ast.h"

/**
 * @brief Evaluates a supported integer constant expression.
 * @param expression Expression to evaluate.
 * @param value Receives the normalized integer value.
 * @return True on success, false for unsupported or invalid expressions.
 */
bool constant_expr_integer(const Expr *expression, int64_t *value);

/**
 * @brief Callback for evaluating an additional integer-valued expression.
 * @param expression Expression to inspect.
 * @param value Receives the result on success.
 * @param context State supplied by the caller.
 * @return True when the expression was evaluated.
 */
typedef bool (*IntegerConstantResolver)(const Expr *expression, int64_t *value, void *context);

/**
 * @brief Evaluates an integer expression with a resolver for additional forms.
 * @param expression Expression to evaluate.
 * @param value Receives the normalized integer value.
 * @param resolver Optional callback that evaluates a supported expression.
 * @param context State passed to the resolver.
 * @return True on success, false for unsupported or invalid expressions.
 */
bool constant_expr_integer_with_resolver(const Expr *expression, int64_t *value,
                                         IntegerConstantResolver resolver, void *context);

/**
 * @brief Checks whether an expression is an integer constant expression with value zero.
 * @param expression Expression to evaluate.
 * @return True when it is an integer constant expression whose value is zero.
 */
bool constant_expr_is_zero(const Expr *expression);

/**
 * @brief Counts decoded string-literal elements including the terminator.
 * @param expression String literal to inspect.
 * @return Element count, or zero when the expression is not a string literal.
 */
size_t string_array_count(const Expr *expression);

typedef const Decl *(*LayoutStructLookup)(const void *context, const char *name);

typedef struct {
    size_t offset;
    unsigned bit_offset;
    unsigned bit_width;
} FieldLayout;

/**
 * @brief Identifies a structure field declared as an incomplete array.
 * @param field Field declaration to inspect.
 * @return True when the field has an omitted array bound.
 */
bool field_is_flexible_array(const Decl *field);

/**
 * @brief Identifies a flexible or zero-length structure array member.
 * @param field Field declaration to inspect.
 * @return True when the field contributes no element storage.
 */
bool field_is_zero_storage(const Decl *field);

/**
 * @brief Computes a target type's size and alignment using structure lookup.
 * @param type Type to lay out.
 * @param lookup Resolves structure names.
 * @param context State passed to lookup.
 * @param size Output size in bytes.
 * @param alignment Output alignment in bytes.
 * @return True for a supported complete layout.
 */
bool type_layout_size(const Type *type, LayoutStructLookup lookup, const void *context,
                      size_t *size, size_t *alignment);

/**
 * @brief Computes a structure's field offsets, size, and alignment.
 * @param structure Structure declaration to lay out.
 * @param lookup Resolves structure names.
 * @param context State passed to lookup.
 * @param offsets Output array with room for all field offsets, or NULL.
 * @param size Output size in bytes.
 * @param alignment Output alignment in bytes.
 * @return True for a supported complete layout.
 */
bool type_layout_struct(const Decl *structure, LayoutStructLookup lookup, const void *context,
                        size_t *offsets, size_t *size, size_t *alignment);

/**
 * @brief Computes field byte and bit positions alongside aggregate layout.
 * @param structure Structure declaration to lay out.
 * @param lookup Resolves structure names.
 * @param context State passed to lookup.
 * @param fields Output array with room for all field layouts, or NULL.
 * @param size Output size in bytes.
 * @param alignment Output alignment in bytes.
 * @return True for a supported complete layout.
 */
bool type_layout_struct_fields(const Decl *structure, LayoutStructLookup lookup, const void *context,
                               FieldLayout *fields, size_t *size, size_t *alignment);

#endif
