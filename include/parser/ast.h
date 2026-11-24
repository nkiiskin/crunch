#ifndef CRUNCH_PARSER_AST_H
#define CRUNCH_PARSER_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/compiler.h"

/* ---------- Types ---------- */

typedef enum {
    TYPE_VOID,
    TYPE_INT,       /* Signed 32-bit integer. */
    TYPE_LONG,      /* Signed 64-bit integer. */
    TYPE_UNSIGNED_INT,
    TYPE_UNSIGNED_LONG,
    TYPE_BYTE,      /* Unsigned 8-bit integer. */
    TYPE_REAL,      /* IEEE 754 binary64. */
    TYPE_POINTER,
    TYPE_STRUCT,
    TYPE_FLOAT,     /* IEEE 754 binary32. */
    TYPE_ARRAY,
    TYPE_SHORT,
    TYPE_UNSIGNED_SHORT,
    TYPE_SIGNED_CHAR,
    TYPE_UNSIGNED_CHAR,
    TYPE_LONG_LONG,
    TYPE_UNSIGNED_LONG_LONG,
    TYPE_BOOL,
    TYPE_ENUM,
    TYPE_FUNCTION,
    TYPE_LONG_DOUBLE
} TypeKind;

typedef struct Type Type;
typedef struct Decl Decl;
typedef struct Expr Expr;
typedef struct ArrayBound ArrayBound;
typedef struct Parameter Parameter;

struct ArrayBound {
    Expr *expression;
    size_t references;
    size_t length;
    bool evaluated;
    bool valid;
    bool variable;
};

struct Type {
    TypeKind kind;
    Type *pointee;  /* Pointer pointee or fixed-array element type. */
    char *struct_name; /* Structure name or enum tag. */
    bool is_const;
    size_t array_length; /* Fixed-array count or enum identity. */
    Decl *enum_definition; /* Borrowed enum declaration, for TYPE_ENUM. */
    bool is_union; /* Distinguishes union tags from structure tags. */
    Decl *function_signature; /* Owned function declaration for TYPE_FUNCTION. */
    bool array_length_from_typedef;
    ArrayBound *array_bound; /* Shared parsed bound for nested or adjusted arrays. */
    bool is_volatile;
    bool is_zero_length_array; /* Resolved GNU zero-length structure member. */
};

/* ---------- Expressions ---------- */

typedef enum {
    EXPR_INTEGER,
    EXPR_REAL,
    EXPR_STRING,
    EXPR_CHAR,
    EXPR_NAME,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_INDEX,
    EXPR_MEMBER,
    EXPR_CONDITIONAL,
    EXPR_CAST,
    EXPR_ARRAY_INIT,
    EXPR_SIZEOF,
    EXPR_DESIGNATED,
    EXPR_ALIGNOF,
    EXPR_OFFSETOF
} ExprKind;

typedef enum {
    UNARY_PLUS,
    UNARY_MINUS,
    UNARY_LOGICAL_NOT,
    UNARY_BITWISE_NOT,
    UNARY_ADDRESS,
    UNARY_DEREFERENCE,
    UNARY_PRE_INCREMENT,
    UNARY_PRE_DECREMENT,
    UNARY_POST_INCREMENT,
    UNARY_POST_DECREMENT
} UnaryOp;

typedef enum {
    BINARY_ADD,
    BINARY_SUBTRACT,
    BINARY_MULTIPLY,
    BINARY_DIVIDE,
    BINARY_REMAINDER,
    BINARY_POWER,

    BINARY_EQUAL,
    BINARY_NOT_EQUAL,
    BINARY_LESS,
    BINARY_LESS_EQUAL,
    BINARY_GREATER,
    BINARY_GREATER_EQUAL,

    BINARY_LOGICAL_AND,
    BINARY_LOGICAL_OR,

    BINARY_BITWISE_AND,
    BINARY_BITWISE_OR,
    BINARY_BITWISE_XOR,
    BINARY_SHIFT_LEFT,
    BINARY_SHIFT_RIGHT,
    BINARY_COMMA
} BinaryOp;

