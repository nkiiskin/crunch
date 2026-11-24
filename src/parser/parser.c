#include "parser/parser.h"
#include "common/runtime_routines.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ParserName ParserName;
typedef struct ParserTag ParserTag;
struct ParserTag {
    const Token *token;
    size_t identity;
    unsigned depth;
    bool complete;
    bool is_struct;
    bool is_union;
    char *structure_name;
    Decl *enum_definition;
    ParserTag *next;
};
struct ParserName {
    const Token *token;
    Type *alias_type;
    ParserName *next;
};

/* Recursive-descent statements/declarations and a precedence-climbing expression parser. */
typedef struct {
    const TokenArray *tokens;
    size_t current;
    unsigned errors;
    SourceLocation *error_locations;
    size_t error_capacity;
    size_t error_count;
    unsigned recursion_depth;
    bool recursion_limit_reported;
    bool out_of_memory;
    bool contains_fixed_array;
    SourceLocation fixed_array_location;
    bool contains_static_local;
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
    bool contains_array_typedef;
    bool contains_builtin_va_list;
    bool contains_nested_array;
    bool contains_static_assert;
    bool contains_static_assert_without_message;
    bool contains_wide_char;
    bool contains_wide_string;
    bool contains_external_inline;
    bool contains_volatile;
    bool contains_bit_field;
    bool contains_alignof;
    bool contains_offsetof;
    bool contains_alignas;
    bool contains_pack;
    bool contains_thread_local;
    AlignmentSpecifiers *alignment_specifiers;
    bool allow_auto;
    bool allow_inline_aggregate_type;
    bool contains_array_compound_literal;
    bool contains_scalar_compound_literal;
    bool contains_long_double;
    bool contains_do_while;
    bool contains_comma_expression;
    bool contains_unnamed_parameters;
    bool allow_enum_definition;
    bool in_for_initializer;
    bool function_body_scope;
    unsigned scope_depth;
    ParserName *names;
    ParserTag *tags;
    DeclArray pending_enums;
    DeclArray hidden_structures;
} Parser;

enum { PARSER_MAX_RECURSION_DEPTH = 128, PARSER_MAX_TYPE_INDIRECTION = 128 };

static void free_type(Type *t);
static void free_expr(Expr *e);
static void free_stmt(Stmt *s);
static void free_decl(Decl *d);
static Type *clone_type(Parser *p, const Type *source);
static Type *parse_enum_type(Parser *p);
static void restore_tags(Parser *p, ParserTag *marker);
static ParserTag *find_tag(Parser *p, const Token *token);
static ParserTag *structure_tag(Parser *p, const Token *token, bool definition,
                                bool standalone, bool is_union);
static Type *parse_parenthesized_pointer(Parser *p, Type *result, const Token **name);
static bool starts_parenthesized_declarator(Parser *p);
static Type *parse_complex_declarator(Parser *p, Type *base, const Token **name);
static bool starts_abstract_function(Parser *p);
static Type *parse_abstract_function(Parser *p, Type *result);
static void parse_alignment_specifier(Parser *p);
static Decl *parse_struct_definition(Parser *p, bool semicolon);

/**
 * @brief Allocates memory for parser-owned data and records allocation failure.
 * @param p Parser state that receives the allocation-failure flag.
 * @param size Number of bytes to allocate.
 * @return Allocated memory, or NULL if allocation fails.
 */
static void *parser_alloc(Parser *p, size_t size)
{
    void *result = p->out_of_memory ? NULL : malloc(size);
    if (!result)
        p->out_of_memory = true;
    return result;
}

/**
 * @brief Grows a parser-owned array to hold one additional item.
 * @param p Parser state that receives the allocation-failure flag.
 * @param items Existing array allocation, or NULL.
 * @param count Number of elements currently in the array.
 * @param size Size in bytes of each element.
 * @return Resized array with room for count plus one, or NULL on failure.
 */
static void *resize_array(Parser *p, void *items, size_t count, size_t size)
{
    if (p->out_of_memory || count == SIZE_MAX || count + 1 > SIZE_MAX / size) {
        p->out_of_memory = true;
        return NULL;
    }
    void *result = realloc(items, (count + 1) * size);
    if (!result)
        p->out_of_memory = true;
    return result;
}

/**
 * @brief Returns the current token without advancing.
 * @param p Parser whose current token is returned.
 * @return Current token; the token array is required to include an EOF sentinel.
 */
static const Token *peek(Parser *p)
{
    return &p->tokens->items[p->current];
}
/**
 * @brief Returns the most recently consumed token.
 * @param p Parser whose last consumed token is returned.
 * @return Most recently consumed token, or the first token if none has been consumed.
 */
static const Token *previous(Parser *p)
{
    return &p->tokens->items[p->current ? p->current - 1 : 0];
}
/**
 * @brief Checks whether the current token has the requested kind.
 * @param p Parser whose current token is tested.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool at(Parser *p, TokenKind k)
{
    return peek(p)->kind == k;
}
/**
 * @brief Consumes the current token only when its kind matches.
 * @param p Parser whose token is conditionally consumed.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool match(Parser *p, TokenKind k)
{
    if (!at(p, k))
        return false;
    if (p->current + 1 < p->tokens->count)
        ++p->current;
    return true;
}
/**
 * @brief Consumes three adjacent dot tokens forming an ellipsis.
 * @param p Parser whose current tokens are inspected.
 * @return True when an ellipsis was consumed.
 */
static bool match_ellipsis(Parser *p)
{
    if (p->current + 2 >= p->tokens->count)
        return false;
    const Token *a = &p->tokens->items[p->current];
    const Token *b = &p->tokens->items[p->current + 1];
    const Token *c = &p->tokens->items[p->current + 2];
    if (a->kind != TOKEN_DOT || b->kind != TOKEN_DOT || c->kind != TOKEN_DOT ||
        a->length != 1 || b->length != 1 || c->length != 1 ||
        a->location.offset > SIZE_MAX - 2 ||
        b->location.offset != a->location.offset + 1 ||
        c->location.offset != a->location.offset + 2)
        return false;
    p->current += 3;
    return true;
}
/**
 * @brief Records a parser error at the current token.
 * @param p Parser whose current location is recorded as an error.
 * @return No value; the function performs the documented output or state update.
 */
static void error(Parser *p)
{
    SourceLocation location = peek(p)->location;
    if (p->error_count < p->error_capacity && p->error_locations)
        p->error_locations[p->error_count++] = location;
    else
        ++p->error_count;
    ++p->errors;
}
/**
 * @brief Requires and consumes a token of the requested kind.
 * @param p Parser whose requested token is required.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool expect(Parser *p, TokenKind k)
{
    if (match(p, k))
        return true;
    error(p);
    return false;
}
/**
 * @brief Allocates zero-initialized parser-owned storage.
 * @param p Parser state recording allocation failure.
 * @param n Number of bytes to allocate.
 * @return Zero-initialized allocation of n bytes, or NULL if allocation fails.
 */
static void *zalloc(Parser *p, size_t n)
{
    void *result = p->out_of_memory ? NULL : calloc(1, n);
    if (!result)
        p->out_of_memory = true;
    return result;
}
/**
 * @brief Allocates a NUL-terminated copy of a token lexeme.
 * @param p Parser state recording allocation failure.
 * @param t Token record being inspected.
 * @return Allocated NUL-terminated copy of the token lexeme, or NULL if allocation fails.
 */
static char *copy_lexeme(Parser *p, const Token *t)
{
    if (t->length == SIZE_MAX) {
        p->out_of_memory = true;
        return NULL;
    }
    char *s = parser_alloc(p, t->length + 1);
    if (!s)
        return NULL;
    memcpy(s, t->lexeme, t->length);
    s[t->length] = '\0';
    return s;
}
/**
 * @brief Allocates and initializes an expression node.
 * @param p Parser state recording allocation failure.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @param loc Source location associated with the token or diagnostic.
 * @return Allocated expression node, or NULL if allocation fails.
 */
static Expr *expr_new(Parser *p, ExprKind k, SourceLocation loc)
{
    Expr *e = (Expr *)zalloc(p, sizeof(*e));
    if (e) {
        e->kind = k;
        e->location = loc;
    }
    return e;
}
/**
 * @brief Allocates and initializes a statement node.
 * @param p Parser state recording allocation failure.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @param loc Source location associated with the token or diagnostic.
 * @return Allocated statement node, or NULL if allocation fails.
 */
static Stmt *stmt_new(Parser *p, StmtKind k, SourceLocation loc)
{
    Stmt *s = (Stmt *)zalloc(p, sizeof(*s));
    if (s) {
        s->kind = k;
        s->location = loc;
    }
    return s;
}
/**
 * @brief Allocates and initializes a declaration node.
 * @param p Parser state recording allocation failure.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @param loc Source location associated with the token or diagnostic.
 * @return Allocated declaration node, or NULL if allocation fails.
 */
static Decl *decl_new(Parser *p, DeclKind k, SourceLocation loc)
{
    Decl *d = (Decl *)zalloc(p, sizeof(*d));
    if (d) {
        d->kind = k;
        d->location = loc;
    }
    return d;
}
/**
 * @brief Allocates and initializes a type node.
 * @param p Parser state recording allocation failure.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @param pointee Owned pointee, consumed even when allocation fails.
 * @return Allocated type node, or NULL if allocation fails.
 */
static Type *type_new(Parser *p, TypeKind k, Type *pointee)
{
    Type *t = (Type *)zalloc(p, sizeof(*t));
    if (t) {
        t->kind = k;
        t->pointee = pointee;
    } else
        free_type(pointee);
    return t;
}
/**
 * @brief Appends an item to a dynamically grown array.
 * @param p Parser state recording allocation failure.
 * @param items Dynamic-array storage being resized.
 * @param count Output parameter receiving the count value.
 * @param size Capacity or size in bytes.
 * @param item Item to append to the collection.
 * @return True when the item is appended; false if reallocation fails.
 */
static bool grow(Parser *p, void **items, size_t *count, size_t size, void *item)
{
    void *n = resize_array(p, *items, *count, size);
    if (!n)
        return false;
    *items = n;
    memcpy((char *)n + (*count) * size, &item, size);
    ++*count;
    return true;
}
/**
 * @brief Appends an expression to an expression array.
 * @param p Parser state recording allocation failure.
 * @param a Expression array receiving the new item.
 * @param e Owned expression, destroyed if appending fails.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool add_expr(Parser *p, ExprArray *a, Expr *e)
{
    if (grow(p, (void **)&a->items, &a->count, sizeof(e), e))
        return true;
    free_expr(e);
    return false;
}
/**
 * @brief Appends a statement to a statement array.
 * @param p Parser state recording allocation failure.
 * @param a Statement array receiving the new item.
 * @param s Owned statement, destroyed if appending fails.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool add_stmt(Parser *p, StmtArray *a, Stmt *s)
{
    if (grow(p, (void **)&a->items, &a->count, sizeof(s), s))
        return true;
    free_stmt(s);
    return false;
}
/**
 * @brief Appends a declaration to a declaration array.
 * @param p Parser state recording allocation failure.
 * @param a Declaration array receiving the new item.
 * @param d Owned declaration, destroyed if appending fails.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool add_decl(Parser *p, DeclArray *a, Decl *d)
{
    if (grow(p, (void **)&a->items, &a->count, sizeof(d), d))
        return true;
    free_decl(d);
    return false;
}

/**
 * @brief Checks whether a token can begin a type.
 * @param p Parser state containing visible names.
 * @param token Token to inspect.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool type_start(Parser *p, const Token *token)
{
    TokenKind k = token->kind;
    if (k == TOKEN_IDENTIFIER) {
        for (ParserName *n = p->names; n; n = n->next)
            if (n->token->length == token->length &&
                !memcmp(n->token->lexeme, token->lexeme, token->length))
                return n->alias_type != NULL;
        if (token->length == 17 && !memcmp(token->lexeme, "__builtin_va_list", 17))
            return true;
    }
    return k == TOKEN_VOID || k == TOKEN_BOOL || k == TOKEN_ENUM || k == TOKEN_INT || k == TOKEN_SHORT || k == TOKEN_LONG || k == TOKEN_SIGNED || k == TOKEN_UNSIGNED || k == TOKEN_BYTE ||
           k == TOKEN_REAL || k == TOKEN_FLOAT || k == TOKEN_STRUCT || k == TOKEN_UNION || k == TOKEN_CONST ||
           k == TOKEN_VOLATILE || k == TOKEN_RESTRICT;
}

/**
 * @brief Builds the System V AMD64 type used by GCC's builtin va_list.
 * @param p Parser owning the generated structure declaration.
 * @param location Location of the builtin type reference.
 * @return Owned array type, or NULL on allocation failure.
 */
static Type *builtin_va_list_type(Parser *p, SourceLocation location)
{
    static const char tag[] = "$builtin_va_list";
    static const char *names[] = {"gp_offset", "fp_offset", "overflow_arg_area",
                                  "reg_save_area"};
    if (!p->contains_builtin_va_list) {
        Decl *structure = decl_new(p, DECL_STRUCT, location);
        if (!structure)
            return NULL;
        Token tag_token = {0};
        tag_token.lexeme = tag;
        tag_token.length = sizeof tag - 1;
        structure->as.structure.name = copy_lexeme(p, &tag_token);
        structure->as.structure.name_length = sizeof tag - 1;
        for (size_t i = 0; i < 4 && !p->out_of_memory; ++i) {
            Decl *field = decl_new(p, DECL_VARIABLE, location);
            if (!field)
                break;
            Token field_token = {0};
            field_token.lexeme = names[i];
            field_token.length = strlen(names[i]);
            field->as.variable.name = copy_lexeme(p, &field_token);
            field->as.variable.name_length = field_token.length;
            Type *type = type_new(p, i < 2 ? TYPE_UNSIGNED_INT : TYPE_VOID, NULL);
            if (i >= 2 && type)
                type = type_new(p, TYPE_POINTER, type);
            field->as.variable.type = type;
            if (p->out_of_memory ||
                !grow(p, (void **)&structure->as.structure.fields,
                      &structure->as.structure.field_count, sizeof(field), field)) {
                free_decl(field);
                break;
            }
        }
        if (p->out_of_memory) {
            free_decl(structure);
            return NULL;
        }
        if (!add_decl(p, &p->hidden_structures, structure))
            return NULL;
        p->contains_builtin_va_list = true;
    }
    Type *element = type_new(p, TYPE_STRUCT, NULL);
    if (!element)
        return NULL;
    Token tag_token = {0};
    tag_token.lexeme = tag;
    tag_token.length = sizeof tag - 1;
    element->struct_name = copy_lexeme(p, &tag_token);
    if (!element->struct_name) {
        free_type(element);
        return NULL;
    }
    Type *array = type_new(p, TYPE_ARRAY, element);
    if (array) {
        array->array_length = 1;
        array->array_length_from_typedef = true;
        p->contains_array_typedef = true;
    }
    return array;
}

/**
 * @brief Consumes type qualifiers and supported declaration storage specifiers.
 * @param p Parser state whose cursor is advanced.
 * @param allow_register Whether a register storage hint is permitted.
 * @return Bit mask of const and volatile qualifiers.
 */
static unsigned consume_qualifiers(Parser *p, bool allow_register)
{
    unsigned qualifiers = 0;
    while (at(p, TOKEN_CONST) || at(p, TOKEN_VOLATILE) || at(p, TOKEN_RESTRICT) ||
           (p->alignment_specifiers && at(p, TOKEN_ALIGNAS)) ||
           (allow_register && (at(p, TOKEN_REGISTER) ||
                               (p->allow_auto && p->alignment_specifiers && at(p, TOKEN_AUTO))))) {
        if (at(p, TOKEN_ALIGNAS))
            parse_alignment_specifier(p);
        else if (match(p, TOKEN_CONST))
            qualifiers |= 1;
        else if (match(p, TOKEN_VOLATILE)) {
            qualifiers |= 2;
            p->contains_volatile = true;
        }
        else if (match(p, TOKEN_RESTRICT))
            continue;
        else if (match(p, TOKEN_AUTO)) {
            if (p->alignment_specifiers->auto_storage)
                error(p);
            p->alignment_specifiers->auto_storage = true;
        } else {
            if (p->alignment_specifiers)
                p->alignment_specifiers->register_storage = true;
            ++p->current;
        }
    }
    return qualifiers;
}

/**
 * @brief Adds parsed qualifiers to an owned type.
 * @param type Type to qualify, or NULL after allocation failure.
 * @param qualifiers Const and volatile qualifier bits.
 * @return No value.
 */
static void apply_qualifiers(Type *type, unsigned qualifiers)
{
    if (type && type->kind == TYPE_ARRAY) {
        Type *element = type;
        while (element && element->kind == TYPE_ARRAY)
            element = element->pointee;
        if ((qualifiers & 2) || (element && element->is_volatile))
            type = element;
    }
    if (type) {
        type->is_const |= (qualifiers & 1) != 0;
        type->is_volatile |= (qualifiers & 2) != 0;
    }
}
/**
 * @brief Parses pointer modifiers following an already parsed base type.
 * @param p Parser state whose cursor is advanced.
 * @param t Owned base type.
 * @return Completed type, or NULL on allocation failure.
 */
static Type *parse_pointer_suffix(Parser *p, Type *t)
{
    if (!t)
        return NULL;
    unsigned indirection = 0;
    for (const Type *part = t; part && part->kind == TYPE_POINTER; part = part->pointee)
        ++indirection;
    while (match(p, TOKEN_STAR)) {
        if (indirection == PARSER_MAX_TYPE_INDIRECTION) {
            if (!p->recursion_limit_reported) {
                error(p);
                p->recursion_limit_reported = true;
            }
            continue;
        }
        t = type_new(p, TYPE_POINTER, t);
        if (!t)
            return NULL;
        unsigned qualifiers = consume_qualifiers(p, false);
        apply_qualifiers(t, qualifiers);
        ++indirection;
    }
    return t;
}

/**
 * @brief Parses a type and optionally identifies its base before new pointer modifiers.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @param base Receives a borrowed base node on success or NULL on allocation failure.
 * @param allow_register Whether a register storage hint is permitted.
 * @return Parsed type node, or NULL if allocation fails.
 */
static Type *parse_type_with_base(Parser *p, Type **base, bool allow_register)
{
    TypeKind k;
    unsigned qualifiers = consume_qualifiers(p, allow_register);
    Type *t;
    if (at(p, TOKEN_IDENTIFIER) && type_start(p, peek(p))) {
        const Token *name = peek(p);
        ParserName *n = p->names;
        while (n && (n->token->length != name->length ||
                     memcmp(n->token->lexeme, name->lexeme, name->length)))
            n = n->next;
        ++p->current;
        t = n ? clone_type(p, n->alias_type) : builtin_va_list_type(p, name->location);
    } else if (at(p, TOKEN_ENUM)) {
        t = parse_enum_type(p);
    } else if (p->allow_inline_aggregate_type &&
               (at(p, TOKEN_STRUCT) || at(p, TOKEN_UNION)) &&
               p->current + 1 < p->tokens->count &&
               (p->tokens->items[p->current + 1].kind == TOKEN_LBRACE ||
                (p->current + 2 < p->tokens->count &&
                 p->tokens->items[p->current + 1].kind == TOKEN_IDENTIFIER &&
                 p->tokens->items[p->current + 2].kind == TOKEN_LBRACE))) {
        Decl *structure = parse_struct_definition(p, false);
        t = type_new(p, TYPE_STRUCT, NULL);
        if (t && structure && structure->as.structure.name) {
            size_t length = structure->as.structure.name_length;
            t->is_union = structure->as.structure.is_union;
            t->struct_name = parser_alloc(p, length + 1);
            if (t->struct_name)
                memcpy(t->struct_name, structure->as.structure.name, length + 1);
        }
        if (structure && !add_decl(p, &p->hidden_structures, structure))
            error(p);
    } else if (at(p, TOKEN_STRUCT) || at(p, TOKEN_UNION)) {
        bool is_union = match(p, TOKEN_UNION);
        if (!is_union)
            expect(p, TOKEN_STRUCT);
        p->contains_union |= is_union;
        const Token *name = peek(p);
        expect(p, TOKEN_IDENTIFIER);
        ParserTag *tag = structure_tag(p, name, false, false, is_union);
        t = type_new(p, TYPE_STRUCT, NULL);
        if (t)
            t->is_union = is_union;
        if (t && tag && tag->structure_name) {
            size_t length = strlen(tag->structure_name);
            t->struct_name = parser_alloc(p, length + 1);
            if (t->struct_name)
                memcpy(t->struct_name, tag->structure_name, length + 1);
        }
    } else {
        if (match(p, TOKEN_VOID))
            k = TYPE_VOID;
        else if (match(p, TOKEN_BOOL)) {
            k = TYPE_BOOL;
            p->contains_bool = true;
        }
        else if (at(p, TOKEN_BYTE) || at(p, TOKEN_INT) || at(p, TOKEN_SHORT) || at(p, TOKEN_LONG) ||
                 at(p, TOKEN_REAL) ||
                 at(p, TOKEN_SIGNED) || at(p, TOKEN_UNSIGNED)) {
            unsigned chars = 0, ints = 0, shorts = 0, longs = 0, signs = 0, unsigneds = 0, reals = 0;
            while (at(p, TOKEN_BYTE) || at(p, TOKEN_INT) || at(p, TOKEN_SHORT) || at(p, TOKEN_LONG) ||
                   at(p, TOKEN_REAL) || at(p, TOKEN_SIGNED) || at(p, TOKEN_UNSIGNED) || at(p, TOKEN_CONST) ||
                   at(p, TOKEN_VOLATILE) || at(p, TOKEN_RESTRICT) ||
                   (p->alignment_specifiers && at(p, TOKEN_ALIGNAS)) ||
                   (allow_register && (at(p, TOKEN_REGISTER) ||
                                       (p->allow_auto && p->alignment_specifiers &&
                                        at(p, TOKEN_AUTO))))) {
                if (match(p, TOKEN_BYTE)) ++chars;
                else if (match(p, TOKEN_INT)) ++ints;
                else if (match(p, TOKEN_SHORT)) ++shorts;
                else if (match(p, TOKEN_LONG)) ++longs;
                else if (match(p, TOKEN_REAL)) ++reals;
                else if (match(p, TOKEN_SIGNED)) ++signs;
                else if (match(p, TOKEN_UNSIGNED)) ++unsigneds;
                else qualifiers |= consume_qualifiers(p, allow_register);
            }
            if (chars > 1 || ints > 1 || shorts > 1 || longs > 2 || signs > 1 ||
                unsigneds > 1 || (signs && unsigneds) || (shorts && longs) ||
                (chars && (ints || shorts || longs))) {
                error(p);
                return type_new(p, TYPE_VOID, NULL);
            }
            if (reals) {
                if (reals != 1 || chars || ints || shorts || longs > 1 || signs || unsigneds) {
                    error(p);
                    return type_new(p, TYPE_VOID, NULL);
                }
                k = longs ? TYPE_LONG_DOUBLE : TYPE_REAL;
                if (longs)
                    p->contains_long_double = true;
            } else {
                k = chars ? (unsigneds ? TYPE_UNSIGNED_CHAR : signs ? TYPE_SIGNED_CHAR : TYPE_BYTE)
                    : shorts ? (unsigneds ? TYPE_UNSIGNED_SHORT : TYPE_SHORT)
                    : longs == 2 ? (unsigneds ? TYPE_UNSIGNED_LONG_LONG : TYPE_LONG_LONG)
                    : longs ? (unsigneds ? TYPE_UNSIGNED_LONG : TYPE_LONG)
                    : unsigneds ? TYPE_UNSIGNED_INT : TYPE_INT;
                if (longs == 2)
                    p->contains_long_long = true;
                if (k == TYPE_SIGNED_CHAR || k == TYPE_UNSIGNED_CHAR)
                    p->contains_character_types = true;
            }
        }
        else if (match(p, TOKEN_FLOAT))
            k = TYPE_FLOAT;
        else {
            error(p);
            return type_new(p, TYPE_VOID, NULL);
        }
        t = type_new(p, k, NULL);
    }
    qualifiers |= consume_qualifiers(p, allow_register);
    apply_qualifiers(t, qualifiers);
    Type *result = parse_pointer_suffix(p, t);
    if (base)
        *base = result ? t : NULL;
    return result;
}

