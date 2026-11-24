#ifndef CRUNCH_COMMON_RUNTIME_DECLARATION_H
#define CRUNCH_COMMON_RUNTIME_DECLARATION_H

#include "common/runtime_routines.h"
#include "parser/ast.h"

/**
 * @brief Compares runtime parameter pointer/integer classes with a declaration.
 * @param d Function declaration to inspect.
 * @return Nonzero if its fixed parameter classes match the runtime interface.
 */
static inline int crunch_runtime_arguments_match(const Decl *d) {
    const char *arguments = NULL;
#define RUNTIME_ARGUMENTS(text, argc, result, classes) \
    if (!strcmp(d->as.function.name, text)) arguments = classes;
    CRUNCH_RUNTIME_ROUTINES(RUNTIME_ARGUMENTS)
#undef RUNTIME_ARGUMENTS
    if (!arguments || strlen(arguments) != d->as.function.parameters.count)
        return 0;
    for (size_t i = 0; arguments[i]; ++i) {
        const Type *type = d->as.function.parameters.items[i].type;
        if (!type || ((type->kind == TYPE_POINTER) != (arguments[i] == 'p')))
            return 0;
    }
    return 1;
}

/**
 * @brief Identifies declarations that can use the private runtime call ABI.
 * @param d Function declaration to inspect.
 * @return Nonzero if its name and basic signature match a runtime routine.
 */
static inline int crunch_is_runtime_declaration(const Decl *d) {
    if (!d || d->kind != DECL_FUNCTION || d->is_extern || d->is_static ||
        d->as.function.body || !d->as.function.return_type)
        return 0;
    const char *name = d->as.function.name;
    size_t count = d->as.function.parameters.count;
    if (!strcmp(name, "printf") || !strcmp(name, "fprintf") ||
        !strcmp(name, "scanf") || !strcmp(name, "fscanf") ||
        !strcmp(name, "sprintf") || !strcmp(name, "snprintf")) {
        size_t index = !strcmp(name, "snprintf") ? 2 :
                       (!strcmp(name, "fprintf") || !strcmp(name, "fscanf") || !strcmp(name, "sprintf")) ? 1 : 0;
        if (count != index + 1 || d->as.function.return_type->kind != TYPE_INT)
            return 0;
        Type *format = d->as.function.parameters.items[index].type;
        if (!format || format->kind != TYPE_POINTER || !format->pointee ||
            format->pointee->kind != TYPE_BYTE)
            return 0;
        if (index == 1 && (!strcmp(name, "fprintf") || !strcmp(name, "fscanf"))) {
            Type *stream = d->as.function.parameters.items[0].type;
            return stream && stream->kind == TYPE_POINTER && stream->pointee &&
                   stream->pointee->kind == TYPE_STRUCT && stream->pointee->struct_name &&
                   !strcmp(stream->pointee->struct_name, "FILE");
        }
        if (!strcmp(name, "sprintf")) {
            Type *buffer = d->as.function.parameters.items[0].type;
            return buffer && buffer->kind == TYPE_POINTER && buffer->pointee &&
                   buffer->pointee->kind == TYPE_BYTE && !buffer->pointee->is_const;
        }
        if (index == 2) {
            Type *buffer = d->as.function.parameters.items[0].type;
            Type *capacity = d->as.function.parameters.items[1].type;
            return buffer && buffer->kind == TYPE_POINTER && buffer->pointee &&
                   buffer->pointee->kind == TYPE_BYTE && !buffer->pointee->is_const &&
                   capacity && (capacity->kind == TYPE_LONG ||
                                capacity->kind == TYPE_UNSIGNED_LONG);
        }
        return 1;
    }
    int expected = -1;
    int category = 0;
#define RUNTIME_SIGNATURE(text, argc, result, arguments) \
    if (!strcmp(name, text)) { expected = argc; category = result; }
    CRUNCH_RUNTIME_ROUTINES(RUNTIME_SIGNATURE)
#undef RUNTIME_SIGNATURE
    if (expected < 0 || count != (size_t)expected || d->as.function.variadic ||
        !crunch_runtime_arguments_match(d))
        return 0;
    TypeKind kind = d->as.function.return_type->kind;
    if (category == 'c' || category == 'n') {
        if (kind != TYPE_VOID) return 0;
    } else if (category == 'p') {
        if (kind != TYPE_POINTER) return 0;
    } else if (category == 'r') {
        if (kind != TYPE_REAL) return 0;
    } else if (category == 'f') {
        if (kind != TYPE_FLOAT) return 0;
    } else if (category == 'e') {
        if (kind != TYPE_LONG_DOUBLE) return 0;
    } else if (kind == TYPE_VOID || kind == TYPE_POINTER || kind == TYPE_STRUCT ||
               kind == TYPE_ARRAY || kind == TYPE_FUNCTION || kind == TYPE_REAL ||
               kind == TYPE_FLOAT || kind == TYPE_LONG_DOUBLE) {
        return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        Type *type = d->as.function.parameters.items[i].type;
        if (!type || type->kind == TYPE_STRUCT || type->kind == TYPE_REAL ||
            type->kind == TYPE_FLOAT || type->kind == TYPE_LONG_DOUBLE)
            return 0;
    }
    return 1;
}

/**
 * @brief Identifies runtime declarations with an ordinary System V call ABI.
 * @param d Function declaration to inspect.
 * @return Nonzero when an ordinary external call can use the runtime fallback.
 */
static inline int crunch_is_linkable_runtime_declaration(const Decl *d) {
    if (!crunch_is_runtime_declaration(d))
        return 0;
    const char *name = d->as.function.name;
    return strcmp(name, "printf") && strcmp(name, "fprintf") && strcmp(name, "scanf") &&
           strcmp(name, "fscanf") && strcmp(name, "sprintf") &&
           strcmp(name, "snprintf") && strcmp(name, "vfork");
}

/**
 * @brief Identifies external declarations incompatible with the known runtime ABI.
 * @param d Function declaration to inspect.
 * @return Nonzero when the runtime fallback must not satisfy this declaration.
 */
static inline int crunch_incompatible_runtime_declaration(const Decl *d) {
    if (!d || d->kind != DECL_FUNCTION || d->is_static || d->as.function.body ||
        !crunch_is_runtime_name(d->as.function.name))
        return 0;
    Decl candidate = *d;
    candidate.is_extern = false;
    if (!strcmp(candidate.as.function.name, "printf") ||
        !strcmp(candidate.as.function.name, "fprintf") || !strcmp(candidate.as.function.name, "scanf") ||
        !strcmp(candidate.as.function.name, "fscanf") || !strcmp(candidate.as.function.name, "sprintf") ||
        !strcmp(candidate.as.function.name, "snprintf"))
        candidate.as.function.variadic = false;
    return !crunch_is_runtime_declaration(&candidate);
}

#endif