typedef enum {
    ASSIGN_SIMPLE,
    ASSIGN_ADD,
    ASSIGN_SUBTRACT,
    ASSIGN_MULTIPLY,
    ASSIGN_DIVIDE,
    ASSIGN_REMAINDER,
    ASSIGN_BITWISE_AND,
    ASSIGN_BITWISE_OR,
    ASSIGN_BITWISE_XOR,
    ASSIGN_SHIFT_LEFT,
    ASSIGN_SHIFT_RIGHT
} AssignOp;

typedef struct Stmt Stmt;
typedef struct AstProgram AstProgram;
typedef struct InitializerPlan InitializerPlan;

typedef struct {
    Expr **items;
    size_t count;
} ExprArray;

typedef struct {
    ExprArray expressions;
    size_t references;
    bool register_storage;
    bool auto_storage;
} AlignmentSpecifiers;

typedef struct {
    char *field;
    size_t field_length;
    Expr *index;
    SourceLocation location;
    size_t resolved_index;
    size_t semantic_stride;
} InitializerDesignator;

typedef enum {
    INTEGER_SUFFIX_NONE,
    INTEGER_SUFFIX_U,
    INTEGER_SUFFIX_UL,
    INTEGER_SUFFIX_L,
    INTEGER_SUFFIX_LL,
    INTEGER_SUFFIX_ULL
} IntegerSuffix;

struct Expr {
    ExprKind kind;
    SourceLocation location;
    TypeKind semantic_type; /* Populated by semantic analysis. */
    bool semantic_unsigned; /* Expression or a descendant uses an unsigned type. */
    bool semantic_float; /* Expression or a descendant uses float. */
    bool semantic_c11_numeric; /* Numeric mode used by semantic analysis. */
    bool semantic_volatile_lvalue;
    bool semantic_bit_field;
    unsigned semantic_bit_field_width;
    bool semantic_function_name;
    bool semantic_builtin_alloca;
    bool semantic_enum_constant;
    int32_t semantic_enum_value;
    bool integer_literal_out_of_range;
    bool integer_literal_unsigned; /* Unsuffixed hexadecimal or octal literal has unsigned type. */
    bool real_literal_float;
    bool char_literal_wide;
    bool string_literal_wide;
    char *real_literal_extended_text;
    IntegerSuffix integer_suffix;
    uint64_t unsigned_integer_value; /* Used only for suffixed integer literals. */

    union {
        int64_t integer_value;
        double real_value;
        int32_t char_value;

        struct {
            unsigned char *bytes;
            size_t length; /* Excludes the implicit terminating zero. */
            unsigned char *wide_bytes;
            size_t wide_count;
        } string;

        struct {
            char *name;
            size_t length;
        } name;

        struct {
            UnaryOp op;
            Expr *operand;
        } unary;

        struct {
            BinaryOp op;
            Expr *left;
            Expr *right;
        } binary;

        struct {
            AssignOp op;
            Expr *target;
            Expr *value;
        } assign;

        struct {
            Expr *callee;
            ExprArray arguments;
        } call;

        ExprArray array_init;

        struct {
            InitializerDesignator *items;
            size_t count;
            Expr *value;
        } designated;

        struct {
            Expr *base;
            Expr *index;
        } index;

        struct {
            Expr *base;
            char *name;
            size_t name_length;
            bool through_pointer;
        } member;

        struct {
            Expr *condition;
            Expr *then_expr;
            Expr *else_expr;
        } conditional;

        struct {
            Type *target_type;
            Decl *enum_definition;
            Expr *operand;
            InitializerPlan *semantic_plan; /* Borrowed from the semantic model. */
            bool semantic_file_scope;
        } cast;

        struct {
            Type *type;
            Decl *enum_definition;
            Expr *operand;
            size_t type_end_offset; /* Visibility point after a type-name definition. */
            int64_t size;
            bool resolved;
            Decl *semantic_vla_decl; /* Borrowed declaration for runtime sizeof. */
            Parameter *semantic_vla_parameter; /* Borrowed parameter for runtime sizeof. */
            unsigned semantic_vla_dimension;
        } size_of;