/**
 * @brief Parses a scalar, structure, or pointer type.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Parsed type node, or NULL if allocation fails.
 */
static Type *parse_type(Parser *p)
{
    return parse_type_with_base(p, NULL, false);
}
static void free_type(Type *t);
/**
 * @brief Creates a deep copy of a type tree.
 * @param p Parser state recording allocation failure.
 * @param source Crunch type node being inspected or emitted.
 * @return Deep copy of the type tree, or NULL if the input is NULL or allocation fails.
 */
static Type *clone_type(Parser *p, const Type *source)
{
    if (!source)
        return NULL;
    Type *copy = type_new(p, source->kind, NULL);
    if (!copy)
        return NULL;
    copy->is_const = source->is_const;
    copy->is_volatile = source->is_volatile;
    copy->array_length = source->array_length;
    copy->array_length_from_typedef = source->array_length_from_typedef;
    copy->is_zero_length_array = source->is_zero_length_array;
    copy->array_bound = source->array_bound;
    if (copy->array_bound)
        ++copy->array_bound->references;
    copy->enum_definition = source->enum_definition;
    copy->is_union = source->is_union;
    if (source->function_signature) {
        const Decl *old = source->function_signature;
        Decl *signature = decl_new(p, DECL_FUNCTION, old->location);
        if (!signature) {
            free_type(copy);
            return NULL;
        }
        copy->function_signature = signature;
        signature->as.function.return_type = clone_type(p, old->as.function.return_type);
        signature->as.function.variadic = old->as.function.variadic;
        for (size_t i = 0; i < old->as.function.parameters.count; ++i) {
            Parameter param = {0};
            param.type = clone_type(p, old->as.function.parameters.items[i].type);
            if (!param.type) {
                free_type(copy);
                return NULL;
            }
            Parameter *items = resize_array(p, signature->as.function.parameters.items,
                                            signature->as.function.parameters.count, sizeof(param));
            if (!items) {
                free_type(param.type);
                free_type(copy);
                return NULL;
            }
            signature->as.function.parameters.items = items;
            items[signature->as.function.parameters.count++] = param;
        }
        if (!signature->as.function.return_type) {
            free_type(copy);
            return NULL;
        }
    }
    if (source->pointee) {
        copy->pointee = clone_type(p, source->pointee);
        if (!copy->pointee) {
            free_type(copy);
            return NULL;
        }
    }
    if (copy && source->struct_name) {
        size_t length = strlen(source->struct_name);
        copy->struct_name = parser_alloc(p, length + 1);
        if (copy->struct_name)
            memcpy(copy->struct_name, source->struct_name, length + 1);
    }
    if (source->struct_name && !copy->struct_name) {
        free_type(copy);
        return NULL;
    }
    return copy;
}

/**
 * @brief Records an ordinary identifier or type alias for parser disambiguation.
 * @param p Parser state recording the name.
 * @param token Identifier token, borrowed from the input.
 * @param type Alias type, or NULL for an ordinary identifier.
 * @return True on success, false on allocation failure.
 */
static bool register_name(Parser *p, const Token *token, const Type *type)
{
    ParserName *name = zalloc(p, sizeof(*name));
    if (!name)
        return false;
    name->token = token;
    if (type) {
        name->alias_type = clone_type(p, type);
        if (!name->alias_type) {
            free(name);
            return false;
        }
    }
    name->next = p->names;
    p->names = name;
    return true;
}

/**
 * @brief Discards names introduced after a lexical-scope marker.
 * @param p Parser state owning the names.
 * @param marker Previous name-list head, or NULL to discard all names.
 * @return No value.
 */
static void restore_names(Parser *p, ParserName *marker)
{
    while (p->names != marker) {
        ParserName *name = p->names;
        p->names = name->next;
        free_type(name->alias_type);
        free(name);
    }
}

static Expr *parse_expression(Parser *p, int min_prec);
static Type *parse_array_suffixes(Parser *p, Type *base, Expr **outer, bool alias);
static Expr *parse_postfix(Parser *p);
static Expr *parse_primary(Parser *p);
static Stmt *parse_statement(Parser *p);
static void free_stmt(Stmt *s);
static void free_expr(Expr *e);
static Decl *parse_declaration(Parser *p, bool global, DeclArray *additional);

/**
 * @brief Discards enum tags introduced after a scope marker.
 * @param p Parser owning the tag records.
 * @param marker Previous tag-list head.
 * @return No value.
 */
static void restore_tags(Parser *p, ParserTag *marker)
{
    while (p->tags != marker) {
        ParserTag *tag = p->tags;
        p->tags = tag->next;
        free(tag->structure_name);
        free(tag);
    }
}

/**
 * @brief Finds the nearest tag with the given spelling.
 * @param p Parser containing visible tags.
 * @param token Tag identifier.
 * @return Matching tag, or NULL if none is visible.
 */
static ParserTag *find_tag(Parser *p, const Token *token)
{
    if (!token)
        return NULL;
    for (ParserTag *tag = p->tags; tag; tag = tag->next)
        if (tag->token->length == token->length &&
            !memcmp(tag->token->lexeme, token->lexeme, token->length))
            return tag;
    return NULL;
}

/**
 * @brief Resolves or introduces a structure tag in the shared tag namespace.
 * @param p Parser containing visible tags.
 * @param token Structure tag identifier.
 * @param definition Whether this is a structure definition.
 * @param standalone Whether this declares a tag in the current scope.
 * @param is_union Whether the tag names a union.
 * @return Matching tag, or NULL on allocation failure.
 */
static ParserTag *structure_tag(Parser *p, const Token *token, bool definition,
                                bool standalone, bool is_union)
{
    ParserTag *tag = find_tag(p, token);
    if (tag && (!(definition || standalone) || tag->depth == p->scope_depth)) {
        if (!tag->is_struct || tag->is_union != is_union)
            error(p);
        if (definition && tag->is_struct)
            tag->complete = true;
        return tag;
    }
    tag = zalloc(p, sizeof(*tag));
    if (!tag)
        return NULL;
    tag->token = token;
    tag->depth = p->scope_depth;
    tag->is_struct = true;
    tag->is_union = is_union;
    tag->complete = definition;
    if (p->scope_depth) {
        char name[64];
        int length = snprintf(name, sizeof name, "$local_struct_%zu", p->current);
        if (length < 0 || (size_t)length >= sizeof name) {
            error(p);
            free(tag);
            return NULL;
        }
        tag->structure_name = parser_alloc(p, (size_t)length + 1);
        if (tag->structure_name)
            memcpy(tag->structure_name, name, (size_t)length + 1);
    } else
        tag->structure_name = copy_lexeme(p, token);
    if (!tag->structure_name) {
        free(tag);
        return NULL;
    }
    tag->next = p->tags;
    p->tags = tag;
    return tag;
}

/**
 * @brief Parses an enum reference or a declaration's enum definition.
 * @param p Parser owning the tokens and pending declarations.
 * @return Owned enum type, or NULL on allocation failure.
 */
static Type *parse_enum_type(Parser *p)
{
    const Token *start = peek(p);
    size_t identity = p->current + 1;
    expect(p, TOKEN_ENUM);
    const Token *tag = at(p, TOKEN_IDENTIFIER) ? peek(p) : NULL;
    if (tag)
        match(p, TOKEN_IDENTIFIER);
    bool definition = at(p, TOKEN_LBRACE);
    ParserTag *entry = NULL;
    Type *type = type_new(p, TYPE_ENUM, NULL);
    if (!type)
        return NULL;
    p->contains_enum = true;
    if (tag)
        type->struct_name = copy_lexeme(p, tag);
    if (!definition) {
        ParserTag *found = find_tag(p, tag);
        if (!found || found->is_struct || !found->complete)
            error(p);
        else {
            type->array_length = found->identity;
            type->enum_definition = found->enum_definition;
        }
        return type;
    }
    if (!p->allow_enum_definition) {
        error(p);
        return type;
    }
    if (p->in_for_initializer)
        error(p);
    if (tag) {
        ParserTag *prior = find_tag(p, tag);
        if (prior && prior->depth == p->scope_depth)
            error(p);
        entry = zalloc(p, sizeof(*entry));
        if (!entry) {
            free_type(type);
            return NULL;
        }
        entry->token = tag;
        entry->identity = identity;
        entry->depth = p->scope_depth;
        entry->next = p->tags;
        p->tags = entry;
    }
    type->array_length = identity;
    Decl *decl = decl_new(p, DECL_ENUM, start->location);
    if (!decl) {
        free_type(type);
        return NULL;
    }
    decl->as.enumeration.tag = tag ? copy_lexeme(p, tag) : NULL;
    decl->as.enumeration.identity = identity;
    type->enum_definition = decl;
    if (entry)
        entry->enum_definition = decl;
    expect(p, TOKEN_LBRACE);
    bool old_allow = p->allow_enum_definition;
    p->allow_enum_definition = false;
    while (!p->out_of_memory && !at(p, TOKEN_RBRACE) && !at(p, TOKEN_EOF)) {
        const Token *name = peek(p);
        if (!expect(p, TOKEN_IDENTIFIER))
            break;
        Enumerator item = {0};
        item.name = copy_lexeme(p, name);
        item.name_length = name->length;
        item.location = name->location;
        if (match(p, TOKEN_ASSIGN))
            item.value = parse_expression(p, 2);
        register_name(p, name, NULL);
        Enumerator *items = resize_array(p, decl->as.enumeration.items,
                                         decl->as.enumeration.count, sizeof(item));
        if (!items) {
            free(item.name);
            free_expr(item.value);
            break;
        }
        decl->as.enumeration.items = items;
        items[decl->as.enumeration.count++] = item;
        if (!match(p, TOKEN_COMMA))
            break;
    }
    p->allow_enum_definition = old_allow;
    if (!decl->as.enumeration.count)
        error(p);
    expect(p, TOKEN_RBRACE);
    if (entry)
        entry->complete = true;
    if (!add_decl(p, &p->pending_enums, decl))
        error(p);
    return type;
}

/**
 * @brief Parses a type name and retains any inline enum definition.
 * @param p Parser owning the token stream and pending declarations.
 * @param definition Receives an owned enum declaration or NULL.
 * @param allow_register Whether a register storage hint is permitted.
 * @return Owned parsed type.
 */
static Type *parse_embedded_type(Parser *p, Decl **definition, bool allow_register)
{
    size_t marker = p->pending_enums.count;
    bool allow = p->allow_enum_definition;
    p->allow_enum_definition = true;
    Type *type = allow_register ? parse_type_with_base(p, NULL, true) : parse_type(p);
    p->allow_enum_definition = allow;
    *definition = NULL;
    if (p->pending_enums.count > marker) {
        *definition = p->pending_enums.items[marker];
        for (size_t i = marker + 1; i < p->pending_enums.count; ++i)
            free_decl(p->pending_enums.items[i]);
        p->pending_enums.count = marker;
        p->contains_embedded_enum = true;
    }
    return type;
}

/**
 * @brief Parses an alignment specifier into the active declaration group.
 * @param p Parser containing the active specifier list.
 * @return No value; records syntax or allocation errors.
 */
static void parse_alignment_specifier(Parser *p)
{
    const Token *start = peek(p);
    AlignmentSpecifiers *specifiers = p->alignment_specifiers;
    match(p, TOKEN_ALIGNAS);
    expect(p, TOKEN_LPAREN);
    p->alignment_specifiers = NULL;
    Expr *expression;
    if (type_start(p, peek(p))) {
        Decl *definition = NULL;
        bool saved_inline_aggregate_type = p->allow_inline_aggregate_type;
        p->allow_inline_aggregate_type = true;
        Type *type = parse_embedded_type(p, &definition, false);
        p->allow_inline_aggregate_type = saved_inline_aggregate_type;
        if (starts_parenthesized_declarator(p)) {
            const Token *name;
            type = parse_parenthesized_pointer(p, type, &name);
            if (name)
                error(p);
        } else
            type = parse_array_suffixes(p, type, NULL, false);
        expression = expr_new(p, EXPR_ALIGNOF, start->location);
        if (expression) {
            expression->as.size_of.type = type;
            expression->as.size_of.enum_definition = definition;
            expression->as.size_of.type_end_offset = peek(p)->location.offset;
        } else {
            free_type(type);
            free_decl(definition);
        }
    } else {
        expression = parse_expression(p, 2);
    }
    expect(p, TOKEN_RPAREN);
    p->alignment_specifiers = specifiers;
    p->contains_alignas = true;
    if (expression)
        add_expr(p, &specifiers->expressions, expression);
}

/**
 * @brief Releases shared alignment specifiers when their last owner is gone.
 * @param specifiers Shared specifiers, or NULL.
 * @return No value.
 */
static void release_alignment_specifiers(AlignmentSpecifiers *specifiers)
{
    if (specifiers && --specifiers->references == 0) {
        for (size_t i = 0; i < specifiers->expressions.count; ++i)
            free_expr(specifiers->expressions.items[i]);
        free(specifiers->expressions.items);
        free(specifiers);
    }
}

/**
 * @brief Attaches declaration-group alignment specifiers to a declaration.
 * @param d Declaration receiving the shared specifiers.
 * @param specifiers Shared specifiers.
 * @return No value.
 */
static void attach_alignment_specifiers(Decl *d, AlignmentSpecifiers *specifiers)
{
    if (d && specifiers && specifiers->expressions.count) {
        d->alignment_specifiers = specifiers;
        ++specifiers->references;
    }
}

/**
 * @brief Applies a declaration group's qualifiers to one variable.
 * @param p Parser state used to report invalid group members.
 * @param d Declaration to qualify; null declarations are ignored.
 * @param is_static Whether the group specifies static linkage.
 * @param is_const Whether the group applies const qualification.
 * @return No value; updates the declaration or records a parse error.
 */
static void qualify_group_declaration(Parser *p, Decl *d, bool is_static, bool is_const)
{
    if (!d)
        return;
    if (d->kind != DECL_VARIABLE) {
        error(p);
        return;
    }
    if (is_static) {
        if (d->is_extern)
            error(p);
        else
            d->is_static = true;
    }
    if (is_const) {
        Type *type = d->as.variable.type;
        while (type && (type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY))
            type = type->pointee;
        if (type)
            type->is_const = true;
    }
}

/**
 * @brief Enters a recursive parser production, reporting excessive nesting once.
 * @param p Parser state whose recursive nesting is tracked.
 * @return True when another recursive production may be entered.
 */
static bool enter_recursion(Parser *p)
{
    if (p->recursion_depth >= PARSER_MAX_RECURSION_DEPTH) {
        if (!p->recursion_limit_reported) {
            error(p);
            p->recursion_limit_reported = true;
        }
        return false;
    }
    ++p->recursion_depth;
    return true;
}

/**
 * @brief Leaves one recursive parser production.
 * @param p Parser state whose recursive nesting is tracked.
 * @return No value.
 */
static void leave_recursion(Parser *p)
{
    --p->recursion_depth;
}

/**
 * @brief Returns the binding precedence of an operator token.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return Operator binding precedence; larger values bind more tightly, and zero means the token is
 * not an operator.
 */
static int precedence(TokenKind k)
{
    switch (k) {
    case TOKEN_ASSIGN:
    case TOKEN_PLUS_ASSIGN:
    case TOKEN_MINUS_ASSIGN:
    case TOKEN_STAR_ASSIGN:
    case TOKEN_SLASH_ASSIGN:
    case TOKEN_PERCENT_ASSIGN:
    case TOKEN_AND_ASSIGN:
    case TOKEN_OR_ASSIGN:
    case TOKEN_XOR_ASSIGN:
    case TOKEN_SHIFT_LEFT_ASSIGN:
    case TOKEN_SHIFT_RIGHT_ASSIGN:
        return 1;
    case TOKEN_QUESTION:
        return 2;
    case TOKEN_LOGICAL_OR:
        return 3;
    case TOKEN_LOGICAL_AND:
        return 4;
    case TOKEN_BITWISE_OR:
        return 5;
    case TOKEN_BITWISE_XOR:
        return 6;
    case TOKEN_BITWISE_AND:
        return 7;
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
        return 8;
    case TOKEN_LESS:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER:
    case TOKEN_GREATER_EQUAL:
        return 9;
    case TOKEN_SHIFT_LEFT:
    case TOKEN_SHIFT_RIGHT:
        return 10;
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        return 11;
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        return 12;
    default:
        return 0;
    }
}
/**
 * @brief Maps a token kind to a binary AST operator.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return Enum value matching the input, or the documented invalid/fallback value.
 */
static BinaryOp binary_op(TokenKind k)
{
    switch (k) {
    case TOKEN_PLUS:
        return BINARY_ADD;
    case TOKEN_MINUS:
        return BINARY_SUBTRACT;
    case TOKEN_STAR:
        return BINARY_MULTIPLY;
    case TOKEN_SLASH:
        return BINARY_DIVIDE;
    case TOKEN_PERCENT:
        return BINARY_REMAINDER;
    case TOKEN_EQUAL:
        return BINARY_EQUAL;
    case TOKEN_NOT_EQUAL:
        return BINARY_NOT_EQUAL;
    case TOKEN_LESS:
        return BINARY_LESS;
    case TOKEN_LESS_EQUAL:
        return BINARY_LESS_EQUAL;
    case TOKEN_GREATER:
        return BINARY_GREATER;
    case TOKEN_GREATER_EQUAL:
        return BINARY_GREATER_EQUAL;
    case TOKEN_LOGICAL_AND:
        return BINARY_LOGICAL_AND;
    case TOKEN_LOGICAL_OR:
        return BINARY_LOGICAL_OR;
    case TOKEN_BITWISE_AND:
        return BINARY_BITWISE_AND;
    case TOKEN_BITWISE_OR:
        return BINARY_BITWISE_OR;
    case TOKEN_BITWISE_XOR:
        return BINARY_BITWISE_XOR;
    case TOKEN_SHIFT_LEFT:
        return BINARY_SHIFT_LEFT;
    default:
        return BINARY_SHIFT_RIGHT;
    }
}
/**
 * @brief Checks whether a token is an assignment operator.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool assignment_kind(TokenKind k)
{
    return k == TOKEN_ASSIGN || k == TOKEN_PLUS_ASSIGN || k == TOKEN_MINUS_ASSIGN ||
           k == TOKEN_STAR_ASSIGN || k == TOKEN_SLASH_ASSIGN || k == TOKEN_PERCENT_ASSIGN ||
           k == TOKEN_AND_ASSIGN || k == TOKEN_OR_ASSIGN || k == TOKEN_XOR_ASSIGN ||
           k == TOKEN_SHIFT_LEFT_ASSIGN || k == TOKEN_SHIFT_RIGHT_ASSIGN;
}
/**
 * @brief Maps an assignment token to its AST operator.
 * @param k Token, AST-node, or type kind used to initialize or inspect a node.
 * @return Enum value matching the input, or the documented invalid/fallback value.
 */
static AssignOp assignment_op(TokenKind k)
{
    switch (k) {
    case TOKEN_PLUS_ASSIGN:
        return ASSIGN_ADD;
    case TOKEN_MINUS_ASSIGN:
        return ASSIGN_SUBTRACT;
    case TOKEN_STAR_ASSIGN:
        return ASSIGN_MULTIPLY;
    case TOKEN_SLASH_ASSIGN:
        return ASSIGN_DIVIDE;
    case TOKEN_PERCENT_ASSIGN:
        return ASSIGN_REMAINDER;
    case TOKEN_AND_ASSIGN:
        return ASSIGN_BITWISE_AND;
    case TOKEN_OR_ASSIGN:
        return ASSIGN_BITWISE_OR;
    case TOKEN_XOR_ASSIGN:
        return ASSIGN_BITWISE_XOR;
    case TOKEN_SHIFT_LEFT_ASSIGN:
        return ASSIGN_SHIFT_LEFT;
    case TOKEN_SHIFT_RIGHT_ASSIGN:
        return ASSIGN_SHIFT_RIGHT;
    default:
        return ASSIGN_SIMPLE;
    }
}

/**
 * @brief Decodes a validated single-byte character literal.
 * @param token Token containing the complete quoted lexeme.
 * @param value Output receiving the decoded byte on success.
 * @return True for a valid character literal, false for a malformed lexeme.
 */
static bool decode_character_literal(const Token *token, unsigned char *value)
{
    const unsigned char *s = (const unsigned char *)token->lexeme;
    size_t n = token->length;
    if (!s || n < 3 || s[0] != '\'' || s[n - 1] != '\'')
        return false;
    if (n == 3 && s[1] != '\\' && s[1] != '\'' && s[1] != '\0' && s[1] != '\n') {
        *value = s[1];
        return true;
    }
    if (s[1] != '\\')
        return false;
    if (n >= 4 && n <= 6 && s[2] >= '0' && s[2] <= '7') {
        unsigned octal = 0;
        for (size_t i = 2; i < n - 1; ++i) {
            if (s[i] < '0' || s[i] > '7') return false;
            octal = octal * 8 + (unsigned)(s[i] - '0');
        }
        if (octal > 255) return false;
        *value = (unsigned char)octal;
        return true;
    }
    if (n == 4) {
        switch (s[2]) {
        case 'a': *value = 7; return true;
        case 'b': *value = 8; return true;
        case 'f': *value = 12; return true;
        case 'n': *value = '\n'; return true;
        case 'r': *value = '\r'; return true;
        case 't': *value = '\t'; return true;
        case 'v': *value = 11; return true;
        case 'e': case 'E': *value = 27; return true;
        case '\\': *value = '\\'; return true;
        case '\'': *value = '\''; return true;
        case '"': *value = '"'; return true;
        case '?': *value = '?'; return true;
        default: return false;
        }
    }
    if (n >= 5 && s[2] == 'x') {
        size_t consumed;
        int32_t decoded;
        if (!token_decode_wide_element(token->lexeme + 1, n - 2, '\'', &consumed, &decoded) ||
            consumed != n - 2 || (uint32_t)decoded > 255)
            return false;
        *value = (unsigned char)decoded;
        return true;
    }
    return false;
}

/**
 * @brief Finds a terminal hex escape that joining would extend.
 * @param tokens Token array containing adjacent literals.
 * @param index Index of the token ending at the boundary.
 * @param end One-past-last adjacent token.
 * @param value Receives the decoded byte when an escape is found.
 * @return Offset of the escape's backslash, or zero when no rewrite is needed.
 */
static size_t string_hex_boundary(const TokenArray *tokens, size_t index, size_t end,
                                  unsigned *value)
{
    const Token *part = &tokens->items[index];
    size_t next = index + 1;
    while (next < end && tokens->items[next].length == 2) ++next;
    if (next == end || tokens->items[next].length < 3 ||
        !tokens->items[next].lexeme ||
        !isxdigit((unsigned char)tokens->items[next].lexeme[1]) ||
        !part->lexeme || part->length < 5)
        return 0;
    size_t pos = part->length - 1;
    while (pos > 1 && isxdigit((unsigned char)part->lexeme[pos - 1])) --pos;
    if (pos == part->length - 1 || pos < 3 || part->lexeme[pos - 1] != 'x' ||
        part->lexeme[pos - 2] != '\\')
        return 0;
    size_t slash = pos - 2, count = 1;
    while (slash > 1 && part->lexeme[slash - 1] == '\\') {
        --slash;
        ++count;
    }
    if (!(count & 1)) return 0;
    size_t consumed;
    int32_t decoded;
    size_t start = pos - 2;
    if (!token_decode_wide_element(part->lexeme + start, part->length - 1 - start,
                                   '"', &consumed, &decoded) ||
        consumed != part->length - 1 - start || (uint32_t)decoded > 255)
        return 0;
    *value = (unsigned)decoded;
    return start;
}

/**
 * @brief Determines padding needed to preserve an octal escape at a string boundary.
 * @param tokens Token array containing adjacent string literals.
 * @param index Index of the string token ending at the boundary.
 * @param end One-past-last adjacent string token.
 * @return Number of leading zero digits to add to the final escape.
 */
static size_t string_octal_padding(const TokenArray *tokens, size_t index, size_t end)
{
    const Token *part = &tokens->items[index];
    if (!part->lexeme || part->length < 3) return 0;
    size_t next_index = index + 1;
    while (next_index < end && tokens->items[next_index].length == 2) ++next_index;
    if (next_index == end) return 0;
    const Token *next = &tokens->items[next_index];
    if (!next->lexeme || next->length < 3 ||
        next->lexeme[1] < '0' || next->lexeme[1] > '7')
        return 0;
    size_t cursor = part->length - 1, digits = 0, slashes = 0;
    while (cursor > 1 && digits < 3 &&
           part->lexeme[cursor - 1] >= '0' && part->lexeme[cursor - 1] <= '7') {
        --cursor;
        ++digits;
    }
    if (!digits || digits == 3) return 0;
    while (cursor > 1 && part->lexeme[cursor - 1] == '\\') {
        --cursor;
        ++slashes;
    }
    return slashes % 2 ? 3 - digits : 0;
}

/**
 * @brief Parses an initializer-list element, including any designators.
 * @param p Parser state.
 * @return Owned initializer expression, or NULL on allocation failure.
 */
static Expr *parse_initializer_element(Parser *p)
{
    if (!at(p, TOKEN_DOT) && !at(p, TOKEN_LBRACKET))
        return parse_expression(p, 1);
    Expr *result = expr_new(p, EXPR_DESIGNATED, peek(p)->location);
    p->contains_designator = true;
    while (at(p, TOKEN_DOT) || at(p, TOKEN_LBRACKET)) {
        InitializerDesignator item = {0};
        item.location = peek(p)->location;
        if (match(p, TOKEN_DOT)) {
            const Token *name = peek(p);
            if (expect(p, TOKEN_IDENTIFIER)) {
                item.field = copy_lexeme(p, name);
                item.field_length = name->length;
            }
        } else {
            match(p, TOKEN_LBRACKET);
            item.index = parse_expression(p, 2);
            expect(p, TOKEN_RBRACKET);
        }
        if (result) {
            InitializerDesignator *items = resize_array(p, result->as.designated.items,
                                                        result->as.designated.count, sizeof item);
            if (items) {
                result->as.designated.items = items;
                items[result->as.designated.count++] = item;
            } else {
                free(item.field);
                free_expr(item.index);
            }
        } else {
            free(item.field);
            free_expr(item.index);
        }
    }
    expect(p, TOKEN_ASSIGN);
    Expr *value = parse_expression(p, 1);
    if (result)
        result->as.designated.value = value;
    else
        free_expr(value);
    return result;
}

/**
 * @brief Parses GNU offsetof's type name and member designator.
 * @param p Parser state.
 * @return Owned expression, or NULL on allocation failure.
 */
static Expr *parse_builtin_offsetof(Parser *p)
{
    const Token *start = peek(p);
    ++p->current;
    p->contains_offsetof = true;
    expect(p, TOKEN_LPAREN);
    Expr *e = expr_new(p, EXPR_OFFSETOF, start->location);
    Decl *definition = NULL;
    Type *type = NULL;
    size_t index = p->current;
    while (index < p->tokens->count &&
           (p->tokens->items[index].kind == TOKEN_CONST ||
            p->tokens->items[index].kind == TOKEN_VOLATILE ||
            p->tokens->items[index].kind == TOKEN_RESTRICT))
        ++index;
    bool aggregate = index + 1 < p->tokens->count &&
        (p->tokens->items[index].kind == TOKEN_STRUCT ||
         p->tokens->items[index].kind == TOKEN_UNION) &&
        (p->tokens->items[index + 1].kind == TOKEN_LBRACE ||
         (index + 2 < p->tokens->count &&
          p->tokens->items[index + 1].kind == TOKEN_IDENTIFIER &&
          p->tokens->items[index + 2].kind == TOKEN_LBRACE));
    if (aggregate) {
        unsigned qualifiers = consume_qualifiers(p, false);
        Decl *structure = parse_struct_definition(p, false);
        type = type_new(p, TYPE_STRUCT, NULL);
        if (type && structure && structure->as.structure.name) {
            size_t length = structure->as.structure.name_length;
            type->is_union = structure->as.structure.is_union;
            type->struct_name = parser_alloc(p, length + 1);
            if (type->struct_name)
                memcpy(type->struct_name, structure->as.structure.name, length + 1);
            apply_qualifiers(type, qualifiers | consume_qualifiers(p, false));
        }
        if (structure)
            add_decl(p, &p->hidden_structures, structure);
        type = parse_pointer_suffix(p, type);
    } else {
        type = parse_embedded_type(p, &definition, false);
    }
    if (starts_parenthesized_declarator(p)) {
        const Token *name;
        type = parse_parenthesized_pointer(p, type, &name);
        if (name) error(p);
    } else {
        type = parse_array_suffixes(p, type, NULL, false);
    }
    if (e) {
        e->as.offset_of.type = type;
        e->as.offset_of.enum_definition = definition;
    } else {
        free_type(type);
        free_decl(definition);
    }
    expect(p, TOKEN_COMMA);
    bool first = true;
    do {
        InitializerDesignator item = {0};
        item.location = peek(p)->location;
        if (first || match(p, TOKEN_DOT)) {
            const Token *name = peek(p);
            if (expect(p, TOKEN_IDENTIFIER)) {
                item.field = copy_lexeme(p, name);
                item.field_length = name->length;
            }
        } else {
            expect(p, TOKEN_LBRACKET);
            item.index = parse_expression(p, 0);
            expect(p, TOKEN_RBRACKET);
        }
        if (e) {
            InitializerDesignator *items = resize_array(p, e->as.offset_of.items,
                                                        e->as.offset_of.count, sizeof item);
            if (items) {
                e->as.offset_of.items = items;
                items[e->as.offset_of.count++] = item;
            } else {
                free(item.field);
                free_expr(item.index);
            }
        } else {
            free(item.field);
            free_expr(item.index);
        }
        first = false;
    } while (at(p, TOKEN_DOT) || at(p, TOKEN_LBRACKET));
    expect(p, TOKEN_RPAREN);
    return e;
}

/**
 * @brief Parses adjacent string tokens when at least one is wide.
 * @param p Parser state at the first literal.
 * @param end Index after the last adjacent literal.
 * @return Owned string expression, or NULL on allocation failure.
 */
static Expr *parse_wide_string(Parser *p, size_t end)
{
    size_t first = p->current, raw_length = 0, capacity = 1;
    Expr *e = expr_new(p, EXPR_STRING, peek(p)->location);
    if (!e)
        return NULL;
    e->string_literal_wide = true;
    p->contains_wide_string = true;
    for (size_t i = first; i < end; ++i) {
        size_t length = p->tokens->items[i].length;
        if (length > SIZE_MAX - raw_length - 1 || length > (SIZE_MAX / 4) - capacity) {
            p->out_of_memory = true;
            return e;
        }
        raw_length += length + (i != first);
        capacity += length;
    }
    e->as.string.bytes = parser_alloc(p, raw_length + 1);
    e->as.string.wide_bytes = parser_alloc(p, capacity * 4);
    if (!e->as.string.bytes || !e->as.string.wide_bytes)
        return e;
    size_t raw = 0, count = 0;
    for (size_t i = first; i < end; ++i) {
        const Token *part = &p->tokens->items[i];
        bool wide = part->kind == TOKEN_WIDE_STRING_LITERAL;
        size_t opening = wide ? 2 : 1;
        if (!part->lexeme || part->length < opening + 1 ||
            (wide && part->lexeme[0] != 'L') ||
            part->lexeme[opening - 1] != '"' || part->lexeme[part->length - 1] != '"') {
            error(p);
            p->current = end;
            return e;
        }
        if (i != first) e->as.string.bytes[raw++] = ' ';
        memcpy(e->as.string.bytes + raw, part->lexeme, part->length);
        raw += part->length;
        for (size_t pos = opening; pos < part->length - 1;) {
            size_t consumed, available = part->length - 1 - pos;
            int32_t value;
            if (!token_decode_wide_element(part->lexeme + pos, available, '"',
                                           &consumed, &value) ||
                (!wide && part->lexeme[pos] == '\\' &&
                 (uint32_t)value > 255)) {
                error(p);
                p->current = end;
                return e;
            }
            uint32_t bits = (uint32_t)value;
            for (size_t j = 0; j < 4; ++j)
                e->as.string.wide_bytes[count * 4 + j] = (unsigned char)(bits >> (j * 8));
            ++count;
            pos += consumed;
        }
    }
    e->as.string.bytes[raw] = 0;
    e->as.string.length = raw;
    e->as.string.wide_count = count;
    memset(e->as.string.wide_bytes + count * 4, 0, 4);
    p->current = end;
    return e;
}

/**
 * @brief Parses a primary or parenthesized expression.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Parsed expression node, or NULL when no expression node can be produced.
 */
static Expr *parse_primary_impl(Parser *p)
{
    const Token *t = peek(p);
    Expr *e = NULL;
    if (t->kind == TOKEN_IDENTIFIER && t->length == 18 &&
        !memcmp(t->lexeme, "__builtin_offsetof", 18))
        return parse_builtin_offsetof(p);
    if (match(p, TOKEN_INTEGER_LITERAL)) {
        e = expr_new(p, EXPR_INTEGER, t->location);
        if (e) {
            char *s = copy_lexeme(p, t);
            if (s) {
                char *end = NULL;
                size_t len = t->length;
                bool suffix_unsigned = false;
                if (len && (s[len - 1] == 'u' || s[len - 1] == 'U')) {
                    suffix_unsigned = true;
                    --len;
                }
                unsigned suffix_longs = 0;
                while (len && (s[len - 1] == 'l' || s[len - 1] == 'L')) {
                    ++suffix_longs;
                    --len;
                }
                if (!suffix_unsigned && len && (s[len - 1] == 'u' || s[len - 1] == 'U')) {
                    suffix_unsigned = true;
                    --len;
                }
                if (suffix_longs == 2) {
                    e->integer_suffix = suffix_unsigned ? INTEGER_SUFFIX_ULL : INTEGER_SUFFIX_LL;
                    p->contains_long_long = true;
                } else if (suffix_longs == 1) {
                    e->integer_suffix = suffix_unsigned ? INTEGER_SUFFIX_UL : INTEGER_SUFFIX_L;
                    if (!suffix_unsigned)
                        p->contains_long_suffix = true;
                } else if (suffix_unsigned)
                    e->integer_suffix = INTEGER_SUFFIX_U;
                if (e->integer_suffix != INTEGER_SUFFIX_NONE) {
                    s[len] = '\0';
                    errno = 0;
                    unsigned long long value = strtoull(s, &end, 0);
                    e->integer_literal_out_of_range =
                        errno == ERANGE || end == s || *end != '\0' || value > UINT64_MAX;
                    e->unsigned_integer_value = (uint64_t)value;
                    if (e->integer_suffix == INTEGER_SUFFIX_L ||
                        e->integer_suffix == INTEGER_SUFFIX_LL) {
                        if (value > INT64_MAX) {
                            if (s[0] == '0' && s[1])
                                e->integer_literal_unsigned = true;
                            else
                                e->integer_literal_out_of_range = true;
                        } else {
                            e->as.integer_value = (int64_t)value;
                        }
                    }
                    free(s);
                    return e;
                }
                errno = 0;
                unsigned long long magnitude = strtoull(s, &end, 0);
                bool magnitude_overflow = (errno == ERANGE || !end || *end != '\0');
                if (!magnitude_overflow && s[0] == '0' && s[1] &&
                    ((magnitude > INT32_MAX && magnitude <= UINT32_MAX) ||
                     magnitude > INT64_MAX)) {
                    e->integer_literal_unsigned = true;
                    e->unsigned_integer_value = (uint64_t)magnitude;
                } else if (!magnitude_overflow && magnitude == ((unsigned long long)INT64_MAX + 1ULL)) {
                    e->as.integer_value = INT64_MIN;
                    e->integer_literal_out_of_range = true;
                } else {
                    errno = 0;
                    long long value = strtoll(s, &end, 0);
                    e->integer_literal_out_of_range = (errno == ERANGE || !end || *end != '\0');
                    e->as.integer_value = (int64_t)value;
                }
                free(s);
            }
        }
        return e;
    }
    if (match(p, TOKEN_REAL_LITERAL)) {
        e = expr_new(p, EXPR_REAL, t->location);
        if (e) {
            char *s = copy_lexeme(p, t);
            if (s) {
                e->real_literal_float = t->length &&
                    (s[t->length - 1] == 'f' || s[t->length - 1] == 'F');
                if (t->length && (s[t->length - 1] == 'l' || s[t->length - 1] == 'L')) {
                    e->real_literal_extended_text = s;
                    p->contains_long_double = true;
                } else {
                    e->as.real_value = e->real_literal_float ? (double)strtof(s, NULL) : strtod(s, NULL);
                    free(s);
                }
            }
        }
        return e;
    }
    if (at(p, TOKEN_STRING_LITERAL) || at(p, TOKEN_WIDE_STRING_LITERAL)) {
        size_t end = p->current;
        bool wide = false;
        while (end < p->tokens->count &&
               (p->tokens->items[end].kind == TOKEN_STRING_LITERAL ||
                p->tokens->items[end].kind == TOKEN_WIDE_STRING_LITERAL)) {
            wide |= p->tokens->items[end].kind == TOKEN_WIDE_STRING_LITERAL;
            ++end;
        }
        if (wide)
            return parse_wide_string(p, end);
        match(p, TOKEN_STRING_LITERAL);
        e = expr_new(p, EXPR_STRING, t->location);
        if (e && !at(p, TOKEN_STRING_LITERAL)) {
            e->as.string.bytes = (unsigned char *)copy_lexeme(p, t);
            e->as.string.length = t->length;
        } else if (e) {
            size_t first = p->current - 1, total = 2;
            bool valid = true;
            while (true) {
                const Token *part = &p->tokens->items[p->current - 1];
                if (!part->lexeme || part->length < 2) {
                    error(p);
                    valid = false;
                    break;
                }
                if (part->length - 2 > SIZE_MAX - total - 1) {
                    p->out_of_memory = true;
                    break;
                }
                total += part->length - 2;
                if (!match(p, TOKEN_STRING_LITERAL))
                    break;
            }
            if (!p->out_of_memory && valid) {
                for (size_t i = first; i + 1 < p->current; ++i) {
                    size_t padding = string_octal_padding(p->tokens, i, p->current);
                    unsigned value;
                    if (string_hex_boundary(p->tokens, i, p->current, &value))
                        ++padding;
                    if (padding > SIZE_MAX - total - 1) {
                        p->out_of_memory = true;
                        break;
                    }
                    total += padding;
                }
            }
            if (!p->out_of_memory && valid) {
                unsigned char *joined = parser_alloc(p, total + 1);
                if (joined) {
                    size_t offset = 1;
                    joined[0] = '"';
                    for (size_t i = first; i < p->current; ++i) {
                        const Token *part = &p->tokens->items[i];
                        unsigned value = 0;
                        size_t hex_start = i + 1 < p->current ?
                            string_hex_boundary(p->tokens, i, p->current, &value) : 0;
                        if (hex_start) {
                            size_t prefix = hex_start - 1;
                            memcpy(joined + offset, part->lexeme + 1, prefix);
                            offset += prefix;
                            joined[offset++] = '\\';
                            joined[offset++] = (unsigned char)('0' + (value >> 6));
                            joined[offset++] = (unsigned char)('0' + ((value >> 3) & 7));
                            joined[offset++] = (unsigned char)('0' + (value & 7));
                            continue;
                        }
                        size_t padding = i + 1 < p->current ?
                            string_octal_padding(p->tokens, i, p->current) : 0;
                        size_t digits = padding ? 3 - padding : 0;
                        size_t prefix = part->length - 2 - digits;
                        memcpy(joined + offset, part->lexeme + 1, prefix);
                        offset += prefix;
                        for (size_t j = 0; j < padding; ++j) joined[offset++] = '0';
                        memcpy(joined + offset, part->lexeme + 1 + prefix, digits);
                        offset += digits;
                    }
                    joined[offset++] = '"';
                    joined[offset] = 0;
                    e->as.string.bytes = joined;
                    e->as.string.length = offset;
                }
            }
        }
        return e;
    }
    if (at(p, TOKEN_WIDE_CHAR_LITERAL)) {
        int32_t value = 0;
        if (!token_decode_wide_character(t->lexeme, t->length, &value))
            error(p);
        match(p, TOKEN_WIDE_CHAR_LITERAL);
        p->contains_wide_char = true;
        e = expr_new(p, EXPR_CHAR, t->location);
        if (e) {
            e->char_literal_wide = true;
            e->as.char_value = value;
        }
        return e;
    }
    if (at(p, TOKEN_CHAR_LITERAL)) {
        unsigned char value = 0;
        if (!decode_character_literal(t, &value))
            error(p);
        match(p, TOKEN_CHAR_LITERAL);
        e = expr_new(p, EXPR_CHAR, t->location);
        if (e)
            e->as.char_value = value;
        return e;
    }
    if (match(p, TOKEN_IDENTIFIER) || match(p, TOKEN_PRINT) || match(p, TOKEN_LENGTH)) {
        e = expr_new(p, EXPR_NAME, t->location);
        if (e) {
            e->as.name.name = copy_lexeme(p, t);
            e->as.name.length = t->length;
        }
        return e;
    }
    if (match(p, TOKEN_SIZEOF) || match(p, TOKEN_ALIGNOF)) {
        bool is_alignof = t->kind == TOKEN_ALIGNOF;
        Type *type = NULL;
        Decl *definition = NULL;
        Expr *operand = NULL;
        size_t type_end_offset = 0;
        if (at(p, TOKEN_LPAREN) && p->current + 1 < p->tokens->count &&
            type_start(p, &p->tokens->items[p->current + 1])) {
            match(p, TOKEN_LPAREN);
            bool saved_inline_aggregate_type = p->allow_inline_aggregate_type;
            p->allow_inline_aggregate_type = true;
            type = parse_embedded_type(p, &definition, false);
            p->allow_inline_aggregate_type = saved_inline_aggregate_type;
            if (starts_parenthesized_declarator(p)) {
                const Token *name;
                type = parse_parenthesized_pointer(p, type, &name);
                if (name)
                    error(p);
            } else
                type = parse_array_suffixes(p, type, NULL, false);
            type_end_offset = peek(p)->location.offset;
            expect(p, TOKEN_RPAREN);
        } else if (!is_alignof) {
            operand = parse_postfix(p);
        } else {
            error(p);
        }
        if (is_alignof)
            p->contains_alignof = true;
        e = expr_new(p, is_alignof ? EXPR_ALIGNOF : EXPR_SIZEOF, t->location);
        if (e) {
            e->as.size_of.type = type;
            e->as.size_of.enum_definition = definition;
            e->as.size_of.operand = operand;
            e->as.size_of.type_end_offset = type_end_offset;
        } else {
            free_type(type);
            free_decl(definition);
            free_expr(operand);
        }
        return e;
    }
    if (match(p, TOKEN_LBRACE)) {
        e = expr_new(p, EXPR_ARRAY_INIT, t->location);
        if (e && !at(p, TOKEN_RBRACE))
            do {
                if (!add_expr(p, &e->as.array_init, parse_initializer_element(p)))
                    error(p);
            } while (match(p, TOKEN_COMMA) && !at(p, TOKEN_RBRACE));
        expect(p, TOKEN_RBRACE);
        return e;
    }
    if (at(p, TOKEN_LPAREN) && p->current + 1 < p->tokens->count &&
        type_start(p, &p->tokens->items[p->current + 1])) {
        match(p, TOKEN_LPAREN);
        Decl *definition = NULL;
        Type *target_type = parse_embedded_type(p, &definition, false);
        if (starts_parenthesized_declarator(p)) {
            const Token *name;
            target_type = parse_parenthesized_pointer(p, target_type, &name);
            if (name)
                error(p);
        } else
            target_type = parse_array_suffixes(p, target_type, NULL, false);
        expect(p, TOKEN_RPAREN);
        if (at(p, TOKEN_LBRACE) && target_type && target_type->kind == TYPE_ARRAY) {
            p->contains_array_compound_literal = true;
            if (!p->contains_fixed_array)
                p->fixed_array_location = t->location;
            p->contains_fixed_array = true;
        } else if (at(p, TOKEN_LBRACE) && target_type && target_type->kind != TYPE_STRUCT)
            p->contains_scalar_compound_literal = true;
        Expr *operand = at(p, TOKEN_LBRACE) ? parse_primary(p) : parse_postfix(p);
        e = expr_new(p, EXPR_CAST, t->location);
        if (e) {
            e->as.cast.target_type = target_type;
            e->as.cast.enum_definition = definition;
            e->as.cast.operand = operand;
        } else {
            free_type(target_type);
            free_decl(definition);
            free_expr(operand);
        }
        return e;
    }
    if (match(p, TOKEN_LPAREN)) {
        e = parse_expression(p, 0);
        expect(p, TOKEN_RPAREN);
        return e;
    }
    if (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS) || match(p, TOKEN_LOGICAL_NOT) ||
        match(p, TOKEN_BITWISE_NOT) || match(p, TOKEN_STAR) || match(p, TOKEN_BITWISE_AND) ||
        match(p, TOKEN_INCREMENT) || match(p, TOKEN_DECREMENT)) {
        TokenKind op = previous(p)->kind;
        Expr *operand = (op == TOKEN_PLUS || op == TOKEN_MINUS)
                            ? parse_expression(p, 13)
                            : parse_postfix(p);
        e = expr_new(p, EXPR_UNARY, t->location);
        if (e) {
            e->as.unary.operand = operand;
            switch (op) {
            case TOKEN_MINUS:
                e->as.unary.op = UNARY_MINUS;
                break;
            case TOKEN_LOGICAL_NOT:
                e->as.unary.op = UNARY_LOGICAL_NOT;
                break;
            case TOKEN_BITWISE_NOT:
                e->as.unary.op = UNARY_BITWISE_NOT;
                break;
            case TOKEN_STAR:
                e->as.unary.op = UNARY_DEREFERENCE;
                break;
            case TOKEN_BITWISE_AND:
                e->as.unary.op = UNARY_ADDRESS;
                break;
            case TOKEN_INCREMENT:
                e->as.unary.op = UNARY_PRE_INCREMENT;
                break;
            case TOKEN_DECREMENT:
                e->as.unary.op = UNARY_PRE_DECREMENT;
                break;
            default:
                e->as.unary.op = UNARY_PLUS;
                break;
            }
        } else
            free_expr(operand);
        return e;
    }
    error(p);
    if (!at(p, TOKEN_EOF) && p->current + 1 < p->tokens->count)
        ++p->current;
    return expr_new(p, EXPR_INTEGER, t->location);
}