        struct {
            Type *type;
            Decl *enum_definition;
            InitializerDesignator *items;
            size_t count;
            uint64_t base_offset;
            uint64_t value;
            bool resolved;
            bool constant;
        } offset_of;
    } as;
};

/* ---------- Declarations ---------- */

struct Parameter {
    Type *type;
    char *name;
    size_t name_length;
    SourceLocation location;
    Decl *enum_definition;
};

typedef struct {
    Parameter *items;
    size_t count;
} ParameterArray;

typedef enum {
    DECL_VARIABLE,
    DECL_FUNCTION,
    DECL_STRUCT,
    DECL_TYPE_ALIAS,
    DECL_ENUM,
    DECL_STRUCT_SCOPE,
    DECL_STATIC_ASSERT
} DeclKind;

typedef struct {
    char *name;
    size_t name_length;
    SourceLocation location;
    Expr *value;
} Enumerator;

typedef enum {
    INIT_NONE,
    INIT_EXPRESSION
} InitializerKind;

typedef struct {
    Type *type;
    Expr *value;
    size_t offset;
    size_t size;
    bool clear;
    bool bit_field;
    unsigned bit_offset;
    unsigned bit_width;
} InitializerOperation;

struct InitializerPlan {
    InitializerOperation *items;
    size_t count;
};

typedef struct {
    InitializerKind kind;
    Expr *expression;
    InitializerPlan *semantic_plan; /* Borrowed from the semantic model. */
} Initializer;

struct Decl {
    DeclKind kind;
    SourceLocation location;
    size_t declarator_offset;
    bool is_static;
    bool is_extern;
    bool is_thread_local;
    bool semantic_global_definition;
    bool semantic_internal_linkage;
    AlignmentSpecifiers *alignment_specifiers;
    size_t semantic_alignment;

    union {
        struct {
            Type *type;
            char *name;
            size_t name_length;
            Initializer initializer;

            bool anonymous_member;
            Expr *array_size; /* Optional fixed-array bound expression. */
            size_t semantic_initial_array_length;
            bool semantic_array_length_recorded;
            bool semantic_vla;
            bool semantic_vla_pointer;
            unsigned semantic_vla_id;
            bool is_bit_field;
            Expr *bit_field_width;
            unsigned resolved_bit_field_width;
            bool bit_field_width_resolved;
        } variable;

        struct {
            Type *return_type;
            char *name;
            size_t name_length;
            ParameterArray parameters;
            bool noreturn;
            bool is_inline;
            bool semantic_inline_only;
            bool variadic;
            bool runtime_referenced;
            size_t label_count; /* Populated by semantic analysis. */
            bool semantic_has_vla;
            bool semantic_has_alloca;
            Stmt *body; /* NULL for a prototype. */
        } function;

        struct {
            char *name;
            size_t name_length;
            bool anonymous;
            bool is_local;
            bool is_union;
            unsigned pack_alignment;
            size_t completion_offset;
            Decl **fields; /* Variable declarations only. */
            size_t field_count;
            Decl **assertions;
            size_t assertion_count;
        } structure;

        struct {
            Type *type;
            char *name;
            size_t name_length;
        } type_alias;

        struct {
            char *tag;
            size_t identity;
            Enumerator *items;
            size_t count;
            bool is_unsigned; /* Set by semantic analysis. */
        } enumeration;

        struct {
            Expr *condition;
            Expr *message;
            size_t preceding_fields;
        } static_assertion;
    } as;
};

/**
 * @brief Returns the integer type compatible with an enum, or the original kind.
 * @param type Type to inspect.
 * @return The effective scalar kind.
 */
static inline TypeKind type_effective_kind(const Type *type)
{
    if (type->kind != TYPE_ENUM)
        return type->kind;
    return type->enum_definition && type->enum_definition->as.enumeration.is_unsigned
               ? TYPE_UNSIGNED_INT : TYPE_INT;
}