/**
 * @brief Parses a primary expression while enforcing the nesting limit.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Parsed expression node, or NULL when the nesting limit is exceeded.
 */
static Expr *parse_primary(Parser *p)
{
    Expr *expression;
    if (!enter_recursion(p))
        return NULL;
    expression = parse_primary_impl(p);
    leave_recursion(p);
    return expression;
}

/**
 * @brief Parses GCC's type operand and lowers va_arg to the existing va_arg form.
 * @param p Parser positioned just after the opening parenthesis.
 * @param call Owned call expression.
 * @return Owned expression, or NULL on allocation failure.
 */
static Expr *parse_builtin_va_arg(Parser *p, Expr *call)
{
    SourceLocation location = call->location;
    Expr *list = parse_expression(p, 1);
    if (!add_expr(p, &call->as.call.arguments, list))
        error(p);
    expect(p, TOKEN_COMMA);
    Decl *definition = NULL;
    Type *type = parse_embedded_type(p, &definition, false);
    if (starts_parenthesized_declarator(p)) {
        const Token *name;
        type = parse_parenthesized_pointer(p, type, &name);
        if (name)
            error(p);
    } else
        type = parse_array_suffixes(p, type, NULL, false);
    expect(p, TOKEN_RPAREN);

    Type *pointer = type_new(p, TYPE_POINTER, type);
    Expr *zero = expr_new(p, EXPR_INTEGER, location);
    Expr *marker = expr_new(p, EXPR_CAST, location);
    if (!marker) {
        free_type(pointer);
        free_decl(definition);
        free_expr(zero);
        free_expr(call);
        return NULL;
    }
    marker->as.cast.target_type = pointer;
    marker->as.cast.enum_definition = definition;
    marker->as.cast.operand = zero;
    if (!add_expr(p, &call->as.call.arguments, marker))
        error(p);

    Expr *dereference = expr_new(p, EXPR_UNARY, location);
    if (!dereference) {
        free_expr(call);
        return NULL;
    }
    dereference->as.unary.op = UNARY_DEREFERENCE;
    dereference->as.unary.operand = call;
    Expr *left = expr_new(p, EXPR_INTEGER, location);
    Expr *value = expr_new(p, EXPR_BINARY, location);
    if (!value) {
        free_expr(left);
        free_expr(dereference);
        return NULL;
    }
    value->as.binary.op = BINARY_COMMA;
    value->as.binary.left = left;
    value->as.binary.right = dereference;
    return value;
}

/**
 * @brief Parses indexing, member access, and postfix operators.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Parsed expression node, or NULL when no expression node can be produced.
 */
static Expr *parse_postfix(Parser *p)
{
    Expr *e = parse_primary(p);
    unsigned held_depth = 0;
    for (;;) {
        if (at(p, TOKEN_LPAREN)) {
            if (!enter_recursion(p))
                break;
            ++held_depth;
            match(p, TOKEN_LPAREN);
            Expr *call = expr_new(p, EXPR_CALL, e ? e->location : peek(p)->location);
            if (!call) {
                while (held_depth > 0) {
                    --held_depth;
                    leave_recursion(p);
                }
                return e;
            }
            call->as.call.callee = e;
            if (e && e->kind == EXPR_NAME && e->as.name.name &&
                !strcmp(e->as.name.name, "__builtin_va_arg")) {
                e = parse_builtin_va_arg(p, call);
                continue;
            }
            if (!at(p, TOKEN_RPAREN))
                do {
                    if (!add_expr(p, &call->as.call.arguments, parse_expression(p, 1)))
                        error(p);
                } while (match(p, TOKEN_COMMA));
            expect(p, TOKEN_RPAREN);
            e = call;
        } else if (at(p, TOKEN_LBRACKET)) {
            if (!enter_recursion(p))
                break;
            ++held_depth;
            match(p, TOKEN_LBRACKET);
            Expr *idx = expr_new(p, EXPR_INDEX, e ? e->location : peek(p)->location);
            if (idx) {
                idx->as.index.base = e;
                idx->as.index.index = parse_expression(p, 0);
            } else
                free_expr(e);
            expect(p, TOKEN_RBRACKET);
            e = idx;
        } else if (at(p, TOKEN_DOT) || at(p, TOKEN_ARROW)) {
            if (!enter_recursion(p))
                break;
            ++held_depth;
            bool through_pointer = match(p, TOKEN_ARROW);
            if (!through_pointer)
                match(p, TOKEN_DOT);
            const Token *name = peek(p);
            expect(p, TOKEN_IDENTIFIER);
            Expr *m = expr_new(p, EXPR_MEMBER, e ? e->location : name->location);
            if (m) {
                m->as.member.base = e;
                m->as.member.name = copy_lexeme(p, name);
                m->as.member.name_length = name->length;
                m->as.member.through_pointer = through_pointer;
            } else
                free_expr(e);
            e = m;
        } else if (at(p, TOKEN_INCREMENT) || at(p, TOKEN_DECREMENT)) {
            if (!enter_recursion(p))
                break;
            ++held_depth;
            if (at(p, TOKEN_INCREMENT))
                match(p, TOKEN_INCREMENT);
            else
                match(p, TOKEN_DECREMENT);
            TokenKind op = previous(p)->kind;
            Expr *u = expr_new(p, EXPR_UNARY, e ? e->location : peek(p)->location);
            if (u) {
                u->as.unary.operand = e;
                u->as.unary.op =
                    op == TOKEN_INCREMENT ? UNARY_POST_INCREMENT : UNARY_POST_DECREMENT;
            } else
                free_expr(e);
            e = u;
        } else
            break;
    }
    while (held_depth > 0) {
        --held_depth;
        leave_recursion(p);
    }
    return e;
}
/**
 * @brief Parses an expression using precedence climbing.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @param min_prec Minimum operator precedence accepted by this parse call.
 * @return Parsed expression node, or NULL when no expression node can be produced.
 */
static Expr *parse_expression_impl(Parser *p, int min_prec)
{
    Expr *left = parse_postfix(p);
    unsigned held_depth = 0;
    for (;;) {
        TokenKind k = peek(p)->kind;
        bool power = k == TOKEN_STAR && p->current + 1 < p->tokens->count &&
                     p->tokens->items[p->current + 1].kind == TOKEN_STAR &&
                     p->tokens->items[p->current + 1].location.offset ==
                         peek(p)->location.offset + peek(p)->length;
        int prec = power ? 13 : precedence(k);
        if (prec < min_prec || (prec == 0 && k != TOKEN_COMMA))
            break;
        if (!enter_recursion(p))
            break;
        ++held_depth;
        const Token *op = peek(p);
        if (p->current + 1 < p->tokens->count)
            ++p->current;
        if (power && p->current + 1 < p->tokens->count)
            ++p->current;
        if (k == TOKEN_QUESTION) {
            Expr *yes = parse_expression(p, 0);
            expect(p, TOKEN_COLON);
            Expr *no = parse_expression(p, prec);
            Expr *c = expr_new(p, EXPR_CONDITIONAL, op->location);
            if (c) {
                c->as.conditional.condition = left;
                c->as.conditional.then_expr = yes;
                c->as.conditional.else_expr = no;
            } else {
                free_expr(left);
                free_expr(yes);
                free_expr(no);
            }
            left = c;
            continue;
        }
        Expr *right = parse_expression(p, (power || assignment_kind(k)) ? prec : prec + 1);
        if (assignment_kind(k)) {
            Expr *a = expr_new(p, EXPR_ASSIGN, op->location);
            if (a) {
                a->as.assign.op = assignment_op(k);
                a->as.assign.target = left;
                a->as.assign.value = right;
            } else {
                free_expr(left);
                free_expr(right);
            }
            left = a;
        } else {
            Expr *b = expr_new(p, EXPR_BINARY, op->location);
            if (b) {
                b->as.binary.op = power ? BINARY_POWER : k == TOKEN_COMMA ? BINARY_COMMA : binary_op(k);
                if (k == TOKEN_COMMA)
                    p->contains_comma_expression = true;
                b->as.binary.left = left;
                b->as.binary.right = right;
            } else {
                free_expr(left);
                free_expr(right);
            }
            left = b;
        }
    }
    while (held_depth > 0) {
        --held_depth;
        leave_recursion(p);
    }
    return left;
}

/**
 * @brief Parses an expression while enforcing the recursive nesting limit.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @param min_prec Minimum operator precedence accepted by this parse call.
 * @return Parsed expression node, or NULL when the nesting limit is exceeded.
 */
static Expr *parse_expression(Parser *p, int min_prec)
{
    Expr *expression;
    if (!enter_recursion(p))
        return NULL;
    expression = parse_expression_impl(p, min_prec);
    leave_recursion(p);
    return expression;
}

/**
 * @brief Parses an initializer and optional array-size suffix.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @param d Declaration node being analyzed or emitted.
 * @return True when the documented condition holds or operation succeeds; otherwise false.
 */
static bool parse_initializer_suffix(Parser *p, Decl *d)
{
    if (match(p, TOKEN_ASSIGN)) {
        d->as.variable.initializer.kind = INIT_EXPRESSION;
        d->as.variable.initializer.expression = parse_expression(p, 1);
        return true;
    }
    return true;
}

/**
 * @brief Parses array suffixes in source order and constructs their type chain.
 * @param p Parser state.
 * @param base Owned element type.
 * @param outer Receives the outer bound for variable declarations, or NULL.
 * @param alias Whether to retain typedef bounds on the type.
 * @param parameter_qualifiers Receives outermost parameter-bracket qualifiers, or NULL.
 * @return Owned array type, or NULL on allocation failure.
 */
static Type *parse_array_suffixes_impl(Parser *p, Type *base, Expr **outer, bool alias,
                                       unsigned *parameter_qualifiers)
{
    Type *head = NULL, *tail = NULL;
    size_t count = 0;
    while (match(p, TOKEN_LBRACKET)) {
        if (parameter_qualifiers && count == 0) {
            while (at(p, TOKEN_CONST) || at(p, TOKEN_VOLATILE) ||
                   at(p, TOKEN_RESTRICT)) {
                if (match(p, TOKEN_CONST))
                    *parameter_qualifiers |= 1;
                else if (match(p, TOKEN_VOLATILE)) {
                    *parameter_qualifiers |= 2;
                    p->contains_volatile = true;
                } else
                    ++p->current;
            }
        }
        Expr *bound = at(p, TOKEN_RBRACKET) ? NULL : parse_expression(p, 1);
        expect(p, TOKEN_RBRACKET);
        Type *node = type_new(p, TYPE_ARRAY, NULL);
        if (!node) {
            free_expr(bound);
            free_type(head);
            free_type(base);
            return NULL;
        }
        if (alias) {
            if (!bound) {
                if (count)
                    error(p);
            } else if (bound->kind == EXPR_INTEGER) {
                uint64_t length = !bound->integer_literal_out_of_range
                    ? (bound->integer_literal_unsigned ? bound->unsigned_integer_value :
                       bound->as.integer_value > 0 ? (uint64_t)bound->as.integer_value : 0) : 0;
                if (!length || length > SIZE_MAX)
                    error(p);
                node->array_length = (size_t)length;
                node->array_length_from_typedef = true;
                free_expr(bound);
                bound = NULL;
            } else {
                node->array_bound = zalloc(p, sizeof(*node->array_bound));
                if (!node->array_bound) {
                    free_expr(bound);
                    free_type(node);
                    free_type(head);
                    free_type(base);
                    return NULL;
                }
                node->array_bound->expression = bound;
                node->array_bound->references = 1;
                p->contains_nested_array = true;
            }
        } else if (count == 0 && outer) {
            *outer = bound;
        } else if (bound || count > 0) {
            node->array_bound = zalloc(p, sizeof(*node->array_bound));
            if (!node->array_bound) {
                free_expr(bound);
                free_type(node);
                free_type(head);
                free_type(base);
                return NULL;
            }
            node->array_bound->expression = bound;
            node->array_bound->references = 1;
        }
        if (tail)
            tail->pointee = node;
        else
            head = node;
        tail = node;
        if (++count > PARSER_MAX_TYPE_INDIRECTION) {
            error(p);
            break;
        }
    }
    if (!head)
        return base;
    tail->pointee = base;
    if (count > 1 || (!outer && !alias && head->array_bound))
        p->contains_nested_array = true;
    return head;
}

/**
 * @brief Parses ordinary array suffixes.
 * @param p Parser state.
 * @param base Owned element type.
 * @param outer Receives the outer bound for variable declarations, or NULL.
 * @param alias Whether to retain typedef bounds on the type.
 * @return Owned array type, or NULL on allocation failure.
 */
static Type *parse_array_suffixes(Parser *p, Type *base, Expr **outer, bool alias)
{
    return parse_array_suffixes_impl(p, base, outer, alias, NULL);
}

/**
 * @brief Parses a static assertion declaration.
 * @param p Parser state.
 * @return Owned assertion declaration, or NULL on allocation failure.
 */
static Decl *parse_static_assertion(Parser *p)
{
    const Token *start = peek(p);
    expect(p, TOKEN_STATIC_ASSERT);
    Decl *d = decl_new(p, DECL_STATIC_ASSERT, start->location);
    if (!d)
        return NULL;
    p->contains_static_assert = true;
    expect(p, TOKEN_LPAREN);
    d->as.static_assertion.condition = parse_expression(p, 1);
    if (match(p, TOKEN_COMMA)) {
        if (at(p, TOKEN_STRING_LITERAL)) {
            d->as.static_assertion.message = parse_primary(p);
            if (d->as.static_assertion.message && d->as.static_assertion.message->string_literal_wide)
                error(p);
        } else
            error(p);
    } else {
        p->contains_static_assert_without_message = true;
    }
    expect(p, TOKEN_RPAREN);
    expect(p, TOKEN_SEMICOLON);
    return d;
}

/**
 * @brief Parses a structure definition and its fields.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @param semicolon Whether to consume the terminating semicolon.
 * @return Structure declaration node, or NULL when it cannot be produced.
 */
static Decl *parse_struct_definition(Parser *p, bool semicolon)
{
    const Token *start = peek(p);
    bool is_union = match(p, TOKEN_UNION);
    if (!is_union)
        expect(p, TOKEN_STRUCT);
    p->contains_union |= is_union;
    const Token *name = peek(p);
    bool named = match(p, TOKEN_IDENTIFIER);
    Decl *d = decl_new(p, DECL_STRUCT, start->location);
    if (!d)
        return NULL;
    d->as.structure.anonymous = !named;
    d->as.structure.is_local = p->scope_depth != 0;
    d->as.structure.is_union = is_union;
    if (named) {
        if (d->as.structure.is_local)
            p->contains_embedded_enum = true;
        ParserTag *tag = structure_tag(p, name, true, false, is_union);
        if (tag && tag->structure_name) {
            size_t length = strlen(tag->structure_name);
            d->as.structure.name = parser_alloc(p, length + 1);
            if (d->as.structure.name)
                memcpy(d->as.structure.name, tag->structure_name, length + 1);
            d->as.structure.name_length = length;
        }
    } else {
        char generated[64];
        int length = snprintf(generated, sizeof generated, "$anonymous_%zu", p->current);
        if (length < 0 || (size_t)length >= sizeof generated) {
            error(p);
            free_decl(d);
            return NULL;
        }
        d->as.structure.name = parser_alloc(p, (size_t)length + 1);
        if (d->as.structure.name)
            memcpy(d->as.structure.name, generated, (size_t)length + 1);
        d->as.structure.name_length = (size_t)length;
    }
    expect(p, TOKEN_LBRACE);
    while (!p->out_of_memory && !at(p, TOKEN_RBRACE) && !at(p, TOKEN_EOF)) {
        if (at(p, TOKEN_STATIC_ASSERT)) {
            Decl *assertion = parse_static_assertion(p);
            if (assertion) {
                assertion->as.static_assertion.preceding_fields = d->as.structure.field_count;
                Decl **items = resize_array(p, d->as.structure.assertions,
                                            d->as.structure.assertion_count, sizeof(*items));
                if (!items)
                    free_decl(assertion);
                else {
                    d->as.structure.assertions = items;
                    items[d->as.structure.assertion_count++] = assertion;
                }
            }
            continue;
        }
        size_t before = p->current;
        AlignmentSpecifiers *saved_specifiers = p->alignment_specifiers;
        AlignmentSpecifiers *specifiers = zalloc(p, sizeof(*specifiers));
        if (!specifiers)
            break;
        specifiers->references = 1;
        p->alignment_specifiers = specifiers;
        unsigned leading_field_qualifiers = consume_qualifiers(p, false);
        Type *base_type = NULL;
        Type *ft;
        size_t field_start = p->current;
        while (field_start < p->tokens->count &&
               (p->tokens->items[field_start].kind == TOKEN_CONST ||
                p->tokens->items[field_start].kind == TOKEN_VOLATILE ||
                p->tokens->items[field_start].kind == TOKEN_RESTRICT))
            ++field_start;
        bool inline_unnamed = field_start + 1 < p->tokens->count &&
                              (p->tokens->items[field_start].kind == TOKEN_STRUCT ||
                               p->tokens->items[field_start].kind == TOKEN_UNION) &&
                              p->tokens->items[field_start + 1].kind == TOKEN_LBRACE;
        bool inline_named = field_start + 2 < p->tokens->count &&
                            (p->tokens->items[field_start].kind == TOKEN_STRUCT ||
                             p->tokens->items[field_start].kind == TOKEN_UNION) &&
                            p->tokens->items[field_start + 1].kind == TOKEN_IDENTIFIER &&
                            p->tokens->items[field_start + 2].kind == TOKEN_LBRACE;
        if (inline_unnamed || inline_named) {
            unsigned qualifiers = consume_qualifiers(p, false);
            if (!enter_recursion(p))
                goto field_done;
            Decl *inner = parse_struct_definition(p, false);
            leave_recursion(p);
            if (!inner)
                goto field_done;
            base_type = type_new(p, TYPE_STRUCT, NULL);
            if (base_type && inner->as.structure.name) {
                base_type->is_union = inner->as.structure.is_union;
                size_t length = inner->as.structure.name_length;
                base_type->struct_name = parser_alloc(p, length + 1);
                if (base_type->struct_name)
                    memcpy(base_type->struct_name, inner->as.structure.name, length + 1);
                qualifiers |= consume_qualifiers(p, false);
                apply_qualifiers(base_type, qualifiers);
            }
            if (!add_decl(p, &p->hidden_structures, inner)) {
                free_type(base_type);
                goto field_done;
            }
            ft = parse_pointer_suffix(p, base_type);
        } else {
            p->allow_enum_definition = true;
            ft = parse_type_with_base(p, &base_type, false);
            p->allow_enum_definition = false;
            if (!base_type)
                base_type = ft;
        }
        if (!ft)
            goto field_done;
        apply_qualifiers(base_type, leading_field_qualifiers);
        if (ft->kind == TYPE_ENUM && at(p, TOKEN_SEMICOLON)) {
            free_type(ft);
            expect(p, TOKEN_SEMICOLON);
            p->alignment_specifiers = saved_specifiers;
            release_alignment_specifiers(specifiers);
            continue;
        }
        do {
            const Token *fn = peek(p);
            bool anonymous = inline_unnamed && at(p, TOKEN_SEMICOLON);
            bool unnamed_bit_field = at(p, TOKEN_COLON);
            if (!anonymous && !unnamed_bit_field) {
                if (starts_parenthesized_declarator(p))
                    ft = parse_parenthesized_pointer(p, ft, &fn);
                else if (!expect(p, TOKEN_IDENTIFIER))
                    break;
                if (!fn || !ft) {
                    error(p);
                    break;
                }
            }
            Decl *field = decl_new(p, DECL_VARIABLE, fn->location);
            if (field) {
                attach_alignment_specifiers(field, specifiers);
                field->as.variable.type = ft;
                ft = NULL;
                field->as.variable.anonymous_member = anonymous;
                if (!anonymous && !unnamed_bit_field) {
                    field->as.variable.name = copy_lexeme(p, fn);
                    field->as.variable.name_length = fn->length;
                }
                if (at(p, TOKEN_LBRACKET)) {
                    field->as.variable.type = parse_array_suffixes(
                        p, field->as.variable.type, &field->as.variable.array_size, false);
                    if (!p->contains_fixed_array)
                        p->fixed_array_location = field->location;
                    p->contains_fixed_array = true;
                }
                if (match(p, TOKEN_COLON)) {
                    field->as.variable.is_bit_field = true;
                    field->as.variable.bit_field_width = parse_expression(p, 2);
                    p->contains_bit_field = true;
                }
                Decl **grown = resize_array(p, d->as.structure.fields,
                                           d->as.structure.field_count, sizeof(*grown));
                if (!grown) {
                    free_decl(field);
                    error(p);
                } else
                    d->as.structure.fields = grown,
                    d->as.structure.fields[d->as.structure.field_count++] = field;
            }
            if (p->out_of_memory)
                break;
            if (at(p, TOKEN_COMMA)) {
                match(p, TOKEN_COMMA);
                ft = parse_pointer_suffix(p, clone_type(p, base_type));
                if (!ft)
                    error(p);
            } else
                break;
        } while (true);
        free_type(ft);
        expect(p, TOKEN_SEMICOLON);
field_done:
        p->alignment_specifiers = saved_specifiers;
        release_alignment_specifiers(specifiers);
        if (before == p->current) {
            error(p);
            if (p->current + 1 < p->tokens->count)
                ++p->current;
        }
    }
    d->as.structure.completion_offset = peek(p)->location.offset;
    d->as.structure.pack_alignment = peek(p)->pack_alignment;
    p->contains_pack |= d->as.structure.pack_alignment != 0;
    expect(p, TOKEN_RBRACE);
    if (semicolon)
        expect(p, TOKEN_SEMICOLON);
    return d;
}

/**
 * @brief Parses one variable name, array suffix, and initializer.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @param type Crunch type node being analyzed or emitted.
 * @return Variable declaration node, or NULL when it cannot be produced.
 */
static Decl *parse_variable_declarator(Parser *p, Type *type)
{
    const Token *name = peek(p);
    if (starts_parenthesized_declarator(p))
        type = parse_parenthesized_pointer(p, type, &name);
    else if (!expect(p, TOKEN_IDENTIFIER)) {
        free_type(type);
        return NULL;
    }
    if (!name || !type) {
        free_type(type);
        error(p);
        return NULL;
    }
    if (!register_name(p, name, NULL)) {
        free_type(type);
        return NULL;
    }
    Decl *d = decl_new(p, DECL_VARIABLE, name->location);
    if (!d) {
        free_type(type);
        return NULL;
    }
    d->as.variable.type = type;
    d->as.variable.name = copy_lexeme(p, name);
    d->as.variable.name_length = name->length;
    if (at(p, TOKEN_LBRACKET)) {
        d->as.variable.type = parse_array_suffixes(p, d->as.variable.type,
                                                  &d->as.variable.array_size, false);
        if (!p->contains_fixed_array)
            p->fixed_array_location = d->location;
        p->contains_fixed_array = true;
    }
    parse_initializer_suffix(p, d);
    return d;
}

/**
 * @brief Adjusts an array or function parameter to a pointer.
 * @param p Parser state recording allocation failure.
 * @param type Owned parameter type.
 * @return Adjusted owned type.
 */
static Type *adjust_array_parameter(Parser *p, Type *type)
{
    if (type && type->kind == TYPE_FUNCTION)
        return type_new(p, TYPE_POINTER, type);
    if (!type || type->kind != TYPE_ARRAY)
        return type;
    if (!type->array_bound && type->pointee && type->pointee->kind == TYPE_ARRAY) {
        Type *inner = type->pointee;
        Type *end = inner;
        while (end && end->kind == TYPE_ARRAY && end->array_bound &&
               !end->array_bound->expression)
            end = end->pointee;
        if (end && end->kind != TYPE_ARRAY) {
            while (inner != end) {
                ArrayBound *empty = inner->array_bound;
                inner->array_bound = NULL;
                if (--empty->references == 0)
                    free(empty);
                inner->kind = TYPE_POINTER;
                inner = inner->pointee;
            }
        }
    }
    Type *element = type->pointee;
    ArrayBound *bound = type->array_bound;
    type->array_bound = NULL;
    type->pointee = NULL;
    if (type->is_const && element)
        element->is_const = true;
    if (type->is_volatile && element)
        element->is_volatile = true;
    free_type(type);
    Type *pointer = type_new(p, TYPE_POINTER, element);
    if (pointer)
        pointer->array_bound = bound;
    else if (bound && --bound->references == 0) {
        free_expr(bound->expression);
        free(bound);
    }
    return pointer;
}

/**
 * @brief Parses and adjusts an array parameter, retaining bracket qualifiers.
 * @param p Parser state.
 * @param type Owned parameter element or declarator type.
 * @return Adjusted owned parameter type, or NULL on allocation failure.
 */
static Type *parse_parameter_array_suffixes(Parser *p, Type *type)
{
    unsigned qualifiers = 0;
    type = parse_array_suffixes_impl(p, type, NULL, false, &qualifiers);
    type = adjust_array_parameter(p, type);
    apply_qualifiers(type, qualifiers);
    return type;
}

/**
 * @brief Parses the ABI-stable subset of identifier-list function definitions.
 * @param p Parser state with the parameter scope already entered.
 * @param d Function definition receiving the parameters.
 * @return No value; records syntax and allocation errors in the parser.
 */
static void parse_old_style_parameters(Parser *p, Decl *d)
{
    do {
        const Token *name = peek(p);
        if (!expect(p, TOKEN_IDENTIFIER))
            return;
        for (size_t i = 0; i < d->as.function.parameters.count; ++i) {
            Parameter *prior = &d->as.function.parameters.items[i];
            if (prior->name && prior->name_length == name->length &&
                !memcmp(prior->name, name->lexeme, name->length))
                error(p);
        }
        Parameter param = {NULL, copy_lexeme(p, name), name->length, name->location, NULL};
        Parameter *items = resize_array(p, d->as.function.parameters.items,
                                        d->as.function.parameters.count, sizeof(param));
        if (!items) {
            free(param.name);
            return;
        }
        d->as.function.parameters.items = items;
        items[d->as.function.parameters.count++] = param;
        if (!register_name(p, name, NULL))
            return;
    } while (match(p, TOKEN_COMMA));
    if (!expect(p, TOKEN_RPAREN))
        return;

    AlignmentSpecifiers *saved_specifiers = p->alignment_specifiers;
    bool saved_allow_auto = p->allow_auto;
    p->alignment_specifiers = NULL;
    p->allow_auto = false;
    while (!p->out_of_memory && !at(p, TOKEN_LBRACE) && !at(p, TOKEN_EOF)) {
        if (!type_start(p, peek(p)) && !at(p, TOKEN_REGISTER)) {
            error(p);
            break;
        }
        Type *base = NULL;
        Type *type = parse_type_with_base(p, &base, true);
        Type *base_copy = clone_type(p, base);
        do {
            const Token *name = peek(p);
            if (!expect(p, TOKEN_IDENTIFIER)) {
                free_type(type);
                type = NULL;
                break;
            }
            type = parse_parameter_array_suffixes(p, type);
            if (type && type->kind != TYPE_INT && type->kind != TYPE_UNSIGNED_INT &&
                type->kind != TYPE_LONG && type->kind != TYPE_UNSIGNED_LONG &&
                type->kind != TYPE_LONG_LONG && type->kind != TYPE_UNSIGNED_LONG_LONG &&
                type->kind != TYPE_REAL && type->kind != TYPE_LONG_DOUBLE &&
                type->kind != TYPE_POINTER)
                error(p);
            size_t i;
            for (i = 0; i < d->as.function.parameters.count; ++i) {
                Parameter *param = &d->as.function.parameters.items[i];
                if (param->name && param->name_length == name->length &&
                    !memcmp(param->name, name->lexeme, name->length)) {
                    if (param->type)
                        error(p);
                    else {
                        param->type = type;
                        type = NULL;
                    }
                    break;
                }
            }
            if (i == d->as.function.parameters.count)
                error(p);
            free_type(type);
            type = NULL;
            if (!match(p, TOKEN_COMMA))
                break;
            type = parse_pointer_suffix(p, clone_type(p, base_copy));
        } while (!p->out_of_memory);
        free_type(type);
        free_type(base_copy);
        if (!expect(p, TOKEN_SEMICOLON))
            break;
    }
    p->alignment_specifiers = saved_specifiers;
    p->allow_auto = saved_allow_auto;
    for (size_t i = 0; i < d->as.function.parameters.count; ++i)
        if (!d->as.function.parameters.items[i].type)
            d->as.function.parameters.items[i].type = type_new(p, TYPE_INT, NULL);
    if (!at(p, TOKEN_LBRACE))
        error(p);
}

/**
 * @brief Parses the parameters of a named function after its opening parenthesis.
 * @param p Parser state with the parameter scope already entered.
 * @param d Function declaration receiving the parameters.
 * @return No value; records syntax and allocation errors in the parser.
 */
static void parse_function_parameters(Parser *p, Decl *d)
{
    bool saved_allow_auto = p->allow_auto;
    p->allow_auto = false;
    if (!at(p, TOKEN_RPAREN)) {
        if (at(p, TOKEN_VOID) && p->current + 1 < p->tokens->count &&
            p->tokens->items[p->current + 1].kind == TOKEN_RPAREN) {
            ++p->current;
        } else
            do {
                if (match_ellipsis(p)) {
                    if (!d->as.function.parameters.count)
                        error(p);
                    for (size_t i = 0; i < d->as.function.parameters.count; ++i)
                        if (d->as.function.parameters.items[i].type->kind == TYPE_VOID)
                            error(p);
                    d->as.function.variadic = true;
                    p->contains_variadic = true;
                    break;
                }
                const Token *ps = peek(p);
                Decl *definition = NULL;
                Type *pt = parse_embedded_type(p, &definition, true);
                const Token *pn = NULL;
                if (starts_abstract_function(p))
                    pt = parse_abstract_function(p, pt);
                else if (starts_parenthesized_declarator(p))
                    pt = parse_parenthesized_pointer(p, pt, &pn);
                else if (at(p, TOKEN_IDENTIFIER) && p->current + 1 < p->tokens->count &&
                         p->tokens->items[p->current + 1].kind == TOKEN_LPAREN)
                    pt = parse_complex_declarator(p, pt, &pn);
                else if (at(p, TOKEN_IDENTIFIER)) {
                    pn = peek(p);
                    ++p->current;
                }
                if (!pt) {
                    free_decl(definition);
                    error(p);
                    break;
                }
                if (!pn)
                    p->contains_unnamed_parameters = true;
                if (pn && !register_name(p, pn, NULL)) {
                    free_type(pt);
                    free_decl(definition);
                    break;
                }
                pt = parse_parameter_array_suffixes(p, pt);
                Parameter param = {pt, pn ? copy_lexeme(p, pn) : NULL, pn ? pn->length : 0,
                                   ps->location, definition};
                Parameter *n = resize_array(p, d->as.function.parameters.items,
                                            d->as.function.parameters.count, sizeof(param));
                if (!n) {
                    free_type(param.type);
                    free(param.name);
                    free_decl(param.enum_definition);
                    error(p);
                    break;
                }
                d->as.function.parameters.items = n;
                d->as.function.parameters.items[d->as.function.parameters.count++] = param;
            } while (match(p, TOKEN_COMMA));
    }
    expect(p, TOKEN_RPAREN);
    p->allow_auto = saved_allow_auto;
}

/**
 * @brief Identifies an abstract function parameter declarator.
 * @param p Parser state.
 * @return True when the next parentheses contain a parameter type list.
 */
static bool starts_abstract_function(Parser *p)
{
    return at(p, TOKEN_LPAREN) && p->current + 1 < p->tokens->count &&
           (p->tokens->items[p->current + 1].kind == TOKEN_RPAREN ||
            type_start(p, &p->tokens->items[p->current + 1]));
}

/**
 * @brief Parses an abstract function type with an owned return type.
 * @param p Parser state at the opening parenthesis.
 * @param result Owned function return type.
 * @return Owned function type, or NULL on allocation failure.
 */
static Type *parse_abstract_function(Parser *p, Type *result)
{
    if (!enter_recursion(p)) {
        free_type(result);
        return NULL;
    }
    const Token *start = peek(p);
    match(p, TOKEN_LPAREN);
    Decl *signature = decl_new(p, DECL_FUNCTION, start->location);
    Type *function = type_new(p, TYPE_FUNCTION, NULL);
    if (!signature || !function) {
        free_decl(signature);
        free_type(function);
        free_type(result);
        leave_recursion(p);
        return NULL;
    }
    signature->as.function.return_type = result;
    function->function_signature = signature;
    ParserName *names = p->names;
    ParserTag *tags = p->tags;
    ++p->scope_depth;
    parse_function_parameters(p, signature);
    restore_names(p, names);
    restore_tags(p, tags);
    --p->scope_depth;
    p->contains_function_types = true;
    leave_recursion(p);
    return function;
}

/**
 * @brief Identifies a grouped declarator at the current token.
 * @param p Parser state.
 * @return True when a parenthesized declarator begins here.
 */
static bool starts_parenthesized_declarator(Parser *p)
{
    if (!at(p, TOKEN_LPAREN) || p->current + 1 >= p->tokens->count)
        return false;
    TokenKind next = p->tokens->items[p->current + 1].kind;
    return next == TOKEN_STAR || next == TOKEN_LPAREN || next == TOKEN_IDENTIFIER;
}

/**
 * @brief Detects grouped declarators requiring recursive parsing.
 * @param p Parser state at the opening parenthesis.
 * @return True for nested grouping or multiple pointer modifiers.
 */
static bool complex_declarator(Parser *p)
{
    if (!starts_parenthesized_declarator(p))
        return false;
    size_t i = p->current + 1;
    if (p->tokens->items[i].kind != TOKEN_STAR)
        return true;
    ++i;
    while (i < p->tokens->count &&
           (p->tokens->items[i].kind == TOKEN_CONST ||
            p->tokens->items[i].kind == TOKEN_VOLATILE ||
            p->tokens->items[i].kind == TOKEN_RESTRICT))
        ++i;
    if (i < p->tokens->count && p->tokens->items[i].kind == TOKEN_IDENTIFIER &&
        i + 1 < p->tokens->count && p->tokens->items[i + 1].kind == TOKEN_RPAREN)
        ++i;
    if (i < p->tokens->count && p->tokens->items[i].kind == TOKEN_RPAREN &&
        (i + 1 >= p->tokens->count ||
         (p->tokens->items[i + 1].kind != TOKEN_LPAREN &&
          p->tokens->items[i + 1].kind != TOKEN_LBRACKET)))
        return true;
    return i < p->tokens->count &&
           (p->tokens->items[i].kind == TOKEN_LPAREN ||
            p->tokens->items[i].kind == TOKEN_STAR ||
            p->tokens->items[i].kind == TOKEN_LBRACKET ||
            (p->tokens->items[i].kind == TOKEN_IDENTIFIER &&
             i + 1 < p->tokens->count &&
             (p->tokens->items[i + 1].kind == TOKEN_LPAREN ||
              p->tokens->items[i + 1].kind == TOKEN_LBRACKET)));
}

/**
 * @brief Parses derived declarator types from the identifier outwards.
 * @param p Parser state.
 * @param name Receives the identifier, if present.
 * @param head Receives the owned derived type chain.
 * @param hole Receives the unfilled base-type link in the chain.
 * @param count Number of derived type nodes parsed so far.
 * @return True on success, false on allocation failure.
 */