typedef struct {
    Decl **items;
    size_t count;
} DeclArray;

struct AstProgram {
    DeclArray declarations;
    DeclArray block_externals; /* Borrowed declarations, populated by semantic analysis. */
    bool contains_unsigned; /* Selects the unsigned-capable IR version. */
    SourceLocation unsigned_location;
    bool contains_float;
    bool contains_float_ir; /* Excludes unused runtime prototypes. */
    SourceLocation float_location;
    bool contains_fixed_array;
    SourceLocation fixed_array_location;
    bool contains_static_local;
    bool contains_thread_local;
    bool contains_short;
    bool contains_character_types;
    bool contains_long_suffix;
    bool contains_long_long;
    bool contains_bool;
    bool contains_enum;
    bool contains_embedded_enum;
    bool contains_designator;
    bool contains_named_labels;
    bool contains_union;
    bool contains_function_types;
    bool contains_variadic;
    bool contains_va_start;
    bool contains_va_aggregate;
    bool contains_array_typedef;
    bool contains_nested_array;
    bool contains_static_assert;
    bool contains_static_assert_without_message;
    bool contains_wide_char;
    bool contains_wide_string;
    bool contains_variable_hex_escape;
    bool contains_external_inline;
    bool contains_pointer_to_integer;
    bool contains_volatile;
    bool contains_bit_field;
    bool contains_alignof;
    bool contains_offsetof;
    bool contains_alignas;
    bool contains_pack;
    bool contains_vla;
    bool contains_alloca;
    bool contains_array_compound_literal;
    bool contains_scalar_compound_literal;
    bool contains_file_compound_literal;
    bool contains_long_double;
    bool contains_flexible_array;
    bool requires_long_double_ir;
    bool contains_pointer_difference;
    bool contains_dynamic_string_precision;
    bool contains_extended_fixed_precision;
    bool contains_runtime_dispatch;
    bool contains_do_while;
    bool contains_comma_expression;
    bool contains_unnamed_parameters;
    bool contains_c_format_integer;
    bool contains_formatted_output;
    bool c11_numeric; /* Set by sema_run; retain through IR generation. */
};

/* ---------- Statements ---------- */

typedef enum {
    STMT_BLOCK,
    STMT_DECLARATION,
    STMT_EXPRESSION,
    STMT_IF,
    STMT_SWITCH,
    STMT_FOR,
    STMT_LOOP,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RETURN,
    STMT_GOTO,
    STMT_LABEL,
    STMT_DO
} StmtKind;

typedef struct {
    Stmt **items;
    size_t count;
} StmtArray;

typedef struct {
    Expr *value; /* NULL for default. */
    SourceLocation location;
    StmtArray statements;
} SwitchCase;

typedef struct {
    SwitchCase *items;
    size_t count;
} SwitchCaseArray;

struct Stmt {
    StmtKind kind;
    SourceLocation location;

    union {
        struct {
            StmtArray statements;
        } block;

        struct {
            Decl *declaration;
        } declaration;

        struct {
            Expr *expression; /* NULL for an empty expression statement. */
        } expression;

        struct {
            Expr *condition;
            Stmt *then_branch;
            Stmt *else_branch; /* NULL if absent. */
        } if_stmt;

        struct {
            Expr *expression;
            StmtArray prefix;
            SwitchCaseArray cases;
        } switch_stmt;

        struct {
            Stmt *initializer; /* NULL if absent. */
            Expr *condition;   /* NULL means no condition. */
            Expr *update;      /* NULL if absent. */
            Stmt *body;
        } for_stmt;

        struct {
            Expr *condition;
            Stmt *body;
        } loop_stmt;

        struct {
            Expr *expression; /* NULL for return; */
        } return_stmt;

        struct {
            char *name;
            size_t id;
            Stmt *statement; /* Only labels own a nested statement. */
        } label;

    } as;
};

/**
 * @brief Releases an AST program and all of its owned declarations.
 * @param program AST program to destroy.
 * @return No value.
 */
void ast_program_destroy(AstProgram *program);

#endif