static bool parse_derived_declarator(Parser *p, const Token **name, Type **head, Type ***hole,
                                     size_t *count)
{
    if (!enter_recursion(p))
        return false;
    Type *pointers = NULL;
    while (match(p, TOKEN_STAR)) {
        unsigned qualifiers = consume_qualifiers(p, false);
        Type *node = type_new(p, TYPE_POINTER, pointers);
        pointers = node;
        if (!node)
            break;
        apply_qualifiers(node, qualifiers);
        if (++*count > PARSER_MAX_TYPE_INDIRECTION) {
            error(p);
            free_type(pointers);
            leave_recursion(p);
            return false;
        }
    }
    Type *derived = NULL;
    Type **tail = &derived;
    if (!p->out_of_memory && match(p, TOKEN_LPAREN)) {
        if (!parse_derived_declarator(p, name, &derived, &tail, count)) {
            free_type(pointers);
            leave_recursion(p);
            return false;
        }
        expect(p, TOKEN_RPAREN);
    } else if (at(p, TOKEN_IDENTIFIER)) {
        if (*name)
            error(p);
        *name = peek(p);
        ++p->current;
    }
    while (!p->out_of_memory && (at(p, TOKEN_LBRACKET) || at(p, TOKEN_LPAREN))) {
        if (at(p, TOKEN_LBRACKET)) {
            SourceLocation loc = peek(p)->location;
            Type *arrays = parse_array_suffixes(p, NULL, NULL, false);
            if (!arrays)
                break;
            *tail = arrays;
            while (*tail) {
                ++*count;
                tail = &(*tail)->pointee;
            }
            if (!p->contains_fixed_array)
                p->fixed_array_location = loc;
            p->contains_fixed_array = true;
        } else {
            const Token *start = peek(p);
            match(p, TOKEN_LPAREN);
            Decl *signature = decl_new(p, DECL_FUNCTION, start->location);
            Type *function = type_new(p, TYPE_FUNCTION, NULL);
            if (!signature || !function) {
                free_decl(signature);
                free_type(function);
                break;
            }
            function->function_signature = signature;
            *tail = function;
            tail = &signature->as.function.return_type;
            ++*count;
            ParserName *names = p->names;
            ParserTag *tags = p->tags;
            ++p->scope_depth;
            parse_function_parameters(p, signature);
            restore_names(p, names);
            restore_tags(p, tags);
            --p->scope_depth;
            p->contains_function_types = true;
        }
        if (*count > PARSER_MAX_TYPE_INDIRECTION) {
            error(p);
            free_type(derived);
            free_type(pointers);
            leave_recursion(p);
            return false;
        }
    }
    if (p->out_of_memory) {
        free_type(derived);
        free_type(pointers);
        leave_recursion(p);
        return false;
    }
    *tail = pointers;
    while (*tail)
        tail = &(*tail)->pointee;
    *head = derived;
    *hole = tail == &derived ? head : tail;
    leave_recursion(p);
    return true;
}

/**
 * @brief Parses a recursively grouped declarator around an owned base type.
 * @param p Parser state.
 * @param base Owned base type.
 * @param name Receives the optional identifier.
 * @return Completed owned type, or NULL on allocation failure.
 */
static Type *parse_complex_declarator(Parser *p, Type *base, const Token **name)
{
    Type *derived = NULL;
    Type **hole = NULL;
    size_t count = 0;
    *name = NULL;
    if (!parse_derived_declarator(p, name, &derived, &hole, &count)) {
        free_type(base);
        return NULL;
    }
    *hole = base;
    return derived ? derived : base;
}

/**
 * @brief Parses a parenthesized function- or array-pointer declarator.
 * @param p Parser state.
 * @param result Owned return type.
 * @param name Receives the optional declarator name.
 * @param outer_function Function receiving parameters inside the parentheses, or NULL.
 * @return Owned pointer type, or NULL on allocation failure.
 */
static Type *parse_parenthesized_pointer_impl(Parser *p, Type *result, const Token **name,
                                              Decl *outer_function)
{
    if (!enter_recursion(p)) {
        free_type(result);
        return NULL;
    }
    expect(p, TOKEN_LPAREN);
    expect(p, TOKEN_STAR);
    unsigned qualifiers = consume_qualifiers(p, false);
    *name = NULL;
    if (at(p, TOKEN_IDENTIFIER)) {
        *name = peek(p);
        ++p->current;
    }
    if (outer_function) {
        expect(p, TOKEN_LPAREN);
        parse_function_parameters(p, outer_function);
    }
    Type *arrays = NULL;
    if (at(p, TOKEN_LBRACKET)) {
        arrays = parse_array_suffixes(p, NULL, NULL, false);
        if (!arrays) {
            free_type(result);
            leave_recursion(p);
            return NULL;
        }
        if (!p->contains_fixed_array)
            p->fixed_array_location = (*name ? *name : peek(p))->location;
        p->contains_fixed_array = true;
    }
    expect(p, TOKEN_RPAREN);
    if (at(p, TOKEN_LBRACKET)) {
        result = parse_array_suffixes(p, result, NULL, false);
        if (!result) {
            free_type(arrays);
            leave_recursion(p);
            return NULL;
        }
        Type *pointer = type_new(p, TYPE_POINTER, result);
        if (!pointer) {
            free_type(arrays);
            leave_recursion(p);
            return NULL;
        }
        apply_qualifiers(pointer, qualifiers);
        if (arrays) {
            Type *tail = arrays;
            while (tail->pointee)
                tail = tail->pointee;
            tail->pointee = pointer;
            pointer = arrays;
        }
        p->contains_nested_array = true;
        leave_recursion(p);
        return pointer;
    }
    expect(p, TOKEN_LPAREN);
    Decl *signature = decl_new(p, DECL_FUNCTION, peek(p)->location);
    Type *function = type_new(p, TYPE_FUNCTION, NULL);
    if (!signature || !function) {
        free_decl(signature);
        free_type(function);
        free_type(result);
        free_type(arrays);
        leave_recursion(p);
        return NULL;
    }
    signature->as.function.return_type = result;
    function->function_signature = signature;
    if (!at(p, TOKEN_RPAREN)) {
        if (at(p, TOKEN_VOID) && p->current + 1 < p->tokens->count &&
            p->tokens->items[p->current + 1].kind == TOKEN_RPAREN)
            ++p->current;
        else
            do {
                if (match_ellipsis(p)) {
                    if (!signature->as.function.parameters.count)
                        error(p);
                    for (size_t i = 0; i < signature->as.function.parameters.count; ++i)
                        if (signature->as.function.parameters.items[i].type->kind == TYPE_VOID)
                            error(p);
                    signature->as.function.variadic = true;
                    p->contains_variadic = true;
                    break;
                }
                Parameter param = {0};
                param.location = peek(p)->location;
                param.type = parse_type(p);
                if (starts_abstract_function(p)) {
                    param.type = parse_abstract_function(p, param.type);
                } else if (starts_parenthesized_declarator(p)) {
                    const Token *nested_name;
                    param.type = parse_parenthesized_pointer(p, param.type, &nested_name);
                } else if (at(p, TOKEN_IDENTIFIER) && p->current + 1 < p->tokens->count &&
                           p->tokens->items[p->current + 1].kind == TOKEN_LPAREN) {
                    const Token *nested_name;
                    param.type = parse_complex_declarator(p, param.type, &nested_name);
                } else if (at(p, TOKEN_IDENTIFIER))
                    ++p->current;
                param.type = parse_parameter_array_suffixes(p, param.type);
                if (!param.type)
                    break;
                Parameter *items = resize_array(p, signature->as.function.parameters.items,
                                                signature->as.function.parameters.count,
                                                sizeof(param));
                if (!items) {
                    free_type(param.type);
                    break;
                }
                signature->as.function.parameters.items = items;
                items[signature->as.function.parameters.count++] = param;
            } while (match(p, TOKEN_COMMA));
    }
    expect(p, TOKEN_RPAREN);
    Type *pointer = type_new(p, TYPE_POINTER, function);
    if (!pointer) {
        free_type(arrays);
        leave_recursion(p);
        return NULL;
    }
    apply_qualifiers(pointer, qualifiers);
    if (arrays) {
        Type *tail = arrays;
        while (tail->pointee)
            tail = tail->pointee;
        tail->pointee = pointer;
        pointer = arrays;
    }
    p->contains_function_types = true;
    leave_recursion(p);
    return pointer;
}

/**
 * @brief Parses a parenthesized pointer declarator without an outer function.
 * @param p Parser state.
 * @param result Owned pointed-to type.
 * @param name Receives the optional declarator name.
 * @return Owned pointer type, or NULL on allocation failure.
 */
static Type *parse_parenthesized_pointer(Parser *p, Type *result, const Token **name)
{
    if (complex_declarator(p))
        return parse_complex_declarator(p, result, name);
    return parse_parenthesized_pointer_impl(p, result, name, NULL);
}

/**
 * @brief Parses a typedef declaration and records its aliases in the current scope.
 * @param p Parser state containing the declaration.
 * @param global Whether the declaration is at file scope.
 * @param leading_qualifiers Qualifiers preceding the typedef keyword.
 * @param additional Output array for additional alias declarators.
 * @return First declaration, or NULL on failure.
 */
static Decl *parse_type_alias(Parser *p, bool global, unsigned leading_qualifiers,
                              DeclArray *additional)
{
    const Token *start = peek(p);
    expect(p, TOKEN_DEF);
    Type *base = NULL;
    Decl *first = NULL;
    Type *type;
    size_t index = p->current;
    while (index < p->tokens->count &&
           (p->tokens->items[index].kind == TOKEN_CONST ||
            p->tokens->items[index].kind == TOKEN_VOLATILE ||
            p->tokens->items[index].kind == TOKEN_RESTRICT))
        ++index;
    bool unnamed = index + 1 < p->tokens->count &&
                   (p->tokens->items[index].kind == TOKEN_STRUCT ||
                    p->tokens->items[index].kind == TOKEN_UNION) &&
                   p->tokens->items[index + 1].kind == TOKEN_LBRACE;
    bool named_inline = index + 2 < p->tokens->count &&
                        (p->tokens->items[index].kind == TOKEN_STRUCT ||
                         p->tokens->items[index].kind == TOKEN_UNION) &&
                        p->tokens->items[index + 1].kind == TOKEN_IDENTIFIER &&
                        p->tokens->items[index + 2].kind == TOKEN_LBRACE;
    if (unnamed || named_inline) {
        unsigned qualifiers = consume_qualifiers(p, false);
        Decl *structure = parse_struct_definition(p, false);
        base = type_new(p, TYPE_STRUCT, NULL);
        if (base && structure && structure->as.structure.name) {
            base->is_union = structure->as.structure.is_union;
            size_t length = structure->as.structure.name_length;
            base->struct_name = parser_alloc(p, length + 1);
            if (base->struct_name)
                memcpy(base->struct_name, structure->as.structure.name, length + 1);
            qualifiers |= consume_qualifiers(p, false);
            apply_qualifiers(base, qualifiers);
        }
        if (structure && !global) {
            if (!add_decl(p, &p->hidden_structures, structure))
                structure = NULL;
        } else {
            first = structure;
        }
        type = parse_pointer_suffix(p, base);
    } else {
        p->allow_enum_definition = true;
        type = parse_type_with_base(p, &base, false);
        p->allow_enum_definition = false;
    }
    if (type && leading_qualifiers)
        apply_qualifiers(base, leading_qualifiers);
    Type *declarator = type;
    do {
        const Token *name = peek(p);
        if (starts_parenthesized_declarator(p) ||
            (at(p, TOKEN_IDENTIFIER) && p->current + 1 < p->tokens->count &&
             p->tokens->items[p->current + 1].kind == TOKEN_LPAREN)) {
            Type *result = clone_type(p, declarator);
            if (declarator != type)
                free_type(declarator);
            declarator = at(p, TOKEN_IDENTIFIER)
                ? parse_complex_declarator(p, result, &name)
                : parse_parenthesized_pointer(p, result, &name);
            if (!name || !declarator) {
                error(p);
                break;
            }
        } else
        if (!expect(p, TOKEN_IDENTIFIER))
            break;
        Decl *d = decl_new(p, DECL_TYPE_ALIAS, start->location);
        if (!d)
            break;
        d->as.type_alias.name = copy_lexeme(p, name);
        d->as.type_alias.name_length = name->length;
        d->declarator_offset = name->location.offset;
        d->as.type_alias.type = clone_type(p, declarator);
        if (at(p, TOKEN_LBRACKET)) {
            d->as.type_alias.type = parse_array_suffixes(p, d->as.type_alias.type, NULL, true);
            if (!p->contains_fixed_array)
                p->fixed_array_location = d->location;
            p->contains_fixed_array = true;
            p->contains_array_typedef = true;
        }
        if (!d->as.type_alias.name || !d->as.type_alias.type ||
            !register_name(p, name, d->as.type_alias.type)) {
            free_decl(d);
            break;
        }
        if (!first)
            first = d;
        else if (!additional || !add_decl(p, additional, d)) {
            if (!additional)
                free_decl(d);
            break;
        }
        if (declarator != type)
            free_type(declarator);
        declarator = NULL;
        if (!match(p, TOKEN_COMMA))
            break;
        declarator = parse_pointer_suffix(p, clone_type(p, base));
    } while (!p->out_of_memory);
    if (declarator && declarator != type)
        free_type(declarator);
    free_type(type);
    expect(p, TOKEN_SEMICOLON);
    return first;
}
/**
 * @brief Parses declarations and comma-separated declarators.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @param global Whether the declaration is at global scope.
 * @param additional Output array receiving additional declarators from the same declaration.
 * @return Declaration node, or NULL when it cannot be produced.
 */
static Decl *parse_declaration_impl(Parser *p, bool global, DeclArray *additional)
{
    while (at(p, TOKEN_ALIGNAS))
        parse_alignment_specifier(p);
    size_t prefix = p->current;
    unsigned leading_qualifiers = 0;
    while (prefix < p->tokens->count &&
           (p->tokens->items[prefix].kind == TOKEN_VOLATILE ||
            p->tokens->items[prefix].kind == TOKEN_RESTRICT))
        ++prefix;
    if (prefix < p->tokens->count &&
        (p->tokens->items[prefix].kind == TOKEN_STATIC ||
         p->tokens->items[prefix].kind == TOKEN_EXTERN ||
         p->tokens->items[prefix].kind == TOKEN_THREAD_LOCAL ||
         p->tokens->items[prefix].kind == TOKEN_DEF))
        leading_qualifiers = consume_qualifiers(p, false);
    if (at(p, TOKEN_DEF)) {
        return parse_type_alias(p, global, leading_qualifiers, additional);
    }
    bool static_const_group = global && at(p, TOKEN_STATIC) &&
                              p->current + 2 < p->tokens->count &&
                              p->tokens->items[p->current + 1].kind == TOKEN_CONST &&
                              p->tokens->items[p->current + 2].kind == TOKEN_LBRACE;
    if (static_const_group ||
        ((at(p, TOKEN_CONST) || (global && at(p, TOKEN_STATIC))) &&
         p->current + 1 < p->tokens->count &&
         p->tokens->items[p->current + 1].kind == TOKEN_LBRACE)) {
        if (leading_qualifiers)
            error(p);
        if (!enter_recursion(p))
            return NULL;
        bool is_static_group = at(p, TOKEN_STATIC);
        bool is_const_group = !is_static_group || static_const_group;
        match(p, is_static_group ? TOKEN_STATIC : TOKEN_CONST);
        if (static_const_group)
            match(p, TOKEN_CONST);
        match(p, TOKEN_LBRACE);
        Decl *first = NULL;
        while (!p->out_of_memory && !at(p, TOKEN_RBRACE) && !at(p, TOKEN_EOF)) {
            size_t before = p->current;
            DeclArray rest = {0};
            Decl *d = NULL;
            if (type_start(p, peek(p)) || at(p, TOKEN_AUTO) || at(p, TOKEN_STATIC) || at(p, TOKEN_THREAD_LOCAL) || at(p, TOKEN_REGISTER) ||
                at(p, TOKEN_EXTERN))
                d = parse_declaration(p, global, &rest);
            else
                error(p);
            qualify_group_declaration(p, d, is_static_group, is_const_group);
            if (d) {
                if (!first)
                    first = d;
                else if (additional)
                    add_decl(p, additional, d);
                else
                    free_decl(d);
            }
            for (size_t i = 0; i < rest.count; ++i) {
                qualify_group_declaration(p, rest.items[i], is_static_group, is_const_group);
                if (additional)
                    add_decl(p, additional, rest.items[i]);
                else
                    free_decl(rest.items[i]);
            }
            free(rest.items);
            if (before == p->current && p->current + 1 < p->tokens->count)
                ++p->current;
        }
        expect(p, TOKEN_RBRACE);
        expect(p, TOKEN_SEMICOLON);
        leave_recursion(p);
        return first;
    }
    const Token *start = peek(p);
    bool is_inline = false, is_static = false, is_extern = false, noreturn = false;
    bool is_thread_local = false;
    while (at(p, TOKEN_INLINE) || at(p, TOKEN_STATIC) || at(p, TOKEN_ALIGNAS) ||
           at(p, TOKEN_EXTERN) || at(p, TOKEN_NORETURN) || at(p, TOKEN_THREAD_LOCAL)) {
        if (at(p, TOKEN_ALIGNAS))
            parse_alignment_specifier(p);
        else if (match(p, TOKEN_INLINE)) {
            if (is_inline) error(p);
            is_inline = true;
        } else if (match(p, TOKEN_STATIC)) {
            if (is_static) error(p);
            is_static = true;
        } else if (match(p, TOKEN_EXTERN)) {
            if (is_extern) error(p);
            is_extern = true;
        } else if (match(p, TOKEN_THREAD_LOCAL)) {
            if (is_thread_local) error(p);
            is_thread_local = true;
            p->contains_thread_local = true;
        } else {
            match(p, TOKEN_NORETURN);
            if (noreturn) error(p);
            noreturn = true;
        }
    }
    if (is_static && is_extern)
        error(p);
    if (is_thread_local && !global)
        error(p);
    if (is_inline && !global)
        error(p);
    leading_qualifiers |= consume_qualifiers(p, true);
    if ((at(p, TOKEN_STRUCT) || at(p, TOKEN_UNION)) &&
        p->current + 2 < p->tokens->count &&
        p->tokens->items[p->current + 1].kind == TOKEN_IDENTIFIER &&
        p->tokens->items[p->current + 2].kind == TOKEN_SEMICOLON) {
        bool is_union = at(p, TOKEN_UNION);
        if (is_inline || is_static || is_extern || is_thread_local || noreturn || leading_qualifiers ||
            p->alignment_specifiers->auto_storage ||
            p->in_for_initializer)
            error(p);
        p->current++;
        const Token *name = peek(p);
        p->current += 2;
        structure_tag(p, name, false, true, is_union);
        p->contains_union |= is_union;
        return NULL;
    }
    size_t index = p->current;
    while (index < p->tokens->count &&
           (p->tokens->items[index].kind == TOKEN_CONST ||
            p->tokens->items[index].kind == TOKEN_VOLATILE ||
            p->tokens->items[index].kind == TOKEN_RESTRICT ||
            p->tokens->items[index].kind == TOKEN_REGISTER))
        ++index;
    bool unnamed = index + 1 < p->tokens->count &&
                   (p->tokens->items[index].kind == TOKEN_STRUCT ||
                    p->tokens->items[index].kind == TOKEN_UNION) &&
                   p->tokens->items[index + 1].kind == TOKEN_LBRACE;
    bool named_inline = index + 2 < p->tokens->count &&
                        (p->tokens->items[index].kind == TOKEN_STRUCT ||
                         p->tokens->items[index].kind == TOKEN_UNION) &&
                        p->tokens->items[index + 1].kind == TOKEN_IDENTIFIER &&
                        p->tokens->items[index + 2].kind == TOKEN_LBRACE;
    Type *base = NULL;
    Type *type;
    if (unnamed || named_inline) {
        unsigned qualifiers = consume_qualifiers(p, true);
        Decl *structure = parse_struct_definition(p, false);
        type = type_new(p, TYPE_STRUCT, NULL);
        if (type && structure && structure->as.structure.name) {
            type->is_union = structure->as.structure.is_union;
            size_t length = structure->as.structure.name_length;
            type->struct_name = parser_alloc(p, length + 1);
            if (type->struct_name)
                memcpy(type->struct_name, structure->as.structure.name, length + 1);
            qualifiers |= consume_qualifiers(p, true);
            apply_qualifiers(type, qualifiers);
        }
        if (structure) {
            if (global && named_inline)
                add_decl(p, &p->pending_enums, structure);
            else
                add_decl(p, &p->hidden_structures, structure);
        }
        base = type;
        type = parse_pointer_suffix(p, type);
    } else {
        p->allow_enum_definition = true;
        type = parse_type_with_base(p, &base, true);
        p->allow_enum_definition = false;
        if (!base)
            base = type;
    }
    if (!type)
        return NULL;
    if (p->alignment_specifiers->auto_storage &&
        (global || is_static || is_extern || is_thread_local ||
         p->alignment_specifiers->register_storage))
        error(p);
    apply_qualifiers(base, leading_qualifiers);
    if (named_inline && type->kind == TYPE_STRUCT && at(p, TOKEN_SEMICOLON)) {
        if (is_thread_local || p->alignment_specifiers->auto_storage)
            error(p);
        if (!global) {
            Decl *scope = decl_new(p, DECL_STRUCT_SCOPE, start->location);
            if (scope && type->struct_name) {
                scope->as.structure.is_union = type->is_union;
                size_t length = strlen(type->struct_name);
                scope->as.structure.name = parser_alloc(p, length + 1);
                if (scope->as.structure.name)
                    memcpy(scope->as.structure.name, type->struct_name, length + 1);
                scope->as.structure.name_length = length;
                add_decl(p, &p->pending_enums, scope);
                p->contains_embedded_enum = true;
            } else
                free_decl(scope);
        }
        free_type(type);
        expect(p, TOKEN_SEMICOLON);
        return NULL;
    }
    if (type->kind == TYPE_ENUM && at(p, TOKEN_SEMICOLON)) {
        if (is_thread_local || p->alignment_specifiers->auto_storage)
            error(p);
        if (type->struct_name) {
            ParserTag *tag = p->tags;
            while (tag && tag->identity != type->array_length)
                tag = tag->next;
            if (tag && tag->depth != p->scope_depth)
                error(p);
        }
        free_type(type);
        expect(p, TOKEN_SEMICOLON);
        return NULL;
    }
    if (match(p, TOKEN_INLINE)) {
        if (is_inline || !global)
            error(p);
        is_inline = true;
    }
    if (match(p, TOKEN_NORETURN)) {
        if (noreturn)
            error(p);
        noreturn = true;
    } else if (at(p, TOKEN_IDENTIFIER) && peek(p)->length == 8 &&
               !memcmp(peek(p)->lexeme, "noreturn", 8) &&
               p->current + 2 < p->tokens->count &&
               p->tokens->items[p->current + 1].kind == TOKEN_IDENTIFIER &&
               p->tokens->items[p->current + 2].kind == TOKEN_LPAREN) {
        ++p->current;
        noreturn = true;
    }
    const Token *name = peek(p);
    bool function_pointer = starts_parenthesized_declarator(p);
    size_t declarator_name = p->current + 2;
    if (function_pointer && p->tokens->items[p->current + 1].kind == TOKEN_STAR) {
        while (declarator_name < p->tokens->count &&
               (p->tokens->items[declarator_name].kind == TOKEN_CONST ||
                p->tokens->items[declarator_name].kind == TOKEN_VOLATILE ||
                p->tokens->items[declarator_name].kind == TOKEN_RESTRICT))
            ++declarator_name;
    }
    if (function_pointer && declarator_name + 1 < p->tokens->count &&
        p->tokens->items[declarator_name].kind == TOKEN_IDENTIFIER &&
        p->tokens->items[declarator_name + 1].kind == TOKEN_LPAREN) {
        if (is_thread_local || p->alignment_specifiers->auto_storage)
            error(p);
        name = &p->tokens->items[declarator_name];
        if (!register_name(p, name, NULL)) {
            free_type(type);
            return NULL;
        }
        Decl *d = decl_new(p, DECL_FUNCTION, start->location);
        if (!d) {
            free_type(type);
            return NULL;
        }
        d->as.function.name = copy_lexeme(p, name);
        d->declarator_offset = name->location.offset;
        d->as.function.name_length = name->length;
        d->as.function.noreturn = noreturn;
        d->as.function.is_inline = is_inline;
        p->contains_external_inline |= is_inline && !is_static;
        d->is_static = is_static;
        d->is_extern = is_extern;
        ParserName *parameter_scope = p->names;
        ParserTag *parameter_tags = p->tags;
        ++p->scope_depth;
        d->as.function.return_type = parse_parenthesized_pointer_impl(p, type, &name, d);
        if (d->as.function.return_type && at(p, TOKEN_LBRACE)) {
            for (size_t i = 0; i < d->as.function.parameters.count; ++i)
                if (!d->as.function.parameters.items[i].name)
                    error(p);
            p->function_body_scope = true;
            d->as.function.body = parse_statement(p);
            p->function_body_scope = false;
        } else if (d->as.function.return_type) {
            expect(p, TOKEN_SEMICOLON);
        }
        restore_names(p, parameter_scope);
        restore_tags(p, parameter_tags);
        --p->scope_depth;
        if (!d->as.function.return_type) {
            free_decl(d);
            return NULL;
        }
        return d;
    }
    if (function_pointer)
        type = parse_parenthesized_pointer(p, type, &name);
    else if (!expect(p, TOKEN_IDENTIFIER)) {
        free_type(type);
        return NULL;
    }
    if (!name || !type) {
        free_type(type);
        error(p);
        return NULL;
    }
    if (!register_name(p, name, NULL)) {
        free_type(type);
        return NULL;
    }
    if (type->kind == TYPE_FUNCTION && type->function_signature) {
        if (is_thread_local || p->alignment_specifiers->auto_storage)
            error(p);
        Decl *d = type->function_signature;
        type->function_signature = NULL;
        free_type(type);
        d->location = start->location;
        d->as.function.name = copy_lexeme(p, name);
        d->declarator_offset = name->location.offset;
        d->as.function.name_length = name->length;
        d->as.function.noreturn = noreturn;
        d->as.function.is_inline = is_inline;
        p->contains_external_inline |= is_inline && !is_static;
        d->is_static = is_static;
        d->is_extern = is_extern;
        ParserName *parameter_scope = p->names;
        ParserTag *parameter_tags = p->tags;
        ++p->scope_depth;
        for (size_t i = 0; i < d->as.function.parameters.count; ++i) {
            Parameter *param = &d->as.function.parameters.items[i];
            if (!param->name) {
                if (at(p, TOKEN_LBRACE))
                    error(p);
                continue;
            }
            for (size_t j = 0; j < p->current; ++j) {
                const Token *token = &p->tokens->items[j];
                if (token->kind == TOKEN_IDENTIFIER &&
                    token->location.offset >= param->location.offset &&
                    token->length == param->name_length &&
                    !memcmp(token->lexeme, param->name, token->length)) {
                    register_name(p, token, NULL);
                    break;
                }
            }
        }
        if (at(p, TOKEN_LBRACE)) {
            p->function_body_scope = true;
            d->as.function.body = parse_statement(p);
            p->function_body_scope = false;
        } else
            expect(p, TOKEN_SEMICOLON);
        restore_names(p, parameter_scope);
        restore_tags(p, parameter_tags);
        --p->scope_depth;
        return d;
    }
    if (!function_pointer && match(p, TOKEN_LPAREN)) {
        if (is_thread_local || p->alignment_specifiers->auto_storage)
            error(p);
        Decl *d = decl_new(p, DECL_FUNCTION, start->location);
        if (!d) {
            free_type(type);
            return NULL;
        }
        d->as.function.return_type = type;
        d->as.function.name = copy_lexeme(p, name);
        d->declarator_offset = name->location.offset;
        d->as.function.name_length = name->length;
        d->as.function.noreturn = noreturn;
        d->as.function.is_inline = is_inline;
        p->contains_external_inline |= is_inline && !is_static;
        d->is_static = is_static;
        d->is_extern = is_extern;
        ParserName *parameter_scope = p->names;
        ParserTag *parameter_tags = p->tags;
        ++p->scope_depth;
        if (at(p, TOKEN_IDENTIFIER) && !type_start(p, peek(p)))
            parse_old_style_parameters(p, d);
        else
            parse_function_parameters(p, d);
        if (at(p, TOKEN_LBRACE)) {
            for (size_t i = 0; i < d->as.function.parameters.count; ++i)
                if (!d->as.function.parameters.items[i].name)
                    error(p);
            p->function_body_scope = true;
            d->as.function.body = parse_statement(p);
            p->function_body_scope = false;
        } else
            expect(p, TOKEN_SEMICOLON);
        restore_names(p, parameter_scope);
        restore_tags(p, parameter_tags);
        --p->scope_depth;
        (void)global;
        return d;
    }

    if (noreturn || is_inline)
        error(p);
    Decl *d = decl_new(p, DECL_VARIABLE, start->location);
    if (!d) {
        free_type(type);
        return NULL;
    }
    d->as.variable.type = type;
    d->is_static = is_static;
    d->is_extern = is_extern;
    d->is_thread_local = is_thread_local;
    d->as.variable.name = copy_lexeme(p, name);
    d->declarator_offset = name->location.offset;
    d->as.variable.name_length = name->length;
    if (at(p, TOKEN_LBRACKET)) {
        d->as.variable.type = parse_array_suffixes(p, d->as.variable.type,
                                                  &d->as.variable.array_size, false);
        if (!p->contains_fixed_array)
            p->fixed_array_location = d->location;
        p->contains_fixed_array = true;
    }
    parse_initializer_suffix(p, d);
    if (!global && is_static)
        p->contains_static_local = true;
    while (!p->out_of_memory && match(p, TOKEN_COMMA)) {
        Type *next_type = parse_pointer_suffix(p, clone_type(p, base));
        if (!next_type) {
            error(p);
            break;
        }
        Decl *next = parse_variable_declarator(p, next_type);
        if (next) {
            next->is_static = is_static;
            next->is_extern = is_extern;
            next->is_thread_local = is_thread_local;
            if (!global && is_static)
                p->contains_static_local = true;
            if (!additional)
                free_decl(next);
            else if (!add_decl(p, additional, next))
                error(p);
        }
    }
    expect(p, TOKEN_SEMICOLON);
    (void)global;
    return d;
}

/**
 * @brief Emits inline enum definitions before their associated declarators.
 * @param p Parser state containing pending enum definitions.
 * @param global Whether the declaration is at file scope.
 * @param additional Receives declarations after the first.
 * @return First declaration, or NULL when none could be parsed.
 */
static Decl *parse_declaration(Parser *p, bool global, DeclArray *additional)
{
    if (at(p, TOKEN_STATIC_ASSERT))
        return parse_static_assertion(p);
    AlignmentSpecifiers *saved_specifiers = p->alignment_specifiers;
    bool saved_allow_auto = p->allow_auto;
    AlignmentSpecifiers *specifiers = zalloc(p, sizeof(*specifiers));
    if (!specifiers)
        return NULL;
    specifiers->references = 1;
    p->alignment_specifiers = specifiers;
    p->allow_auto = true;
    size_t marker = p->pending_enums.count;
    DeclArray rest = {0};
    Decl *first = parse_declaration_impl(p, global, &rest);
    p->alignment_specifiers = saved_specifiers;
    p->allow_auto = saved_allow_auto;
    attach_alignment_specifiers(first, specifiers);
    for (size_t i = 0; i < rest.count; ++i)
        attach_alignment_specifiers(rest.items[i], specifiers);
    release_alignment_specifiers(specifiers);
    if (p->pending_enums.count > marker) {
        Decl *definition = p->pending_enums.items[marker];
        for (size_t i = marker + 1; i < p->pending_enums.count; ++i)
            add_decl(p, additional, p->pending_enums.items[i]);
        if (first)
            add_decl(p, additional, first);
        first = definition;
    }
    p->pending_enums.count = marker;
    for (size_t i = 0; i < rest.count; ++i)
        add_decl(p, additional, rest.items[i]);
    free(rest.items);
    return first;
}

/**
 * @brief Appends all declarators from a declaration to the enclosing statement list.
 * @param p Parser state used to parse the declaration.
 * @param statements Enclosing statement list.
 * @return No value; reports allocation or parse errors through the parser.
 */
static void parse_declarations_into(Parser *p, StmtArray *statements)
{
    DeclArray additional = {0};
    Decl *first = parse_declaration(p, false, &additional);
    if (first) {
        Stmt *child = stmt_new(p, STMT_DECLARATION, first->location);
        if (child)
            child->as.declaration.declaration = first;
        else
            free_decl(first);
        if (!child || !add_stmt(p, statements, child))
            error(p);
    }
    for (size_t i = 0; i < additional.count; ++i) {
        Stmt *child = stmt_new(p, STMT_DECLARATION, additional.items[i]->location);
        if (child)
            child->as.declaration.declaration = additional.items[i];
        else
            free_decl(additional.items[i]);
        if (!child || !add_stmt(p, statements, child))
            error(p);
    }
    free(additional.items);
}
/**
 * @brief Parses a declaration statement.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Declaration statement node, or NULL when it cannot be produced.
 */
static Stmt *parse_declaration_statement(Parser *p)
{
    SourceLocation location = peek(p)->location;
    DeclArray additional = {0};
    Decl *first = parse_declaration(p, false, &additional);
    if (!first) {
        for (size_t i = 0; i < additional.count; ++i)
            free_decl(additional.items[i]);
        free(additional.items);
        return NULL;
    }
    if (!additional.count) {
        Stmt *statement = stmt_new(p, STMT_DECLARATION, location);
        if (statement)
            statement->as.declaration.declaration = first;
        else
            free_decl(first);
        free(additional.items);
        return statement;
    }
    Stmt *group = stmt_new(p, STMT_BLOCK, location);
    if (!group) {
        free_decl(first);
        for (size_t i = 0; i < additional.count; ++i)
            free_decl(additional.items[i]);
        free(additional.items);
        return NULL;
    }
    Stmt *statement = stmt_new(p, STMT_DECLARATION, first->location);
    if (statement)
        statement->as.declaration.declaration = first;
    else
        free_decl(first);
    if (!statement || !add_stmt(p, &group->as.block.statements, statement))
        error(p);
    for (size_t i = 0; i < additional.count; i++) {
        statement = stmt_new(p, STMT_DECLARATION, additional.items[i]->location);
        if (statement)
            statement->as.declaration.declaration = additional.items[i];
        else
            free_decl(additional.items[i]);
        if (!statement || !add_stmt(p, &group->as.block.statements, statement))
            error(p);
    }
    free(additional.items);
    return group;
}
/**
 * @brief Parses a braced sequence of statements.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Block statement node, or NULL when it cannot be produced.
 */
static Stmt *parse_block(Parser *p)
{
    const Token *start = peek(p);
    expect(p, TOKEN_LBRACE);
    Stmt *s = stmt_new(p, STMT_BLOCK, start->location);
    if (!s)
        return NULL;
    ParserName *scope = p->names;
    ParserTag *tags = p->tags;
    bool shared_scope = p->function_body_scope;
    p->function_body_scope = false;
    if (!shared_scope)
        ++p->scope_depth;
    while (!p->out_of_memory && !at(p, TOKEN_RBRACE) && !at(p, TOKEN_EOF)) {
        size_t before = p->current;
        if (!(at(p, TOKEN_IDENTIFIER) && p->current + 1 < p->tokens->count &&
              p->tokens->items[p->current + 1].kind == TOKEN_COLON) &&
            (at(p, TOKEN_ALIGNAS) || at(p, TOKEN_STATIC_ASSERT) || at(p, TOKEN_DEF) || at(p, TOKEN_AUTO) || at(p, TOKEN_STATIC) || at(p, TOKEN_THREAD_LOCAL) || at(p, TOKEN_EXTERN) || at(p, TOKEN_REGISTER) ||
             at(p, TOKEN_INLINE) || at(p, TOKEN_NORETURN) || type_start(p, peek(p)))) {
            parse_declarations_into(p, &s->as.block.statements);
        } else {
            Stmt *child = parse_statement(p);
            if (child && !add_stmt(p, &s->as.block.statements, child))
                error(p);
        }
        if (before == p->current) {
            error(p);
            if (p->current + 1 < p->tokens->count)
                ++p->current;
        }
    }
    expect(p, TOKEN_RBRACE);
    restore_names(p, scope);
    restore_tags(p, tags);
    if (!shared_scope)
        --p->scope_depth;
    return s;
}
/**
 * @brief Parses one Crunch statement.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Statement node, or NULL when no statement node can be produced.
 */
static Stmt *parse_statement_impl(Parser *p)
{
    const Token *start = peek(p);
    if (at(p, TOKEN_IDENTIFIER) && p->current + 1 < p->tokens->count &&
        p->tokens->items[p->current + 1].kind == TOKEN_COLON) {
        p->contains_named_labels = true;
        Stmt *s = stmt_new(p, STMT_LABEL, start->location);
        if (!s)
            return NULL;
        s->as.label.name = copy_lexeme(p, start);
        p->current += 2;
        s->as.label.statement = parse_statement(p);
        return s;
    }
    if (match(p, TOKEN_GOTO)) {
        p->contains_named_labels = true;
        Stmt *s = stmt_new(p, STMT_GOTO, start->location);
        if (!s)
            return NULL;
        const Token *name = peek(p);
        if (expect(p, TOKEN_IDENTIFIER))
            s->as.label.name = copy_lexeme(p, name);
        expect(p, TOKEN_SEMICOLON);
        return s;
    }
    if (at(p, TOKEN_LBRACE))
        return parse_block(p);
    if (match(p, TOKEN_IF)) {
        Stmt *s = stmt_new(p, STMT_IF, start->location);
        if (!s)
            return NULL;
        expect(p, TOKEN_LPAREN);
        s->as.if_stmt.condition = parse_expression(p, 0);
        expect(p, TOKEN_RPAREN);
        s->as.if_stmt.then_branch = parse_statement(p);
        if (match(p, TOKEN_ELSE))
            s->as.if_stmt.else_branch = parse_statement(p);
        return s;
    }
    if (match(p, TOKEN_SWITCH)) {
        Stmt *s = stmt_new(p, STMT_SWITCH, start->location);
        if (!s)
            return NULL;
        expect(p, TOKEN_LPAREN);
        s->as.switch_stmt.expression = parse_expression(p, 0);
        expect(p, TOKEN_RPAREN);
        expect(p, TOKEN_LBRACE);
        ParserName *scope = p->names;
        ParserTag *tags = p->tags;
        ++p->scope_depth;
        SwitchCase *current = NULL;
        while (!p->out_of_memory && !at(p, TOKEN_RBRACE) && !at(p, TOKEN_EOF)) {
            if (at(p, TOKEN_CASE) || at(p, TOKEN_DEFAULT)) {
                const Token *label = peek(p);
                bool is_default = match(p, TOKEN_DEFAULT);
                if (!is_default)
                    expect(p, TOKEN_CASE);
                SwitchCase item = {0};
                item.location = label->location;
                if (!is_default)
                    item.value = parse_expression(p, 1);
                expect(p, TOKEN_COLON);
                SwitchCase *grown = resize_array(p, s->as.switch_stmt.cases.items,
                                                 s->as.switch_stmt.cases.count, sizeof(*grown));
                if (!grown) {
                    error(p);
                    free_expr(item.value);
                    free_stmt(s);
                    restore_names(p, scope);
                    restore_tags(p, tags);
                    --p->scope_depth;
                    return NULL;
                }
                s->as.switch_stmt.cases.items = grown;
                current = &grown[s->as.switch_stmt.cases.count++];
                *current = item;
            } else {
                StmtArray *statements = current ? &current->statements : &s->as.switch_stmt.prefix;
                if (!current)
                    p->contains_named_labels = true;
                if (!(at(p, TOKEN_IDENTIFIER) && p->current + 1 < p->tokens->count &&
                      p->tokens->items[p->current + 1].kind == TOKEN_COLON) &&
                    (at(p, TOKEN_ALIGNAS) || at(p, TOKEN_STATIC_ASSERT) || at(p, TOKEN_DEF) || at(p, TOKEN_AUTO) || at(p, TOKEN_STATIC) || at(p, TOKEN_THREAD_LOCAL) || at(p, TOKEN_EXTERN) || at(p, TOKEN_REGISTER) ||
                     at(p, TOKEN_INLINE) || at(p, TOKEN_NORETURN) || type_start(p, peek(p)))) {
                    parse_declarations_into(p, statements);
                } else {
                    Stmt *child = parse_statement(p);
                    if (!child || !add_stmt(p, statements, child))
                        error(p);
                }
            }
        }
        expect(p, TOKEN_RBRACE);
        restore_names(p, scope);
        restore_tags(p, tags);
        --p->scope_depth;
        return s;
    }
    if (match(p, TOKEN_FOR)) {
        Stmt *s = stmt_new(p, STMT_FOR, start->location);
        if (!s)
            return NULL;
        ParserName *scope = p->names;
        ParserTag *tags = p->tags;
        ++p->scope_depth;
        expect(p, TOKEN_LPAREN);
        if (!at(p, TOKEN_SEMICOLON)) {
            if (at(p, TOKEN_ALIGNAS) || at(p, TOKEN_DEF) || at(p, TOKEN_AUTO) || at(p, TOKEN_STATIC) || at(p, TOKEN_THREAD_LOCAL) || at(p, TOKEN_EXTERN) || at(p, TOKEN_REGISTER) ||
                at(p, TOKEN_INLINE) || at(p, TOKEN_NORETURN) ||
                type_start(p, peek(p))) {
                p->in_for_initializer = true;
                s->as.for_stmt.initializer = parse_declaration_statement(p);
                p->in_for_initializer = false;
            } else {
                s->as.for_stmt.initializer = stmt_new(p, STMT_EXPRESSION, start->location);
                if (!s->as.for_stmt.initializer) {
                    free_stmt(s);
                    restore_names(p, scope);
                    restore_tags(p, tags);
                    --p->scope_depth;
                    return NULL;
                }
                s->as.for_stmt.initializer->as.expression.expression = parse_expression(p, 0);
                expect(p, TOKEN_SEMICOLON);
            }
        } else
            expect(p, TOKEN_SEMICOLON);
        if (!at(p, TOKEN_SEMICOLON))
            s->as.for_stmt.condition = parse_expression(p, 0);
        expect(p, TOKEN_SEMICOLON);
        if (!at(p, TOKEN_RPAREN))
            s->as.for_stmt.update = parse_expression(p, 0);
        expect(p, TOKEN_RPAREN);
        s->as.for_stmt.body = parse_statement(p);
        restore_names(p, scope);
        restore_tags(p, tags);
        --p->scope_depth;
        return s;
    }
    if (match(p, TOKEN_DO)) {
        Stmt *s = stmt_new(p, STMT_DO, start->location);
        p->contains_do_while = true;
        if (!s)
            return NULL;
        s->as.loop_stmt.body = parse_statement(p);
        expect(p, TOKEN_LOOP);
        expect(p, TOKEN_LPAREN);
        s->as.loop_stmt.condition = parse_expression(p, 0);
        expect(p, TOKEN_RPAREN);
        expect(p, TOKEN_SEMICOLON);
        return s;
    }
    if (match(p, TOKEN_LOOP)) {
        Stmt *s = stmt_new(p, STMT_LOOP, start->location);
        if (!s)
            return NULL;
        expect(p, TOKEN_LPAREN);
        s->as.loop_stmt.condition = parse_expression(p, 0);
        expect(p, TOKEN_RPAREN);
        s->as.loop_stmt.body = parse_statement(p);
        return s;
    }
    if (match(p, TOKEN_BREAK)) {
        Stmt *s = stmt_new(p, STMT_BREAK, start->location);
        expect(p, TOKEN_SEMICOLON);
        return s;
    }
    if (match(p, TOKEN_CONTINUE)) {
        Stmt *s = stmt_new(p, STMT_CONTINUE, start->location);
        expect(p, TOKEN_SEMICOLON);
        return s;
    }
    if (match(p, TOKEN_RETURN)) {
        Stmt *s = stmt_new(p, STMT_RETURN, start->location);
        if (!s)
            return NULL;
        if (!at(p, TOKEN_SEMICOLON))
            s->as.return_stmt.expression = parse_expression(p, 0);
        expect(p, TOKEN_SEMICOLON);
        return s;
    }
    if (at(p, TOKEN_ALIGNAS) || at(p, TOKEN_DEF) || at(p, TOKEN_AUTO) || at(p, TOKEN_STATIC) || at(p, TOKEN_THREAD_LOCAL) || at(p, TOKEN_EXTERN) || at(p, TOKEN_REGISTER) || at(p, TOKEN_NORETURN) ||
        type_start(p, peek(p)))
        return parse_declaration_statement(p);
    Stmt *s = stmt_new(p, STMT_EXPRESSION, start->location);
    if (!s)
        return NULL;
    if (!at(p, TOKEN_SEMICOLON))
        s->as.expression.expression = parse_expression(p, 0);
    expect(p, TOKEN_SEMICOLON);
    return s;
}

/**
 * @brief Parses a statement while enforcing the recursive nesting limit.
 * @param p Parser state whose current token and cursor are used by the operation.
 * @return Parsed statement node, or NULL when the nesting limit is exceeded.
 */
static Stmt *parse_statement(Parser *p)
{
    Stmt *statement;
    if (!enter_recursion(p))
        return NULL;
    statement = parse_statement_impl(p);
    leave_recursion(p);
    return statement;
}

/**
 * @brief Parses tokens and collects all parser error locations.
 * @param tokens Token array produced by the lexer.
 * @param output Output pointer receiving the AST on success; set to NULL before parsing.
 * @param error_locations Optional output array receiving source locations for syntax errors.
 * @param error_capacity Maximum number of source locations writable to error_locations.
 * @param error_count Optional output receiving the total syntax-error count.
 * @param nesting_limit_exceeded Optional output flag set if parser nesting exceeds the limit.
 * @return COMPILER_OK on success, COMPILER_PARSE_ERROR for syntax errors, or another status for
 * invalid input or allocation failure.
 */
CompilerStatus parser_run_with_errors_and_depth(const TokenArray *tokens, AstProgram **output,
                                                SourceLocation *error_locations,
                                                size_t error_capacity, size_t *error_count,
                                                bool *nesting_limit_exceeded)
{
    Parser p;
    AstProgram *program;
    if (error_count)
        *error_count = 0;
    if (nesting_limit_exceeded)
        *nesting_limit_exceeded = false;
    if (!output)
        return COMPILER_INVALID_INPUT;
    *output = NULL;
    if (!tokens || !tokens->items || tokens->count == 0)
        return COMPILER_INVALID_INPUT;
    for (size_t i = 0; i < tokens->count; ++i) {
        const Token *token = &tokens->items[i];
        if ((unsigned)token->kind > TOKEN_WIDE_STRING_LITERAL || !token->lexeme ||
            (token->kind == TOKEN_EOF
                 ? i != tokens->count - 1 || token->length != 0
                 : i == tokens->count - 1 || token->length == 0) ||
            (token->kind != TOKEN_INVALID && token->length &&
             memchr(token->lexeme, '\0', token->length)))
            return COMPILER_INVALID_INPUT;
    }
    memset(&p, 0, sizeof(p));
    p.tokens = tokens;
    p.error_locations = error_locations;
    p.error_capacity = error_capacity;
    program = (AstProgram *)zalloc(&p, sizeof(*program));
    if (!program)
        return COMPILER_OUT_OF_MEMORY;
    for (size_t i = 0; i < tokens->count; ++i)
        if (tokens->items[i].kind == TOKEN_SHORT)
            program->contains_short = true;
    for (size_t i = 0; i < tokens->count; ++i)
        if (token_has_variable_hex_escape(&tokens->items[i]))
            program->contains_variable_hex_escape = true;
    for (size_t i = 0; i < tokens->count; ++i) {
        const Token *token = &tokens->items[i];
        if (token->kind == TOKEN_FLOAT ||
            (token->kind == TOKEN_REAL_LITERAL && token->length &&
             (token->lexeme[token->length - 1] == 'f' ||
              token->lexeme[token->length - 1] == 'F'))) {
            program->contains_float = true;
            program->float_location = token->location;
            break;
        }
    }
    while (!p.out_of_memory && !at(&p, TOKEN_EOF)) {
        size_t before = p.current;
        bool had_long_double = p.contains_long_double;
        size_t enum_marker = p.pending_enums.count;
        Decl *d = NULL;
        DeclArray additional = {0};
        if (at(&p, TOKEN_ALIGNAS) || at(&p, TOKEN_STATIC_ASSERT) || at(&p, TOKEN_DEF) || at(&p, TOKEN_AUTO) || at(&p, TOKEN_REGISTER) || at(&p, TOKEN_INLINE) ||
            at(&p, TOKEN_NORETURN) ||
            type_start(&p, peek(&p)) ||
                 at(&p, TOKEN_STATIC) || at(&p, TOKEN_THREAD_LOCAL) || at(&p, TOKEN_EXTERN))
            d = parse_declaration(&p, true, &additional);
        else {
            error(&p);
            if (p.current + 1 < p.tokens->count)
                ++p.current;
        }
        int runtime_prototype = d && d->kind == DECL_FUNCTION && !d->is_extern &&
                                !d->is_static && !d->as.function.body &&
                                crunch_is_runtime_name(d->as.function.name);
        if (runtime_prototype)
            p.contains_long_double = had_long_double;
        if (!runtime_prototype && !program->contains_float_ir) {
            for (size_t i = before; i < p.current; ++i) {
                const Token *token = &tokens->items[i];
                if (token->kind == TOKEN_FLOAT ||
                    (token->kind == TOKEN_REAL_LITERAL && token->length &&
                     (token->lexeme[token->length - 1] == 'f' ||
                      token->lexeme[token->length - 1] == 'F'))) {
                    program->contains_float_ir = true;
                    break;
                }
            }
        }
        if (!runtime_prototype && (!d || d->kind != DECL_TYPE_ALIAS) &&
            !program->contains_unsigned) {
            for (size_t i = before; i < p.current; ++i) {
                const Token *token = &tokens->items[i];
                if (token->kind == TOKEN_UNSIGNED ||
                    (token->kind == TOKEN_INTEGER_LITERAL && token->length &&
                     (token->lexeme[token->length - 1] == 'u' ||
                      token->lexeme[token->length - 1] == 'U' ||
                      (token->length >= 2 &&
                       (token->lexeme[token->length - 1] == 'l' ||
                        token->lexeme[token->length - 1] == 'L') &&
                       (token->lexeme[token->length - 2] == 'u' ||
                        token->lexeme[token->length - 2] == 'U'))))) {
                    program->contains_unsigned = true;
                    program->unsigned_location = token->location;
                    break;
                }
            }
        }
        for (size_t i = enum_marker; i < p.pending_enums.count; ++i)
            if (!add_decl(&p, &program->declarations, p.pending_enums.items[i]))
                error(&p);
        p.pending_enums.count = enum_marker;
        if (d && !add_decl(&p, &program->declarations, d))
            error(&p);
        for (size_t i = 0; i < additional.count; i++)
            if (!add_decl(&p, &program->declarations, additional.items[i]))
                error(&p);
        free(additional.items);
        if (before == p.current) {
            error(&p);
            if (p.current + 1 < p.tokens->count)
                ++p.current;
        }
    }
    for (size_t i = 0; i < p.hidden_structures.count; ++i)
        add_decl(&p, &program->declarations, p.hidden_structures.items[i]);
    program->contains_fixed_array = p.contains_fixed_array;
    program->fixed_array_location = p.fixed_array_location;
    program->contains_static_local = p.contains_static_local;
    program->contains_thread_local = p.contains_thread_local;
    program->contains_character_types = p.contains_character_types;
    program->contains_long_suffix = p.contains_long_suffix;
    program->contains_long_long = p.contains_long_long;
    program->contains_bool = p.contains_bool;
    program->contains_enum = p.contains_enum;
    program->contains_embedded_enum = p.contains_embedded_enum;
    program->contains_designator = p.contains_designator;
    program->contains_named_labels = p.contains_named_labels;
    program->contains_union = p.contains_union;
    program->contains_function_types = p.contains_function_types;
    program->contains_variadic = p.contains_variadic;
    program->contains_array_typedef = p.contains_array_typedef;
    program->contains_nested_array = p.contains_nested_array;
    program->contains_static_assert = p.contains_static_assert;
    program->contains_static_assert_without_message = p.contains_static_assert_without_message;
    program->contains_wide_char = p.contains_wide_char;
    program->contains_wide_string = p.contains_wide_string;
    program->contains_external_inline = p.contains_external_inline;
    program->contains_volatile = p.contains_volatile;
    program->contains_bit_field = p.contains_bit_field;
    program->contains_alignof = p.contains_alignof;
    program->contains_offsetof = p.contains_offsetof;
    program->contains_alignas = p.contains_alignas;
    program->contains_pack = p.contains_pack;
    program->contains_array_compound_literal = p.contains_array_compound_literal;
    program->contains_scalar_compound_literal = p.contains_scalar_compound_literal;
    program->contains_long_double = p.contains_long_double;
    program->contains_do_while = p.contains_do_while;
    program->contains_comma_expression = p.contains_comma_expression;
    program->contains_unnamed_parameters = p.contains_unnamed_parameters;
    free(p.hidden_structures.items);
    free(p.pending_enums.items);
    if (error_count)
        *error_count = p.error_count;
    if (nesting_limit_exceeded)
        *nesting_limit_exceeded = p.recursion_limit_reported;
    restore_names(&p, NULL);
    restore_tags(&p, NULL);
    if (p.out_of_memory || p.errors) {
        ast_program_destroy(program);
        return p.out_of_memory ? COMPILER_OUT_OF_MEMORY : COMPILER_PARSE_ERROR;
    }
    *output = program;
    return COMPILER_OK;
}

/**
 * @brief Parses tokens and collects all parser error locations.
 * @param tokens Token array produced by the lexer.
 * @param output Output pointer receiving the AST on success; set to NULL before parsing.
 * @param error_locations Optional output array receiving source locations for syntax errors.
 * @param error_capacity Maximum number of source locations writable to error_locations.
 * @param error_count Optional output receiving the total syntax-error count.
 * @return COMPILER_OK on success, COMPILER_PARSE_ERROR for syntax errors, or another status for
 * invalid input or allocation failure.
 */
CompilerStatus parser_run_with_errors(const TokenArray *tokens, AstProgram **output,
                                      SourceLocation *error_locations, size_t error_capacity,
                                      size_t *error_count)
{
    return parser_run_with_errors_and_depth(tokens, output, error_locations, error_capacity,
                                            error_count, NULL);
}

/**
 * @brief Parses tokens and returns the first error location.
 * @param tokens Token array produced by the lexer.
 * @param output Output pointer receiving the AST on success; set to NULL before parsing.
 * @param error_location Optional output receiving the first syntax-error location.
 * @return COMPILER_OK on success, COMPILER_PARSE_ERROR for syntax errors, or another status for
 * invalid input or allocation failure.
 */
CompilerStatus parser_run_with_error_location(const TokenArray *tokens, AstProgram **output,
                                              SourceLocation *error_location)
{
    size_t count = 0;
    CompilerStatus status =
        parser_run_with_errors(tokens, output, error_location, error_location ? 1 : 0, &count);
    return status;
}

/**
 * @brief Parses tokens into an AST program.
 * @param tokens Token array produced by the lexer.
 * @param output Output pointer receiving the AST on success; set to NULL before parsing.
 * @return COMPILER_OK on success, COMPILER_PARSE_ERROR for syntax errors, or another status for
 * invalid input or allocation failure.
 */
CompilerStatus parser_run(const TokenArray *tokens, AstProgram **output)
{
    return parser_run_with_error_location(tokens, output, NULL);
}

/**
 * @brief Recursively releases a type tree.
 * @param t Crunch type node being analyzed or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void free_type(Type *t)
{
    if (t) {
        free_type(t->pointee);
        free_decl(t->function_signature);
        if (t->array_bound && --t->array_bound->references == 0) {
            free_expr(t->array_bound->expression);
            free(t->array_bound);
        }
        free(t->struct_name);
        free(t);
    }
}
/**
 * @brief Recursively releases an expression tree and owned literal data.
 * @param e Expression node being analyzed or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void free_expr(Expr *e)
{
    size_t i;
    if (!e)
        return;
    switch (e->kind) {
    case EXPR_REAL:
        free(e->real_literal_extended_text);
        break;
    case EXPR_STRING:
        free(e->as.string.bytes);
        free(e->as.string.wide_bytes);
        break;
    case EXPR_ARRAY_INIT:
        for (i = 0; i < e->as.array_init.count; i++)
            free_expr(e->as.array_init.items[i]);
        free(e->as.array_init.items);
        break;
    case EXPR_DESIGNATED:
        for (i = 0; i < e->as.designated.count; ++i) {
            free(e->as.designated.items[i].field);
            free_expr(e->as.designated.items[i].index);
        }
        free(e->as.designated.items);
        free_expr(e->as.designated.value);
        break;
    case EXPR_OFFSETOF:
        free_type(e->as.offset_of.type);
        free_decl(e->as.offset_of.enum_definition);
        for (i = 0; i < e->as.offset_of.count; ++i) {
            free(e->as.offset_of.items[i].field);
            free_expr(e->as.offset_of.items[i].index);
        }
        free(e->as.offset_of.items);
        break;
    case EXPR_NAME:
        free(e->as.name.name);
        break;
    case EXPR_UNARY:
        free_expr(e->as.unary.operand);
        break;
    case EXPR_BINARY:
        free_expr(e->as.binary.left);
        free_expr(e->as.binary.right);
        break;
    case EXPR_ASSIGN:
        free_expr(e->as.assign.target);
        free_expr(e->as.assign.value);
        break;
    case EXPR_CALL:
        free_expr(e->as.call.callee);
        for (i = 0; i < e->as.call.arguments.count; i++)
            free_expr(e->as.call.arguments.items[i]);
        free(e->as.call.arguments.items);
        break;
    case EXPR_INDEX:
        free_expr(e->as.index.base);
        free_expr(e->as.index.index);
        break;
    case EXPR_MEMBER:
        free_expr(e->as.member.base);
        free(e->as.member.name);
        break;
    case EXPR_CONDITIONAL:
        free_expr(e->as.conditional.condition);
        free_expr(e->as.conditional.then_expr);
        free_expr(e->as.conditional.else_expr);
        break;
    case EXPR_CAST:
        free_type(e->as.cast.target_type);
        free_decl(e->as.cast.enum_definition);
        free_expr(e->as.cast.operand);
        break;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        free_type(e->as.size_of.type);
        free_decl(e->as.size_of.enum_definition);
        free_expr(e->as.size_of.operand);
        break;
    default:
        break;
    }
    free(e);
}
static void free_stmt(Stmt *s);
/**
 * @brief Recursively releases a declaration and its owned children.
 * @param d Declaration node being analyzed or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void free_decl(Decl *d)
{
    size_t i;
    if (!d)
        return;
    release_alignment_specifiers(d->alignment_specifiers);
    if (d->kind == DECL_STRUCT_SCOPE) {
        free(d->as.structure.name);
    } else if (d->kind == DECL_STATIC_ASSERT) {
        free_expr(d->as.static_assertion.condition);
        free_expr(d->as.static_assertion.message);
    } else if (d->kind == DECL_ENUM) {
        free(d->as.enumeration.tag);
        for (i = 0; i < d->as.enumeration.count; ++i) {
            free(d->as.enumeration.items[i].name);
            free_expr(d->as.enumeration.items[i].value);
        }
        free(d->as.enumeration.items);
    } else if (d->kind == DECL_STRUCT) {
        free(d->as.structure.name);
        for (i = 0; i < d->as.structure.field_count; i++)
            free_decl(d->as.structure.fields[i]);
        free(d->as.structure.fields);
        for (i = 0; i < d->as.structure.assertion_count; i++)
            free_decl(d->as.structure.assertions[i]);
        free(d->as.structure.assertions);
    } else if (d->kind == DECL_VARIABLE) {
        free_type(d->as.variable.type);
        free(d->as.variable.name);
        free_expr(d->as.variable.initializer.expression);
        free_expr(d->as.variable.array_size);
        free_expr(d->as.variable.bit_field_width);
    } else if (d->kind == DECL_TYPE_ALIAS) {
        free_type(d->as.type_alias.type);
        free(d->as.type_alias.name);
    } else {
        free_type(d->as.function.return_type);
        free(d->as.function.name);
        for (i = 0; i < d->as.function.parameters.count; i++) {
            free_type(d->as.function.parameters.items[i].type);
            free(d->as.function.parameters.items[i].name);
            free_decl(d->as.function.parameters.items[i].enum_definition);
        }
        free(d->as.function.parameters.items);
        free_stmt(d->as.function.body);
    }
    free(d);
}
/**
 * @brief Recursively releases a statement and its owned children.
 * @param s Statement node being analyzed or emitted.
 * @return No value; the function performs the documented output or state update.
 */
static void free_stmt(Stmt *s)
{
    size_t i;
    if (!s)
        return;
    switch (s->kind) {
    case STMT_BLOCK:
        for (i = 0; i < s->as.block.statements.count; i++)
            free_stmt(s->as.block.statements.items[i]);
        free(s->as.block.statements.items);
        break;
    case STMT_DECLARATION:
        free_decl(s->as.declaration.declaration);
        break;
    case STMT_EXPRESSION:
        free_expr(s->as.expression.expression);
        break;
    case STMT_IF:
        free_expr(s->as.if_stmt.condition);
        free_stmt(s->as.if_stmt.then_branch);
        free_stmt(s->as.if_stmt.else_branch);
        break;
    case STMT_SWITCH:
        free_expr(s->as.switch_stmt.expression);
        for (i = 0; i < s->as.switch_stmt.prefix.count; i++)
            free_stmt(s->as.switch_stmt.prefix.items[i]);
        free(s->as.switch_stmt.prefix.items);
        for (i = 0; i < s->as.switch_stmt.cases.count; i++) {
            SwitchCase *c = &s->as.switch_stmt.cases.items[i];
            free_expr(c->value);
            for (size_t j = 0; j < c->statements.count; j++)
                free_stmt(c->statements.items[j]);
            free(c->statements.items);
        }
        free(s->as.switch_stmt.cases.items);
        break;
    case STMT_FOR:
        free_stmt(s->as.for_stmt.initializer);
        free_expr(s->as.for_stmt.condition);
        free_expr(s->as.for_stmt.update);
        free_stmt(s->as.for_stmt.body);
        break;
    case STMT_LOOP:
    case STMT_DO:
        free_expr(s->as.loop_stmt.condition);
        free_stmt(s->as.loop_stmt.body);
        break;
    case STMT_RETURN:
        free_expr(s->as.return_stmt.expression);
        break;
    case STMT_LABEL:
        free_stmt(s->as.label.statement);
        /* fall through */
    case STMT_GOTO:
        free(s->as.label.name);
        break;
    default:
        break;
    }
    free(s);
}
/**
 * @brief Releases an AST program and its owned declarations.
 * @param program AST program being analyzed, emitted, or destroyed.
 * @return No value; the function performs the documented output or state update.
 */
void ast_program_destroy(AstProgram *program)
{
    size_t i;
    if (!program)
        return;
    for (i = 0; i < program->declarations.count; i++)
        free_decl(program->declarations.items[i]);
    free(program->declarations.items);
    free(program->block_externals.items);
    free(program);
}
/**
 * @brief Compatibility wrapper that destroys an AST program.
 * @param program AST program being analyzed, emitted, or destroyed.
 * @return No value; the function performs the documented output or state update.
 */
void parser_destroy(AstProgram *program)
{
    ast_program_destroy(program);
}
