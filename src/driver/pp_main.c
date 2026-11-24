#define _POSIX_C_SOURCE 200809L
#include "common/identifier.h"
#include "version.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifndef CRUNCH_DEFAULT_INCLUDE_DIR
#define CRUNCH_DEFAULT_INCLUDE_DIR "include"
#endif

#ifndef CRUNCH_DEFAULT_CONF_FILE
#define CRUNCH_DEFAULT_CONF_FILE "/etc/crunch/crunch.conf"
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & 0170000) == 0040000)
#endif

enum { PP_MAX_INCLUDE_DEPTH = 64 };

typedef struct {
    char *path;
    char *macro;
} HeaderGuard;

typedef struct {
    char *path;
    unsigned long device;
    unsigned long inode;
    int stat_ok;
} IncludeDirectory;

typedef struct {
    unsigned long device;
    unsigned long inode;
} OnceFile;

typedef struct {
    char *name;
    char **parameters;
    size_t parameter_count;
    char *replacement;
    int function_like;
    int variadic;
} MacroDefinition;

typedef struct {
    size_t *offsets;
    size_t count;
    size_t capacity;
} SpliceOffsets;

typedef struct {
    FILE *out;
    int assembly;
    const char *include_dir;
    const char *include_path;
    char *conf_include_path;
    const char **cli_dirs;
    size_t cli_dir_count;
    const char **system_dirs;
    size_t system_dir_count;
    const char *stack[PP_MAX_INCLUDE_DEPTH];
    size_t origins[PP_MAX_INCLUDE_DEPTH];
    size_t depth;
    IncludeDirectory *search_dirs;
    size_t search_dir_count;
    char **macros;
    size_t macro_count;
    MacroDefinition *definitions;
    size_t definition_count;
    HeaderGuard *guards;
    size_t guard_count;
    OnceFile *once_files;
    size_t once_count;
    unsigned int pack_alignment;
    unsigned int *pack_stack;
    size_t pack_depth;
    size_t pack_capacity;
    const char *source_path;
    size_t source_line;
    int if_expression;
    unsigned if_argument_depth;
    unsigned feature_query_depth;
    int track_source_lines;
    int emit_source_markers;
    const char *source_date_epoch;
    int timestamp_ready;
    char date_literal[12];
    char time_literal[9];
    const char *mapped_text;
    size_t mapped_length;
    const SpliceOffsets *mapped_splices;
} Preprocessor;

typedef enum {
    DIRECTIVE_NONE,
    DIRECTIVE_INCLUDE,
    DIRECTIVE_INCLUDE_NEXT,
    DIRECTIVE_IF,
    DIRECTIVE_ELIF,
    DIRECTIVE_IFDEF,
    DIRECTIVE_IFNDEF,
    DIRECTIVE_DEFINE,
    DIRECTIVE_DEFINE_VALUE,
    DIRECTIVE_UNDEF,
    DIRECTIVE_MACRO,
    DIRECTIVE_ERROR,
    DIRECTIVE_WARNING,
    DIRECTIVE_PRAGMA_ONCE,
    DIRECTIVE_PRAGMA_PACK,
    DIRECTIVE_PRAGMA_DIAGNOSTIC,
    DIRECTIVE_PRAGMA_GCC_OPTIONS,
    DIRECTIVE_LINE,
    DIRECTIVE_ELSE,
    DIRECTIVE_ENDIF,
    DIRECTIVE_OTHER,
    DIRECTIVE_UNSUPPORTED,
    DIRECTIVE_INVALID
} DirectiveKind;

typedef struct {
    DirectiveKind kind;
    const char *argument;
    size_t argument_length;
} Directive;

typedef struct {
    int parent_active;
    int branch_taken;
    int else_seen;
} Conditional;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} TextBuffer;

typedef struct {
    const char *text;
    size_t length;
} MacroArgument;

enum { PP_MAX_MACRO_EXPANSION_DEPTH = 64 };

/**
 * @brief Copies a byte range into a newly allocated NUL-terminated string.
 * @param text Source bytes to copy.
 * @param length Number of bytes to copy.
 * @return Allocated string, or NULL if allocation fails.
 */
static char *copy_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

/**
 * @brief Reads one dynamically sized line from an input stream.
 * @param input Stream to read.
 * @param result Output receiving the allocated, NUL-terminated line.
 * @param length Output receiving the line length, excluding its NUL terminator.
 * @return One for a line, zero at clean end-of-file, or -1 on I/O/allocation failure.
 */
static int read_line(FILE *input, char **result, size_t *length)
{
    size_t capacity = 128, used = 0;
    char *line = malloc(capacity);
    if (!line) return -1;
    for (;;) {
        int ch = fgetc(input);
        if (ch == EOF) {
            if (ferror(input)) {
                free(line);
                return -1;
            }
            if (!used) {
                free(line);
                return 0;
            }
            break;
        }
        if (used + 1 >= capacity) {
            if (capacity > (size_t)-1 / 2) {
                free(line);
                return -1;
            }
            capacity *= 2;
            char *grown = realloc(line, capacity);
            if (!grown) {
                free(line);
                return -1;
            }
            line = grown;
        }
        line[used++] = (char)ch;
        if (ch == '\n') break;
    }
    line[used] = '\0';
    *length = used;
    *result = line;
    return 1;
}

/**
 * @brief Skips spaces, tabs, and line-ending characters.
 * @param text Text to scan.
 * @return Pointer to the first non-whitespace character or the terminating NUL.
 */
static const char *skip_space(const char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        ++text;
    return text;
}

/**
 * @brief Checks whether the remainder contains only whitespace and comments.
 * @param text Text to inspect.
 * @return Nonzero if no tokens follow the whitespace/comment; otherwise zero.
 */
static int trailing_comment_only(const char *text)
{
    for (;;) {
        text = skip_space(text);
        if (!*text || (text[0] == '/' && text[1] == '/')) return 1;
        if (text[0] != '/' || text[1] != '*') return 0;
        const char *end = strstr(text + 2, "*/");
        if (!end) return 1;
        text = end + 2;
    }
}

/**
 * @brief Updates block-comment state while ignoring comment markers in quotes.
 * @param cursor First character in the source range.
 * @param end One-past-last character in the source range.
 * @param in_block_comment Input/output flag indicating block-comment state.
 * @param hash_comments Whether an unquoted # begins a line comment.
 * @return No value; updates the supplied state for the end of the range.
 */
static void update_block_comment_state(const char *cursor, const char *end,
                                       int *in_block_comment, int hash_comments)
{
    char quote = '\0';
    while (cursor < end) {
        if (*in_block_comment) {
            if (end - cursor >= 2 && cursor[0] == '*' && cursor[1] == '/') {
                *in_block_comment = 0;
                cursor += 2;
            } else {
                ++cursor;
            }
        } else if (quote) {
            if (*cursor == '\\' && cursor + 1 < end) cursor += 2;
            else {
                if (*cursor == quote) quote = '\0';
                ++cursor;
            }
        } else if ((hash_comments && *cursor == '#') ||
                   (end - cursor >= 2 && cursor[0] == '/' && cursor[1] == '/')) {
            return;
        } else if (end - cursor >= 2 && cursor[0] == '/' && cursor[1] == '*') {
            *in_block_comment = 1;
            cursor += 2;
        } else {
            if (*cursor == '"' || *cursor == '\'') quote = *cursor;
            ++cursor;
        }
    }
}

/**
 * @brief Validates a complete identifier within a byte range.
 * @param text Identifier bytes to inspect.
 * @param length Number of bytes in the range.
 * @return Nonzero for a valid identifier; otherwise zero.
 */
static int valid_identifier(const char *text, size_t length)
{
    if (!length || !crunch_identifier_start((unsigned char)text[0]))
        return 0;
    for (size_t i = 1; i < length; ++i)
        if (!crunch_identifier_continue((unsigned char)text[i])) return 0;
    return 1;
}

/**
 * @brief Rejects absolute or empty-component include names.
 * @param name Header-name bytes to validate.
 * @param length Number of bytes in the name.
 * @return Nonzero for a nonempty relative path with no empty components.
 */
static int safe_header_name(const char *name, size_t length)
{
    size_t start = 0;
    if (!length || name[0] == '/') return 0;
    for (size_t i = 0; i <= length; ++i) {
        if (i != length && name[i] != '/') continue;
        size_t component = i - start;
        if (!component) return 0;
        start = i + 1;
    }
    return 1;
}

/**
 * @brief Checks whether a macro with the given name is defined.
 * @param pp Preprocessor state containing defined macro names.
 * @param name Macro-name bytes to search for.
 * @param length Number of bytes in name.
 * @return Nonzero if a matching macro exists; otherwise zero.
 */
static int macro_defined(const Preprocessor *pp, const char *name, size_t length)
{
    for (size_t i = 0; i < pp->macro_count; ++i)
        if (strlen(pp->macros[i]) == length &&
            memcmp(pp->macros[i], name, length) == 0) return 1;
    return 0;
}

/**
 * @brief Records a macro name unless it is already defined.
 * @param pp Preprocessor state whose macro list is updated.
 * @param name Macro-name bytes to copy.
 * @param length Number of bytes in name.
 * @return Nonzero on success, including when already defined; zero on allocation failure.
 */
static int define_macro(Preprocessor *pp, const char *name, size_t length)
{
    if (macro_defined(pp, name, length)) return 1;
    if (pp->macro_count >= SIZE_MAX / sizeof(*pp->macros)) return 0;
    char *copy = copy_text(name, length);
    if (!copy) return 0;
    char **grown = realloc(pp->macros,
                           (pp->macro_count + 1) * sizeof(*pp->macros));
    if (!grown) {
        free(copy);
        return 0;
    }
    pp->macros = grown;
    pp->macros[pp->macro_count++] = copy;
    return 1;
}

/**
 * @brief Removes a macro name and its replacement definition if present.
 * @param pp Preprocessor state whose macro tables are updated.
 * @param name Macro-name bytes to remove.
 * @param length Number of bytes in name.
 * @return No value; absent macro names are ignored.
 */
static void undefine_macro(Preprocessor *pp, const char *name, size_t length)
{
    for (size_t i = 0; i < pp->macro_count; ++i) {
        if (strlen(pp->macros[i]) != length ||
            memcmp(pp->macros[i], name, length) != 0)
            continue;
        free(pp->macros[i]);
        memmove(&pp->macros[i], &pp->macros[i + 1],
                (pp->macro_count - i - 1) * sizeof(*pp->macros));
        --pp->macro_count;
        break;
    }
    for (size_t i = 0; i < pp->definition_count; ++i) {
        MacroDefinition *definition = &pp->definitions[i];
        if (strlen(definition->name) != length ||
            memcmp(definition->name, name, length) != 0)
            continue;
        free(definition->name);
        for (size_t j = 0; j < definition->parameter_count; ++j)
            free(definition->parameters[j]);
        free(definition->parameters);
        free(definition->replacement);
        memmove(definition, definition + 1,
                (pp->definition_count - i - 1) * sizeof(*definition));
        --pp->definition_count;
        break;
    }
}

/**
 * @brief Looks up a replacement macro by its complete name.
 * @param pp Preprocessor containing the replacement definitions.
 * @param name Name bytes to match.
 * @param length Number of bytes in name.
 * @return Borrowed definition, or NULL if absent; table mutations may invalidate it.
 */
static MacroDefinition *find_macro_definition(Preprocessor *pp,
                                             const char *name, size_t length)
{
    for (size_t i = 0; i < pp->definition_count; ++i)
        if (strlen(pp->definitions[i].name) == length &&
            memcmp(pp->definitions[i].name, name, length) == 0)
            return &pp->definitions[i];
    return NULL;
}

/**
 * @brief Appends bytes to a dynamically growing, NUL-terminated text buffer.
 * @param buffer Buffer to extend.
 * @param text Bytes to append.
 * @param length Number of bytes to append.
 * @return Nonzero on success; zero if size overflows or allocation fails.
 */
static int buffer_append(TextBuffer *buffer, const char *text, size_t length)
{
    if (length > (size_t)-1 - buffer->length - 1) return 0;
    size_t needed = buffer->length + length + 1;
    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 128;
        while (capacity < needed) {
            if (capacity > (size_t)-1 / 2) {
                capacity = needed;
                break;
            }
            capacity *= 2;
        }
        char *grown = realloc(buffer->data, capacity);
        if (!grown) return 0;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

/**
 * @brief Appends one character to a text buffer.
 * @param buffer Buffer to extend.
 * @param ch Character to append.
 * @return Nonzero on success; zero if allocation fails.
 */
static int buffer_append_char(TextBuffer *buffer, char ch)
{
    return buffer_append(buffer, &ch, 1);
}

/**
 * @brief Measures an identifier beginning at a specified source offset.
 * @param text Source bytes to scan.
 * @param length Total number of bytes in text.
 * @param offset Starting byte offset.
 * @param identifier_length Output receiving the identifier length on success.
 * @return One if an identifier starts at offset; zero without setting the output otherwise.
 */
static int identifier_at(const char *text, size_t length, size_t offset,
                         size_t *identifier_length)
{
    if (offset >= length ||
        !crunch_identifier_start((unsigned char)text[offset]))
        return 0;
    size_t end = offset + 1;
    while (end < length &&
           crunch_identifier_continue((unsigned char)text[end]))
        ++end;
    *identifier_length = end - offset;
    return 1;
}

/**
 * @brief Measures a preprocessing number so macro names inside it stay intact.
 * @param text Source bytes to scan.
 * @param length Number of source bytes.
 * @param offset Starting byte offset.
 * @return Number of bytes in the preprocessing number, or zero.
 */
static size_t pp_number_length(const char *text, size_t length, size_t offset)
{
    if (offset >= length ||
        (!isdigit((unsigned char)text[offset]) &&
         !(text[offset] == '.' && offset + 1 < length &&
           isdigit((unsigned char)text[offset + 1]))))
        return 0;
    size_t end = offset + 1;
    while (end < length) {
        unsigned char ch = (unsigned char)text[end];
        if (crunch_identifier_continue(ch) || ch == '.') {
            ++end;
        } else if ((ch == '+' || ch == '-') &&
                   (text[end - 1] == 'e' || text[end - 1] == 'E' ||
                    text[end - 1] == 'p' || text[end - 1] == 'P')) {
            ++end;
        } else {
            break;
        }
    }
    return end - offset;
}

/**
 * @brief Copies a quoted span without interpreting escapes or expanding macros.
 * @param output Buffer receiving the span.
 * @param text Source bytes, with a quote at the supplied offset.
 * @param length Total number of source bytes.
 * @param offset Input/output cursor advanced past the closing quote or to end of text.
 * @return One on success, or zero on buffer growth failure; no cursor update on failure.
 */
static int append_quoted_text(TextBuffer *output, const char *text,
                              size_t length, size_t *offset)
{
    char quote = text[*offset];
    size_t end = *offset + 1;
    while (end < length) {
        if (text[end] == '\\' && end + 1 < length) {
            end += 2;
            continue;
        }
        if (text[end++] == quote) break;
    }
    if (!buffer_append(output, text + *offset, end - *offset)) return 0;
    *offset = end;
    return 1;
}

/**
 * @brief Compares replacement text, treating comments as whitespace.
 * @param left First replacement list.
 * @param right Second replacement list.
 * @return Nonzero if the lists and their whitespace separation match.
 */
static int same_replacement(const char *left, const char *right)
{
    int first = 1;
    for (;;) {
        int left_space = 0, right_space = 0;
        const char **sides[2] = {&left, &right};
        int *spaces[2] = {&left_space, &right_space};
        for (int i = 0; i < 2; ++i) {
            const char *p = *sides[i];
            for (;;) {
                if (isspace((unsigned char)*p)) {
                    *spaces[i] = 1;
                    ++p;
                } else if (p[0] == '/' && p[1] == '*') {
                    const char *end = strstr(p + 2, "*/");
                    if (!end) return 0;
                    *spaces[i] = 1;
                    p = end + 2;
                } else if (p[0] == '/' && p[1] == '/') {
                    *spaces[i] = 1;
                    p += strlen(p);
                    break;
                } else {
                    break;
                }
            }
            *sides[i] = p;
        }
        if (!*left || !*right) return !*left && !*right;
        if (!first && left_space != right_space) return 0;
        first = 0;
        if (*left != *right) return 0;
        if (*left == '"' || *left == '\'') {
            char quote = *left++;
            int closed = 0;
            ++right;
            while (*left && *right && *left == *right) {
                char ch = *left++;
                ++right;
                if (ch == '\\') {
                    if (!*left || *left != *right) return 0;
                    ++left;
                    ++right;
                } else if (ch == quote) {
                    closed = 1;
                    break;
                }
            }
            if (!closed) return 0;
        } else {
            ++left;
            ++right;
        }
    }
}

/**
 * @brief Parses and records an object-like or function-like replacement macro.
 * @param pp Preprocessor that owns the copied definition and records its name.
 * @param text Definition bytes following #macro or #define.
 * @param length Number of bytes in text.
 * @return One on success; zero for malformed/duplicate definitions or allocation failure.
 * Failure may leave the name recorded as defined; callers stop preprocessing on failure.
 */
static int parse_macro_definition(Preprocessor *pp, const char *text,
                                  size_t length)
{
    while (length && isspace((unsigned char)*text)) {
        ++text;
        --length;
    }
    size_t name_length = 0;
    if (!identifier_at(text, length, 0, &name_length)) return 0;
    const char *cursor = text + name_length;
    const char *end = text + length;
    int function_like = cursor < end && *cursor == '(';
    int variadic = 0;
    char **parameters = NULL;
    size_t parameter_count = 0;
    if (function_like) {
        ++cursor;
        while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
        if (cursor < end && *cursor != ')') {
            for (;;) {
                const char *parameter = cursor;
                size_t parameter_length;
                if (end - cursor >= 3 && !memcmp(cursor, "...", 3)) {
                    parameter = "__VA_ARGS__";
                    parameter_length = strlen(parameter);
                    cursor += 3;
                    variadic = 1;
                } else {
                    while (cursor < end &&
                           crunch_identifier_continue((unsigned char)*cursor))
                        ++cursor;
                    parameter_length = (size_t)(cursor - parameter);
                }
                if (!valid_identifier(parameter, parameter_length)) {
                    fprintf(stderr, "crunch-pp: malformed parameter list for macro '%.*s'\n",
                            (int)name_length, text);
                    goto fail;
                }
                for (size_t i = 0; i < parameter_count; ++i) {
                    if (strlen(parameters[i]) == parameter_length &&
                        memcmp(parameters[i], parameter, parameter_length) == 0) {
                        fprintf(stderr, "crunch-pp: duplicate parameter '%.*s' in macro '%.*s'\n",
                                (int)parameter_length, parameter,
                                (int)name_length, text);
                        goto fail;
                    }
                }
                if (parameter_count >= SIZE_MAX / sizeof(*parameters)) goto fail;
                char *parameter_copy = copy_text(parameter, parameter_length);
                if (!parameter_copy) goto fail;
                char **grown = realloc(parameters,
                                       (parameter_count + 1) * sizeof(*parameters));
                if (!grown) {
                    free(parameter_copy);
                    goto fail;
                }
                parameters = grown;
                parameters[parameter_count++] = parameter_copy;
                while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
                if (!variadic && end - cursor >= 3 && !memcmp(cursor, "...", 3)) {
                    variadic = 1;
                    cursor += 3;
                    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
                }
                if (cursor < end && *cursor == ',') {
                    if (variadic) {
                        fprintf(stderr, "crunch-pp: variadic parameter must be last in macro '%.*s'\n",
                                (int)name_length, text);
                        goto fail;
                    }
                    ++cursor;
                    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
                    continue;
                }
                break;
            }
        }
        if (cursor >= end || *cursor != ')') {
            fprintf(stderr, "crunch-pp: unterminated parameter list for macro '%.*s'\n",
                    (int)name_length, text);
            goto fail;
        }
        ++cursor;
    } else {
        while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
    }
    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
    while (end > cursor && (end[-1] == '\n' || end[-1] == '\r')) --end;
    char quote = '\0';
    for (const char *p = cursor; p < end; ++p) {
        if (quote) {
            if (*p == '\\' && p + 1 < end) ++p;
            else if (*p == quote) quote = '\0';
        } else if (*p == '"' || *p == '\'') {
            quote = *p;
        } else if (p + 1 < end && p[0] == '/' && p[1] == '/') {
            end = p;
            break;
        } else if (p + 1 < end && p[0] == '/' && p[1] == '*') {
            const char *comment = p;
            p += 2;
            while (p < end && !(p[0] == '*' && p + 1 < end && p[1] == '/')) ++p;
            if (p == end) {
                end = comment;
                break;
            }
            ++p;
        }
    }

    MacroDefinition definition = {0};
    definition.name = copy_text(text, name_length);
    definition.parameters = parameters;
    definition.parameter_count = parameter_count;
    definition.replacement = copy_text(cursor, (size_t)(end - cursor));
    definition.function_like = function_like;
    definition.variadic = variadic;
    if (macro_defined(pp, text, name_length)) {
        MacroDefinition *existing = find_macro_definition(pp, text, name_length);
        int same = existing && definition.replacement &&
                   existing->function_like == function_like &&
                   existing->variadic == variadic &&
                   existing->parameter_count == parameter_count &&
                   same_replacement(existing->replacement, definition.replacement);
        for (size_t i = 0; same && i < parameter_count; ++i)
            if (strcmp(existing->parameters[i], parameters[i])) same = 0;
        free(definition.name);
        free(definition.replacement);
        for (size_t i = 0; i < parameter_count; ++i) free(parameters[i]);
        free(parameters);
        if (!same)
            fprintf(stderr, "crunch-pp: macro '%.*s' is already defined\n",
                    (int)name_length, text);
        return same;
    }
    if (!definition.name || !definition.replacement ||
        !define_macro(pp, text, name_length) ||
        pp->definition_count >= SIZE_MAX / sizeof(*pp->definitions)) {
        free(definition.name);
        free(definition.replacement);
        for (size_t i = 0; i < parameter_count; ++i) free(parameters[i]);
        free(parameters);
        return 0;
    }
    MacroDefinition *grown = realloc(pp->definitions,
                                     (pp->definition_count + 1) *
                                     sizeof(*pp->definitions));
    if (!grown) {
        free(definition.name);
        free(definition.replacement);
        for (size_t i = 0; i < parameter_count; ++i) free(parameters[i]);
        free(parameters);
        return 0;
    }
    pp->definitions = grown;
    pp->definitions[pp->definition_count++] = definition;
    return 1;

fail:
    for (size_t i = 0; i < parameter_count; ++i) free(parameters[i]);
    free(parameters);
    return 0;
}

/**
 * @brief Finds the include-guard macro registered for a header path.
 * @param pp Preprocessor state containing registered guards.
 * @param path Header path to search for.
 * @return Guard macro name, or NULL if no guard is registered.
 */
static const char *guard_for_path(const Preprocessor *pp, const char *path)
{
    for (size_t i = 0; i < pp->guard_count; ++i)
        if (strcmp(pp->guards[i].path, path) == 0) return pp->guards[i].macro;
    return NULL;
}

/**
 * @brief Records an include-guard macro for a header path.
 * @param pp Preprocessor that owns the copied path and macro name.
 * @param path NUL-terminated header path.
 * @param macro Guard macro name bytes.
 * @param length Number of bytes in macro.
 * @return One on success or if path is already registered; zero on allocation failure.
 */
static int register_guard(Preprocessor *pp, const char *path,
                          const char *macro, size_t length)
{
    if (guard_for_path(pp, path)) return 1;
    if (pp->guard_count >= SIZE_MAX / sizeof(*pp->guards)) return 0;
    char *path_copy = copy_text(path, strlen(path));
    char *macro_copy = copy_text(macro, length);
    if (!path_copy || !macro_copy) {
        free(path_copy);
        free(macro_copy);
        return 0;
    }
    HeaderGuard *grown = realloc(pp->guards,
                                 (pp->guard_count + 1) * sizeof(*pp->guards));
    if (!grown) {
        free(path_copy);
        free(macro_copy);
        return 0;
    }
    pp->guards = grown;
    pp->guards[pp->guard_count++] = (HeaderGuard){path_copy, macro_copy};
    return 1;
}

/**
 * @brief Encodes a byte as a fixed-width octal escape.
 * @param value Byte to encode.
 * @param out Buffer receiving the escape and terminator.
 * @return No value.
 */
static void octal_escape(unsigned char value, char out[5])
{
    out[0] = '\\';
    out[1] = '0' + (value >> 6);
    out[2] = '0' + ((value >> 3) & 7);
    out[3] = '0' + (value & 7);
    out[4] = '\0';
}

/**
 * @brief Writes a source-location marker for the lexer.
 * @param pp Preprocessor state supplying the output stream.
 * @param path Source path encoded in the marker.
 * @param line One-based source line to associate with following text.
 * @return Nonzero if the complete marker was written; otherwise zero.
 */
static int write_marker(Preprocessor *pp, const char *path, size_t line)
{
    if (pp->assembly) {
        if (fprintf(pp->out, "# %zu \"", line) < 0) return 0;
        for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
            if (*p == '"' || *p == '\\') {
                if (fputc('\\', pp->out) == EOF || fputc(*p, pp->out) == EOF) return 0;
            } else if (*p < 32 || *p >= 127) {
                char escaped[5];
                octal_escape(*p, escaped);
                if (fputs(escaped, pp->out) == EOF) return 0;
            } else if (fputc(*p, pp->out) == EOF) {
                return 0;
            }
        }
        return fputs("\"\n", pp->out) != EOF;
    }
    if (fprintf(pp->out, "/*CRUNCH-PP-MAP:%zu:", line) < 0) return 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p)
        if (fprintf(pp->out, "%02x", *p) < 0) return 0;
    return fputs("*/\n", pp->out) != EOF;
}

static size_t skip_macro_space(const char *text, size_t length, size_t cursor);

/* Source NUL bytes are rejected, so NUL can represent an internal boundary. */
#define PP_TOKEN_BOUNDARY '\0'

/**
 * @brief Recognizes GCC diagnostic controls for the unsupported pedantic warning category.
 * @param line Directive text.
 * @param length Number of directive bytes.
 * @param cursor Offset of the pragma name.
 * @return Nonzero for supported controls; zero for other pragmas.
 */
static int pragma_pedantic_control(const char *line, size_t length, size_t cursor)
{
    const char *words[] = {"GCC", "diagnostic"};
    for (size_t i = 0; i < sizeof words / sizeof words[0]; ++i) {
        size_t word_length = 0;
        cursor = skip_macro_space(line, length, cursor);
        if (!identifier_at(line, length, cursor, &word_length) ||
            word_length != strlen(words[i]) ||
            memcmp(line + cursor, words[i], word_length))
            return 0;
        cursor += word_length;
    }
    size_t action_length = 0;
    cursor = skip_macro_space(line, length, cursor);
    if (!identifier_at(line, length, cursor, &action_length)) return 0;
    const char *action = line + cursor;
    cursor += action_length;
    if ((action_length == 4 && !memcmp(action, "push", 4)) ||
        (action_length == 3 && !memcmp(action, "pop", 3)))
        return trailing_comment_only(line + cursor);
    if (action_length != 7 || memcmp(action, "ignored", 7)) return 0;
    cursor = skip_macro_space(line, length, cursor);
    const char *options[] = {"\"-Wpedantic\"", "\"-pedantic\""};
    for (size_t i = 0; i < sizeof options / sizeof options[0]; ++i) {
        size_t option_length = strlen(options[i]);
        if (option_length <= length - cursor &&
            !memcmp(line + cursor, options[i], option_length) &&
            trailing_comment_only(line + cursor + option_length))
            return 1;
    }
    return 0;
}

/**
 * @brief Identifies GCC target and optimization option pragmas.
 * @param line Directive text.
 * @param length Number of directive bytes.
 * @param cursor Offset of the pragma name.
 * @return Nonzero for an option pragma; zero otherwise.
 */
static int pragma_gcc_options(const char *line, size_t length, size_t cursor)
{
    size_t word_length = 0;
    if (!identifier_at(line, length, cursor, &word_length) ||
        word_length != 3 || memcmp(line + cursor, "GCC", 3))
        return 0;
    cursor = skip_macro_space(line, length, cursor + word_length);
    if (!identifier_at(line, length, cursor, &word_length))
        return 0;
    const char *names[] = {"target", "optimize", "push_options", "pop_options", "reset_options"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (word_length == strlen(names[i]) && !memcmp(line + cursor, names[i], word_length))
            return 1;
    return 0;
}

/**
 * @brief Parses a preprocessor directive and exposes its argument range.
 * @param line Directive text beginning with '#'.
 * @param length Number of bytes in line.
 * @return Parsed directive kind and a borrowed argument slice into line.
 */
static Directive parse_directive_text(const char *line, size_t length)
{
    Directive result = {DIRECTIVE_NONE, NULL, 0};
    const char *cursor = line;
    const char *word;
    size_t word_length;
    if (*cursor != '#') return result;
    if (strlen(cursor) != (size_t)(line + length - cursor)) {
        result.kind = DIRECTIVE_INVALID;
        return result;
    }
    word = line + skip_macro_space(line, length, 1);
    cursor = word;
    if (crunch_identifier_start((unsigned char)*cursor)) {
        ++cursor;
        while (crunch_identifier_continue((unsigned char)*cursor)) ++cursor;
    }
    word_length = (size_t)(cursor - word);
    cursor = line + skip_macro_space(line, length, (size_t)(cursor - line));
    if ((word_length == 7 && memcmp(word, "include", 7) == 0) ||
        (word_length == 12 && memcmp(word, "include_next", 12) == 0)) {
        result.kind = word_length == 7 ? DIRECTIVE_INCLUDE : DIRECTIVE_INCLUDE_NEXT;
        result.argument = cursor;
        result.argument_length = (size_t)(line + length - cursor);
    } else if ((word_length == 5 && memcmp(word, "ifdef", 5) == 0) ||
               (word_length == 6 && memcmp(word, "ifndef", 6) == 0)) {
        int is_ifdef = word_length == 5;
        const char *arg = cursor;
        if (crunch_identifier_start((unsigned char)*cursor)) {
            ++cursor;
            while (crunch_identifier_continue((unsigned char)*cursor)) ++cursor;
        }
        result.argument_length = (size_t)(cursor - arg);
        if (!valid_identifier(arg, result.argument_length) ||
            !trailing_comment_only(cursor)) {
            result.kind = DIRECTIVE_INVALID;
            return result;
        }
        result.argument = arg;
        result.kind = is_ifdef ? DIRECTIVE_IFDEF : DIRECTIVE_IFNDEF;
    } else if ((word_length == 2 && memcmp(word, "if", 2) == 0) ||
               (word_length == 4 && memcmp(word, "elif", 4) == 0)) {
        result.kind = word_length == 2 ? DIRECTIVE_IF : DIRECTIVE_ELIF;
        result.argument = cursor;
        result.argument_length = (size_t)(line + length - cursor);
    } else if (word_length == 6 && memcmp(word, "define", 6) == 0) {
        const char *arg = cursor;
        if (!crunch_identifier_start((unsigned char)*cursor)) {
            result.kind = DIRECTIVE_INVALID;
            return result;
        }
        while (crunch_identifier_continue((unsigned char)*cursor)) ++cursor;
        result.argument_length = (size_t)(cursor - arg);
        if (*cursor && *cursor != '(' &&
            !(cursor[0] == '/' && (cursor[1] == '/' || cursor[1] == '*')) &&
            !isspace((unsigned char)*cursor)) {
            result.kind = DIRECTIVE_INVALID;
            return result;
        }
        result.argument = arg;
        if (*cursor != '(' && trailing_comment_only(cursor)) {
            result.kind = DIRECTIVE_DEFINE;
        } else {
            result.kind = DIRECTIVE_DEFINE_VALUE;
            result.argument_length = (size_t)(line + length - arg);
        }
    } else if (word_length == 5 && memcmp(word, "undef", 5) == 0) {
        const char *arg = cursor;
        if (crunch_identifier_start((unsigned char)*cursor)) {
            ++cursor;
            while (crunch_identifier_continue((unsigned char)*cursor)) ++cursor;
        }
        result.argument_length = (size_t)(cursor - arg);
        if (!valid_identifier(arg, result.argument_length) ||
            !trailing_comment_only(cursor)) {
            result.kind = DIRECTIVE_INVALID;
            return result;
        }
        result.argument = arg;
        result.kind = DIRECTIVE_UNDEF;
    } else if (word_length == 5 && memcmp(word, "macro", 5) == 0) {
        result.argument = cursor;
        result.argument_length = (size_t)(line + length - cursor);
        result.kind = DIRECTIVE_MACRO;
    } else if (word_length == 4 && memcmp(word, "else", 4) == 0) {
        result.kind = trailing_comment_only(cursor) ? DIRECTIVE_ELSE : DIRECTIVE_INVALID;
    } else if (word_length == 5 && memcmp(word, "endif", 5) == 0) {
        result.kind = trailing_comment_only(cursor) ? DIRECTIVE_ENDIF : DIRECTIVE_INVALID;
    } else if ((word_length == 5 && memcmp(word, "error", 5) == 0) ||
               (word_length == 7 && memcmp(word, "warning", 7) == 0) ||
               (word_length == 4 && memcmp(word, "line", 4) == 0)) {
        result.kind = word_length == 5 ? DIRECTIVE_ERROR :
                      word_length == 7 ? DIRECTIVE_WARNING : DIRECTIVE_LINE;
        result.argument = cursor;
        result.argument_length = (size_t)(line + length - cursor);
    } else if (word_length == 6 && memcmp(word, "pragma", 6) == 0) {
        result.kind = DIRECTIVE_UNSUPPORTED;
        size_t start = skip_macro_space(line, length, (size_t)(cursor - line));
        size_t name_length = 0;
        if (identifier_at(line, length, start, &name_length) &&
            name_length == 4 && !memcmp(line + start, "once", 4)) {
            result.kind = DIRECTIVE_PRAGMA_ONCE;
            result.argument = line + start + name_length;
            result.argument_length = length - start - name_length;
        } else if (name_length == 4 && !memcmp(line + start, "pack", 4)) {
            result.kind = DIRECTIVE_PRAGMA_PACK;
            result.argument = line + start + name_length;
            result.argument_length = length - start - name_length;
        } else if (pragma_pedantic_control(line, length, start)) {
            result.kind = DIRECTIVE_PRAGMA_DIAGNOSTIC;
        } else if (pragma_gcc_options(line, length, start)) {
            result.kind = DIRECTIVE_PRAGMA_GCC_OPTIONS;
            size_t action = skip_macro_space(line, length, start + 3);
            result.argument = line + action;
            result.argument_length = length - action;
        }
    } else {
        result.kind = DIRECTIVE_OTHER;
    }
    return result;
}

/**
 * @brief Recognizes a directive after leading whitespace and comments.
 * @param line NUL-terminated source line.
 * @param length Number of bytes before the terminating NUL.
 * @param in_block_comment Input/output state carried between source lines.
 * @param has_code Output flag set when non-directive code follows leading comments.
 * @param assembly Whether to recognize assembler # comments.
 * @return Directive with a borrowed argument slice, or DIRECTIVE_NONE for ordinary text.
 */
static Directive parse_directive(const char *line, size_t length,
                                 int *in_block_comment, int *has_code, int assembly)
{
    const char *cursor = line;
    const char *end = line + length;
    *has_code = 0;
    for (;;) {
        while (cursor < end &&
               isspace((unsigned char)*cursor)) ++cursor;
        if (*in_block_comment) {
            while (cursor < end &&
                   !(end - cursor >= 2 && cursor[0] == '*' && cursor[1] == '/'))
                ++cursor;
            if (cursor == end) return (Directive){DIRECTIVE_NONE, NULL, 0};
            *in_block_comment = 0;
            cursor += 2;
            continue;
        }
        if (cursor == end ||
            (end - cursor >= 2 && cursor[0] == '/' && cursor[1] == '/'))
            return (Directive){DIRECTIVE_NONE, NULL, 0};
        if (end - cursor >= 2 && cursor[0] == '/' && cursor[1] == '*') {
            *in_block_comment = 1;
            cursor += 2;
            continue;
        }
        break;
    }
    if (*cursor == '#') {
        Directive directive = parse_directive_text(
            cursor, (size_t)(end - cursor));
        if (assembly && directive.kind == DIRECTIVE_OTHER)
            return (Directive){DIRECTIVE_NONE, NULL, 0};
        update_block_comment_state(line, end, in_block_comment, 0);
        return directive;
    }
    *has_code = 1;
    update_block_comment_state(cursor, end, in_block_comment, assembly);
    return (Directive){DIRECTIVE_NONE, NULL, 0};
}

/**
 * @brief Finds the backslash-newline suffix of a physical line.
 * @param line Source bytes.
 * @param length Number of source bytes.
 * @return Suffix length, including backslash and LF or CRLF, or zero.
 */
static size_t continuation_suffix(const char *line, size_t length)
{
    if (length >= 2 && line[length - 2] == '\\' && line[length - 1] == '\n')
        return 2;
    if (length >= 3 && line[length - 3] == '\\' &&
        line[length - 2] == '\r' && line[length - 1] == '\n')
        return 3;
    return 0;
}

/**
 * @brief Records an offset where a physical newline was spliced out.
 * @param splices Owned offset array.
 * @param offset Byte offset in the spliced source.
 * @return Nonzero on success; zero on allocation failure.
 */
static int add_splice_offset(SpliceOffsets *splices, size_t offset)
{
    if (splices->count == splices->capacity) {
        if (splices->capacity > SIZE_MAX / 2 / sizeof(*splices->offsets)) return 0;
        size_t capacity = splices->capacity ? splices->capacity * 2 : 8;
        size_t *grown = realloc(splices->offsets, capacity * sizeof(*grown));
        if (!grown) return 0;
        splices->offsets = grown;
        splices->capacity = capacity;
    }
    splices->offsets[splices->count++] = offset;
    return 1;
}

/**
 * @brief Splices a continued source line, consuming subsequent physical lines.
 * @param input Source stream following the first physical line.
 * @param path Source path for diagnostics.
 * @param source_line Line number of the first physical line.
 * @param line Input/output owned line buffer, released on failure.
 * @param length Input/output byte length.
 * @param continuations Output count of removed physical newlines.
 * @param physical_ended Output flag indicating whether the last physical line ended in LF.
 * @param splices Output offsets of removed newlines.
 * @return One on success, or zero after reporting an input or allocation failure.
 */
static int splice_lines(FILE *input, const char *path, size_t source_line,
                        char **line, size_t *length, size_t *continuations,
                        int *physical_ended, SpliceOffsets *splices)
{
    TextBuffer joined = {0};
    size_t suffix;
    while ((suffix = continuation_suffix(*line, *length)) != 0) {
        if (!buffer_append(&joined, *line, *length - suffix)) goto allocation_failure;
        if (!add_splice_offset(splices, joined.length)) goto allocation_failure;
        free(*line);
        *line = NULL;
        ++*continuations;
        int status = read_line(input, line, length);
        if (status < 0) {
            fprintf(stderr, "crunch-pp: failed reading '%s' or allocating input buffer\n", path);
            free(joined.data);
            return 0;
        }
        if (status == 0) {
            *physical_ended = 1;
            *line = joined.data;
            *length = joined.length;
            return 1;
        }
        *physical_ended = *length && (*line)[*length - 1] == '\n';
        if (memchr(*line, '\0', *length)) {
            fprintf(stderr, "crunch-pp: %s:%zu: raw NUL byte in source\n",
                    path, source_line + *continuations);
            free(*line);
            *line = NULL;
            free(joined.data);
            return 0;
        }
    }
    if (!buffer_append(&joined, *line, *length)) goto allocation_failure;
    free(*line);
    *line = joined.data;
    *length = joined.length;
    return 1;
allocation_failure:
    fprintf(stderr, "crunch-pp: out of memory joining source line in '%s' line %zu\n",
            path, source_line);
    free(*line);
    *line = NULL;
    free(joined.data);
    return 0;
}

/**
 * @brief Tests whether a macro argument consists only of whitespace/comments.
 * @param text Argument bytes to inspect.
 * @param length Number of bytes in text.
 * @return Nonzero when the argument is empty; otherwise zero.
 */
static int macro_argument_is_empty(const char *text, size_t length)
{
    for (size_t cursor = 0; cursor < length;) {
        if (isspace((unsigned char)text[cursor]) || text[cursor] == PP_TOKEN_BOUNDARY) {
            ++cursor;
        } else if (cursor + 1 < length && text[cursor] == '/' &&
                   text[cursor + 1] == '*') {
            cursor += 2;
            while (cursor + 1 < length &&
                   !(text[cursor] == '*' && text[cursor + 1] == '/'))
                ++cursor;
            if (cursor + 1 >= length) return 0;
            cursor += 2;
        } else if (cursor + 1 < length && text[cursor] == '/' &&
                   text[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < length && text[cursor] != '\n') ++cursor;
        } else {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Appends a borrowed source slice to an owned argument descriptor array.
 * @param arguments Input/output allocation containing argument descriptors.
 * @param count Input/output number of descriptors.
 * @param text Borrowed argument bytes; not copied.
 * @param length Number of bytes in the argument.
 * @return One on success; zero on overflow/allocation failure, leaving outputs unchanged.
 */
static int add_macro_argument(MacroArgument **arguments, size_t *count,
                              const char *text, size_t length)
{
    if (*count >= SIZE_MAX / sizeof(**arguments)) return 0;
    MacroArgument *grown = realloc(*arguments,
                                   (*count + 1) * sizeof(**arguments));
    if (!grown) return 0;
    *arguments = grown;
    (*arguments)[(*count)++] = (MacroArgument){text, length};
    return 1;
}

/**
 * @brief Splits invocation arguments, respecting nested parentheses and quotes.
 * @param text Source containing the invocation.
 * @param length Total number of source bytes.
 * @param open Offset of the opening parenthesis.
 * @param after Output cursor past the closing parenthesis, set only on success.
 * @param arguments Output array to free; its slices borrow bytes from text.
 * @param argument_count Output number of argument slices.
 * @return One on success, zero on malformed input, or -1 on allocation failure/overflow.
 * On failure, the argument array is NULL and its count is zero.
 */
static int parse_macro_arguments(const char *text, size_t length, size_t open,
                                 size_t *after, MacroArgument **arguments,
                                 size_t *argument_count)
{
    size_t start = open + 1, depth = 0;
    int saw_comma = 0;
    *arguments = NULL;
    *argument_count = 0;
    for (size_t cursor = start; cursor < length;) {
        if (text[cursor] == '"' || text[cursor] == '\'') {
            char quote = text[cursor++];
            while (cursor < length) {
                if (text[cursor] == '\\' && cursor + 1 < length) {
                    cursor += 2;
                    continue;
                }
                if (text[cursor++] == quote) break;
            }
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' &&
            text[cursor + 1] == '*') {
            cursor += 2;
            while (cursor + 1 < length &&
                   !(text[cursor] == '*' && text[cursor + 1] == '/'))
                ++cursor;
            if (cursor + 1 >= length) goto fail;
            cursor += 2;
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' &&
            text[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < length && text[cursor] != '\n') ++cursor;
            continue;
        }
        if (text[cursor] == '(') {
            ++depth;
        } else if (text[cursor] == ')') {
            if (!depth) {
                size_t part_length = cursor - start;
                if (saw_comma ||
                    !macro_argument_is_empty(text + start, part_length)) {
                    if (!add_macro_argument(arguments, argument_count,
                                            text + start, part_length))
                        goto oom;
                }
                *after = cursor + 1;
                return 1;
            }
            --depth;
        } else if (text[cursor] == ',' && !depth) {
            if (!add_macro_argument(arguments, argument_count,
                                    text + start, cursor - start))
                goto oom;
            saw_comma = 1;
            start = cursor + 1;
        }
        ++cursor;
    }
    goto fail;
oom:
    free(*arguments);
    *arguments = NULL;
    *argument_count = 0;
    return -1;
fail:
    free(*arguments);
    *arguments = NULL;
    *argument_count = 0;
    return 0;
}

/**
 * @brief Tests whether a macro is already on the expansion stack.
 * @param macro Definition to search for by identity.
 * @param active Active definitions, from outermost to innermost expansion.
 * @param depth Number of valid entries in active.
 * @return One if macro is active; otherwise zero.
 */
static int macro_is_active(const MacroDefinition *macro,
                           const MacroDefinition *const *active, size_t depth)
{
    for (size_t i = 0; i < depth; ++i)
        if (active[i] == macro) return 1;
    return 0;
}

/**
 * @brief Skips whitespace and block comments in preprocessing text.
 * @param text Text to scan.
 * @param length Number of bytes in text.
 * @param cursor Offset to advance.
 * @return Offset after whitespace and comments, or length for an unterminated comment.
 */
static size_t skip_macro_space(const char *text, size_t length, size_t cursor)
{
    for (;;) {
        while (cursor < length && (isspace((unsigned char)text[cursor]) ||
                                   text[cursor] == PP_TOKEN_BOUNDARY))
            ++cursor;
        if (cursor + 1 < length && text[cursor] == '/' && text[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < length && text[cursor] != '\n') ++cursor;
            continue;
        }
        if (cursor + 1 >= length || text[cursor] != '/' || text[cursor + 1] != '*')
            return cursor;
        cursor += 2;
        while (cursor + 1 < length && !(text[cursor] == '*' && text[cursor + 1] == '/'))
            ++cursor;
        if (cursor + 1 >= length)
            return length;
        cursor += 2;
    }
}

/**
 * @brief Finds the end of the last non-whitespace token in a macro argument.
 * @param text Argument bytes.
 * @param length Number of bytes in text.
 * @param cursor Offset of the first token.
 * @return Offset after the final token, excluding trailing whitespace and comments.
 */
static size_t macro_argument_end(const char *text, size_t length, size_t cursor)
{
    size_t end = cursor;
    while (cursor < length) {
        if (isspace((unsigned char)text[cursor]) || text[cursor] == PP_TOKEN_BOUNDARY) {
            ++cursor;
        } else if (cursor + 1 < length && text[cursor] == '/' && text[cursor + 1] == '*') {
            cursor = skip_macro_space(text, length, cursor);
        } else if (cursor + 1 < length && text[cursor] == '/' && text[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < length && text[cursor] != '\n') ++cursor;
        } else if (text[cursor] == '"' || text[cursor] == '\'') {
            char quote = text[cursor++];
            while (cursor < length) {
                if (text[cursor] == '\\' && cursor + 1 < length)
                    cursor += 2;
                else if (text[cursor++] == quote)
                    break;
            }
            end = cursor;
        } else {
            end = ++cursor;
        }
    }
    return end;
}

/**
 * @brief Stringifies an unexpanded macro argument as a C string literal.
 * @param output Destination buffer.
 * @param argument Raw argument bytes.
 * @return Nonzero on success.
 */
static int stringify_argument(TextBuffer *output, MacroArgument argument)
{
    const char *text = argument.text;
    size_t length = argument.length, cursor = 0;
    int pending_space = 0, emitted = 0;
    if (!buffer_append_char(output, '"')) return 0;
    while (cursor < length) {
        if (text[cursor] == PP_TOKEN_BOUNDARY) {
            ++cursor;
            continue;
        }
        if (isspace((unsigned char)text[cursor])) {
            pending_space = 1;
            ++cursor;
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' && text[cursor + 1] == '*') {
            pending_space = 1;
            cursor = skip_macro_space(text, length, cursor);
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' && text[cursor + 1] == '/') {
            pending_space = 1;
            cursor += 2;
            while (cursor < length && text[cursor] != '\n') ++cursor;
            continue;
        }
        if (pending_space && emitted && !buffer_append_char(output, ' ')) return 0;
        pending_space = 0;
        if (text[cursor] == '"' || text[cursor] == '\'') {
            char quote = text[cursor++];
            if (quote == '"' && !buffer_append_char(output, '\\')) return 0;
            if (!buffer_append_char(output, quote)) return 0;
            while (cursor < length) {
                char ch = text[cursor++];
                if ((ch == '"' || ch == '\\') && !buffer_append_char(output, '\\'))
                    return 0;
                if (!buffer_append_char(output, ch)) return 0;
                if (ch == '\\' && cursor < length) {
                    ch = text[cursor++];
                    if ((ch == '"' || ch == '\\') && !buffer_append_char(output, '\\'))
                        return 0;
                    if (!buffer_append_char(output, ch)) return 0;
                } else if (ch == quote) {
                    break;
                }
            }
        } else if (!buffer_append_char(output, text[cursor++])) return 0;
        emitted = 1;
    }
    return buffer_append_char(output, '"');
}

/**
 * @brief Substitutes parameters and processes token pastes in a macro body.
 * @param macro Definition supplying parameter names and replacement text.
 * @param arguments Expanded argument texts, in parameter order.
 * @param argument_lengths Byte lengths corresponding to arguments.
 * @param raw_arguments Unexpanded argument slices, in parameter order.
 * @param variadic_omitted Whether the variadic argument was omitted.
 * @param substituted Buffer receiving the substituted replacement.
 * @return One on success; zero on malformed comments/allocation failure, possibly after appending.
 */
static int substitute_macro_parameters(const MacroDefinition *macro,
                                       const char *const *arguments,
                                       const size_t *argument_lengths,
                                       const MacroArgument *raw_arguments,
                                       int variadic_omitted,
                                       TextBuffer *substituted)
{
    const char *text = macro->replacement;
    size_t length = strlen(text);
    int paste_pending = 0;
    for (size_t cursor = 0; cursor < length;) {
        if (paste_pending) {
            cursor = skip_macro_space(text, length, cursor);
            if (cursor == length) return 0;
        }
        if (text[cursor] == 'L' && cursor + 1 < length &&
            (text[cursor + 1] == '\'' || text[cursor + 1] == '"')) {
            if (!buffer_append_char(substituted, text[cursor++]) ||
                !append_quoted_text(substituted, text, length, &cursor))
                return 0;
            paste_pending = 0;
            continue;
        }
        if (text[cursor] == '"' || text[cursor] == '\'') {
            if (!append_quoted_text(substituted, text, length, &cursor))
                return 0;
            paste_pending = 0;
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' &&
            text[cursor + 1] == '/') {
            return buffer_append(substituted, text + cursor,
                                 length - cursor);
        }
        if (cursor + 1 < length && text[cursor] == '/' &&
            text[cursor + 1] == '*') {
            size_t end = cursor + 2;
            while (end + 1 < length &&
                   !(text[end] == '*' && text[end + 1] == '/'))
                ++end;
            if (end + 1 >= length) return 0;
            end += 2;
            if (!buffer_append_char(substituted, ' '))
                return 0;
            cursor = end;
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '#' && text[cursor + 1] == '#') {
            while (substituted->length &&
                   isspace((unsigned char)substituted->data[substituted->length - 1]))
                --substituted->length;
            if (!substituted->length || paste_pending) return 0;
            substituted->data[substituted->length] = '\0';
            size_t next = skip_macro_space(text, length, cursor + 2), name_length = 0;
            if (macro->variadic && substituted->data[substituted->length - 1] == ',' &&
                identifier_at(text, length, next, &name_length) &&
                strlen(macro->parameters[macro->parameter_count - 1]) == name_length &&
                !memcmp(macro->parameters[macro->parameter_count - 1], text + next, name_length)) {
                if (variadic_omitted) {
                    --substituted->length;
                    substituted->data[substituted->length] = '\0';
                    cursor = next + name_length;
                } else {
                    cursor += 2;
                    if (!buffer_append_char(substituted, ' ')) return 0;
                }
                continue;
            }
            cursor += 2;
            paste_pending = 1;
            continue;
        }
        if (text[cursor] == '#' && macro->function_like) {
            size_t start = skip_macro_space(text, length, cursor + 1), name_length = 0;
            if (!identifier_at(text, length, start, &name_length)) return 0;
            size_t parameter = 0;
            for (; parameter < macro->parameter_count; ++parameter)
                if (strlen(macro->parameters[parameter]) == name_length &&
                    !memcmp(macro->parameters[parameter], text + start, name_length))
                    break;
            if (parameter == macro->parameter_count ||
                !stringify_argument(substituted, raw_arguments[parameter])) return 0;
            cursor = start + name_length;
            paste_pending = 0;
            continue;
        }
        size_t number_length = pp_number_length(text, length, cursor);
        if (number_length) {
            if (!buffer_append(substituted, text + cursor, number_length)) return 0;
            cursor += number_length;
            paste_pending = 0;
            continue;
        }
        size_t identifier_length = 0;
        if (identifier_at(text, length, cursor, &identifier_length)) {
            size_t parameter = 0;
            for (; parameter < macro->parameter_count; ++parameter)
                if (strlen(macro->parameters[parameter]) == identifier_length &&
                    memcmp(macro->parameters[parameter], text + cursor,
                           identifier_length) == 0)
                    break;
            if (parameter < macro->parameter_count) {
                size_t next = skip_macro_space(text, length, cursor + identifier_length);
                int paste_after = next + 1 < length && text[next] == '#' && text[next + 1] == '#';
                int raw = paste_pending || paste_after;
                const char *value = raw ? raw_arguments[parameter].text : arguments[parameter];
                size_t value_length = raw ? raw_arguments[parameter].length :
                                            argument_lengths[parameter];
                if (raw) {
                    size_t begin = skip_macro_space(value, value_length, 0);
                    size_t end = macro_argument_end(value, value_length, begin);
                    value += begin;
                    value_length = end - begin;
                }
                if (!paste_pending && !buffer_append_char(substituted, PP_TOKEN_BOUNDARY))
                    return 0;
                if (!buffer_append(substituted, value, value_length)) return 0;
                if (!paste_after && !buffer_append_char(substituted, PP_TOKEN_BOUNDARY))
                    return 0;
            } else {
                if (!buffer_append(substituted, text + cursor, identifier_length))
                    return 0;
            }
            cursor += identifier_length;
            paste_pending = 0;
            continue;
        }
        if (!buffer_append_char(substituted, text[cursor++])) return 0;
        if (!isspace((unsigned char)text[cursor - 1]))
            paste_pending = 0;
    }
    if (paste_pending) return 0;
    return 1;
}

/**
 * @brief Checks whether a parameter is used outside stringification or pasting.
 * @param macro Macro definition to inspect.
 * @param parameter Index of the parameter.
 * @return Nonzero when the argument needs macro expansion.
 */
static int parameter_needs_expansion(const MacroDefinition *macro, size_t parameter)
{
    const char *text = macro->replacement;
    size_t length = strlen(text), cursor = 0;
    int hash = 0, last_comma = 0, comma_paste = 0;
    while (cursor < length) {
        size_t next = skip_macro_space(text, length, cursor);
        if (next != cursor) {
            cursor = next;
            continue;
        }
        if (text[cursor] == '"' || text[cursor] == '\'') {
            char quote = text[cursor++];
            while (cursor < length) {
                if (text[cursor] == '\\' && cursor + 1 < length) cursor += 2;
                else if (text[cursor++] == quote) break;
            }
            hash = 0;
            last_comma = 0;
            continue;
        }
        if (text[cursor] == '#') {
            comma_paste = last_comma && cursor + 1 < length && text[cursor + 1] == '#';
            hash = 1;
            ++cursor;
            if (cursor < length && text[cursor] == '#') ++cursor;
            continue;
        }
        size_t number_length = pp_number_length(text, length, cursor);
        if (number_length) {
            cursor += number_length;
            hash = 0;
            last_comma = 0;
            continue;
        }
        size_t name_length = 0;
        if (identifier_at(text, length, cursor, &name_length)) {
            next = skip_macro_space(text, length, cursor + name_length);
            if ((!hash || (comma_paste && macro->variadic &&
                           parameter + 1 == macro->parameter_count)) &&
                !(next + 1 < length && text[next] == '#' && text[next + 1] == '#') &&
                strlen(macro->parameters[parameter]) == name_length &&
                !memcmp(macro->parameters[parameter], text + cursor, name_length))
                return 1;
            cursor += name_length;
            last_comma = 0;
        } else {
            last_comma = text[cursor] == ',';
            ++cursor;
        }
        hash = 0;
        comma_paste = 0;
    }
    return 0;
}

static int resolve_has_include(Preprocessor *pp, const char *text, size_t length,
                               size_t identifier_start, size_t identifier_end, int include_next,
                               size_t *after, int *found);

/**
 * @brief Computes stable UTC date and time strings for this translation.
 * @param pp Preprocessor holding the optional reproducible timestamp.
 * @return Nonzero on success, or zero for an invalid SOURCE_DATE_EPOCH.
 */
static int prepare_timestamp(Preprocessor *pp)
{
    if (pp->timestamp_ready)
        return 1;
    uint64_t seconds = 0;
    const uint64_t maximum = UINT64_C(253402300799);
    if (pp->source_date_epoch) {
        const char *p = pp->source_date_epoch;
        if (!*p)
            goto invalid_epoch;
        for (; *p; ++p) {
            if (*p < '0' || *p > '9' ||
                seconds > (maximum - (unsigned)(*p - '0')) / 10)
                goto invalid_epoch;
            seconds = seconds * 10 + (unsigned)(*p - '0');
        }
    } else {
        time_t now = time(NULL);
        if (now < 0 || (uint64_t)now > maximum) {
            memcpy(pp->date_literal, "??? ?? ????", sizeof pp->date_literal);
            memcpy(pp->time_literal, "??:??:??", sizeof pp->time_literal);
            pp->timestamp_ready = 1;
            return 1;
        }
        seconds = (uint64_t)now;
    }
    uint64_t days = seconds / 86400;
    unsigned remainder = (unsigned)(seconds % 86400);
    unsigned year = 1970;
    for (;;) {
        unsigned leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        unsigned length = 365 + leap;
        if (days < length)
            break;
        days -= length;
        ++year;
    }
    const unsigned month_lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    unsigned month = 0;
    for (;;) {
        unsigned length = month_lengths[month] +
                          (month == 1 && year % 4 == 0 &&
                           (year % 100 != 0 || year % 400 == 0));
        if (days < length)
            break;
        days -= length;
        ++month;
    }
    unsigned day = (unsigned)days + 1;
    memcpy(pp->date_literal, months[month], 3);
    pp->date_literal[3] = ' ';
    pp->date_literal[4] = day < 10 ? ' ' : (char)('0' + day / 10);
    pp->date_literal[5] = (char)('0' + day % 10);
    pp->date_literal[6] = ' ';
    pp->date_literal[7] = (char)('0' + year / 1000);
    pp->date_literal[8] = (char)('0' + year / 100 % 10);
    pp->date_literal[9] = (char)('0' + year / 10 % 10);
    pp->date_literal[10] = (char)('0' + year % 10);
    pp->date_literal[11] = '\0';
    unsigned hour = remainder / 3600;
    unsigned minute = remainder / 60 % 60;
    unsigned second = remainder % 60;
    pp->time_literal[0] = (char)('0' + hour / 10);
    pp->time_literal[1] = (char)('0' + hour % 10);
    pp->time_literal[2] = ':';
    pp->time_literal[3] = (char)('0' + minute / 10);
    pp->time_literal[4] = (char)('0' + minute % 10);
    pp->time_literal[5] = ':';
    pp->time_literal[6] = (char)('0' + second / 10);
    pp->time_literal[7] = (char)('0' + second % 10);
    pp->time_literal[8] = '\0';
    pp->timestamp_ready = 1;
    return 1;

invalid_epoch:
    fprintf(stderr, "crunch-pp: SOURCE_DATE_EPOCH must be an integer from 0 to 253402300799\n");
    return 0;
}

/**
 * @brief Identifies a built-in query or an object-like alias for one.
 * @param pp Preprocessor containing the macro namespace.
 * @param name Identifier bytes to inspect.
 * @param length Number of identifier bytes.
 * @return One or two for header queries, three or four for capability queries,
 * or zero otherwise.
 */
static int builtin_query_kind(Preprocessor *pp, const char *name, size_t length)
{
    const MacroDefinition *seen[PP_MAX_MACRO_EXPANSION_DEPTH];
    size_t count = 0;
    for (;;) {
        MacroDefinition *macro = find_macro_definition(pp, name, length);
        if (!macro) {
            if (!macro_defined(pp, name, length)) return 0;
            if (length == 13 && !memcmp(name, "__has_include", 13)) return 1;
            if (length == 18 && !memcmp(name, "__has_include_next", 18)) return 2;
            if (length == 13 && !memcmp(name, "__has_builtin", 13)) return 3;
            if (length == 15 && !memcmp(name, "__has_attribute", 15)) return 4;
            return 0;
        }
        if (macro->function_like || count == PP_MAX_MACRO_EXPANSION_DEPTH)
            return 0;
        for (size_t i = 0; i < count; ++i)
            if (seen[i] == macro) return 0;
        seen[count++] = macro;
        const char *replacement = macro->replacement;
        size_t replacement_length = strlen(replacement);
        size_t start = skip_macro_space(replacement, replacement_length, 0), n = 0;
        if (!identifier_at(replacement, replacement_length, start, &n) ||
            skip_macro_space(replacement, replacement_length, start + n) != replacement_length)
            return 0;
        name = replacement + start;
        length = n;
    }
}

/**
 * @brief Resolves a chain of single-identifier object aliases to a function macro.
 * @param pp Preprocessor containing the macro namespace.
 * @param macro First object-like definition.
 * @param active Definitions already being expanded.
 * @param depth Number of active definitions.
 * @return Callable definition, or NULL for a cycle, suppressed or non-alias chain.
 */
static MacroDefinition *function_macro_alias(Preprocessor *pp, MacroDefinition *macro,
                                             const MacroDefinition *const *active,
                                             size_t depth)
{
    const MacroDefinition *seen[PP_MAX_MACRO_EXPANSION_DEPTH];
    size_t count = 0;
    while (macro) {
        if (macro_is_active(macro, active, depth)) return NULL;
        if (macro->function_like) return count ? macro : NULL;
        if (count >= PP_MAX_MACRO_EXPANSION_DEPTH - depth) return NULL;
        for (size_t i = 0; i < count; ++i)
            if (seen[i] == macro) return NULL;
        seen[count++] = macro;
        const char *replacement = macro->replacement;
        size_t length = strlen(replacement), name_length = 0;
        size_t start = skip_macro_space(replacement, length, 0);
        if (!identifier_at(replacement, length, start, &name_length) ||
            skip_macro_space(replacement, length, start + name_length) != length)
            return NULL;
        macro = find_macro_definition(pp, replacement + start, name_length);
    }
    return NULL;
}

/**
 * @brief Locates the operand and closing parenthesis of a header query.
 * @param text Expression bytes.
 * @param length Number of expression bytes.
 * @param identifier_end Offset after the operator name.
 * @param start Output first operand byte.
 * @param end Output one-past-last operand byte.
 * @param after Output offset after the closing parenthesis.
 * @return Nonzero on success; zero for malformed delimiters.
 */
static int has_include_bounds(const char *text, size_t length, size_t identifier_end,
                              size_t *start, size_t *end, size_t *after)
{
    size_t open = skip_macro_space(text, length, identifier_end);
    if (open >= length || text[open] != '(') return 0;
    *start = skip_macro_space(text, length, open + 1);
    *end = *start;
    if (*start < length && (text[*start] == '<' || text[*start] == '"')) {
        char close = text[*start] == '<' ? '>' : '"';
        const char *terminator = memchr(text + *start + 1, close, length - *start - 1);
        if (!terminator) return 0;
        *end = (size_t)(terminator - text) + 1;
    } else {
        size_t depth = 0;
        for (*end = *start; *end < length; ++*end) {
            if (text[*end] == '"' || text[*end] == '\'') {
                char quote = text[(*end)++];
                while (*end < length && text[*end] != quote) {
                    if (text[*end] == '\\' && *end + 1 < length) ++*end;
                    ++*end;
                }
                if (*end == length) return 0;
            } else if (*end + 1 < length && text[*end] == '/' && text[*end + 1] == '*') {
                *end = skip_macro_space(text, length, *end);
                if (*end == length) return 0;
                --*end;
            } else if (*end + 1 < length && text[*end] == '/' && text[*end + 1] == '/') {
                return 0;
            } else if (text[*end] == '(') {
                ++depth;
            } else if (text[*end] == ')') {
                if (!depth) break;
                --depth;
            }
        }
    }
    size_t close = skip_macro_space(text, length, *end);
    if (close >= length || text[close] != ')') return 0;
    *after = close + 1;
    return 1;
}

static int expand_text(Preprocessor *pp, const char *text, size_t length,
                       const MacroDefinition *const *active, size_t depth,
                       int *in_block_comment, TextBuffer *output);

/**
 * @brief Counts line endings in a source byte range.
 * @param text Source bytes.
 * @param start First byte to inspect.
 * @param end One-past-last byte to inspect.
 * @return Number of newline bytes in the range.
 */
static size_t source_newlines(const char *text, size_t start, size_t end)
{
    size_t count = 0;
    for (size_t i = start; i < end; ++i)
        if (text[i] == '\n') ++count;
    return count;
}

/**
 * @brief Counts splice newlines at or before a source offset.
 * @param splices Removed newline offsets in ascending order.
 * @param offset Source offset to inspect.
 * @return Number of removed newlines preceding the offset.
 */
static size_t leading_splices(const SpliceOffsets *splices, size_t offset)
{
    size_t low = 0, high = splices->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (splices->offsets[middle] <= offset) low = middle + 1;
        else high = middle;
    }
    return low;
}

/**
 * @brief Counts physical line breaks, including removed splice newlines.
 * @param pp Preprocessor supplying the current source map.
 * @param text Source bytes, possibly a slice of the mapped source.
 * @param start First byte of the range.
 * @param end One-past-last byte of the range.
 * @return Number of physical line breaks crossed.
 */
static size_t source_breaks(const Preprocessor *pp, const char *text,
                            size_t start, size_t end)
{
    size_t count = source_newlines(text, start, end);
    if (!pp->track_source_lines || !pp->mapped_text || !pp->mapped_splices)
        return count;
    /* Tracked expansion text is always a slice of mapped_text. */
    size_t origin = (size_t)(text - pp->mapped_text);
    if (origin > pp->mapped_length || end > pp->mapped_length - origin) return count;
    return count + leading_splices(pp->mapped_splices, origin + end) -
                   leading_splices(pp->mapped_splices, origin + start);
}

/**
 * @brief Appends a lexer source marker to an expanded text buffer.
 * @param output Destination buffer.
 * @param path Logical source path.
 * @param line One-based logical line number.
 * @return Nonzero on success; zero on allocation failure.
 */
static int append_source_marker(TextBuffer *output, const char *path, size_t line)
{
    char prefix[64];
    int length = snprintf(prefix, sizeof prefix, "/*CRUNCH-PP-MAP:%zu:", line);
    if (length < 0 || (size_t)length >= sizeof prefix ||
        !buffer_append(output, prefix, (size_t)length))
        return 0;
    static const char hex[] = "0123456789abcdef";
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        char digits[2] = {hex[*p >> 4], hex[*p & 15]};
        if (!buffer_append(output, digits, 2)) return 0;
    }
    return buffer_append(output, "*/\n", 3);
}

/**
 * @brief Keeps a multiline macro expansion on its invocation line.
 * @param output Buffer containing the expanded source.
 * @param start First byte of the macro expansion.
 * @return No value; normalizes comments and newlines in place.
 */
static void normalize_multiline_macro(TextBuffer *output, size_t start)
{
    size_t read = start, written = start;
    while (read < output->length) {
        char ch = output->data[read++];
        if (ch == '"' || ch == '\'') {
            char quote = ch;
            output->data[written++] = ch;
            while (read < output->length) {
                ch = output->data[read++];
                output->data[written++] = ch;
                if (ch == '\\' && read < output->length)
                    output->data[written++] = output->data[read++];
                else if (ch == quote)
                    break;
            }
        } else if (ch == '/' && read < output->length && output->data[read] == '/') {
            ++read;
            while (read < output->length && output->data[read] != '\n') ++read;
            output->data[written++] = ' ';
        } else if (ch == '/' && read < output->length && output->data[read] == '*') {
            ++read;
            while (read + 1 < output->length &&
                   !(output->data[read] == '*' && output->data[read + 1] == '/'))
                ++read;
            if (read + 1 < output->length) read += 2;
            output->data[written++] = ' ';
        } else {
            output->data[written++] = ch == '\n' || ch == '\r' ? ' ' : ch;
        }
    }
    output->length = written;
    output->data[written] = '\0';
}

/**
 * @brief Validates and evaluates a compiler capability query.
 * @param pp Preprocessor containing the macro namespace.
 * @param text Source containing the invocation.
 * @param length Number of source bytes.
 * @param identifier_start Offset at the operator name.
 * @param identifier_end Offset after the operator name.
 * @param active Definitions already being expanded.
 * @param depth Number of active definitions.
 * @param after Output offset after the invocation.
 * @param output Buffer receiving the result.
 * @return Nonzero on success; zero for malformed input or allocation failure.
 */
static int expand_feature_query(Preprocessor *pp, const char *text, size_t length,
                                size_t identifier_start, size_t identifier_end,
                                const MacroDefinition *const *active, size_t depth,
                                size_t *after, TextBuffer *output)
{
    size_t open = skip_macro_space(text, length, identifier_end);
    if (open >= length || text[open] != '(' ||
        pp->feature_query_depth >= PP_MAX_MACRO_EXPANSION_DEPTH)
        return 0;
    MacroArgument *arguments = NULL;
    size_t count = 0;
    TextBuffer expanded = {0};
    int ok = 0, comment = 0;
    if (parse_macro_arguments(text, length, open, after, &arguments, &count) <= 0 ||
        count != 1)
        goto done;
    ++pp->feature_query_depth;
    ++pp->if_argument_depth;
    size_t saved_line = pp->source_line;
    int saved_markers = pp->emit_source_markers;
    if (pp->track_source_lines)
        pp->source_line += source_breaks(pp, text, identifier_start,
                                         (size_t)(arguments[0].text - text));
    pp->emit_source_markers = 0;
    int expanded_ok = expand_text(pp, arguments[0].text, arguments[0].length,
                                  active, depth, &comment, &expanded);
    pp->source_line = saved_line;
    pp->emit_source_markers = saved_markers;
    --pp->if_argument_depth;
    --pp->feature_query_depth;
    if (!expanded_ok || comment) goto done;
    const char *value = expanded.data ? expanded.data : "";
    size_t cursor = skip_macro_space(value, expanded.length, 0), name_length = 0;
    size_t name_start = cursor;
    if (!identifier_at(value, expanded.length, cursor, &name_length)) goto done;
    cursor = skip_macro_space(value, expanded.length, cursor + name_length);
    if (cursor != expanded.length) goto done;
    int supported = builtin_query_kind(pp, text + identifier_start,
                                       identifier_end - identifier_start) == 3 &&
                    ((name_length == 16 &&
                      !memcmp(value + name_start, "__builtin_alloca", 16)) ||
                     (name_length == 18 &&
                      !memcmp(value + name_start, "__builtin_offsetof", 18)));
    ok = buffer_append(output, supported ? "1" : "0", 1);
done:
    free(arguments);
    free(expanded.data);
    return ok;
}

/**
 * @brief Resolves a defined operator without expanding its operand.
 * @param pp Preprocessor containing the macro namespace.
 * @param text Expression bytes.
 * @param length Number of expression bytes.
 * @param identifier_end Offset after defined.
 * @param after Output offset after the operand.
 * @param output Buffer receiving the result.
 * @return Nonzero on success; zero for malformed input or allocation failure.
 */
static int expand_defined(Preprocessor *pp, const char *text, size_t length,
                          size_t identifier_end, size_t *after, TextBuffer *output)
{
    size_t cursor = skip_macro_space(text, length, identifier_end);
    int parens = cursor < length && text[cursor] == '(';
    if (parens) cursor = skip_macro_space(text, length, cursor + 1);
    size_t name_length = 0;
    if (!identifier_at(text, length, cursor, &name_length)) return 0;
    int defined = macro_defined(pp, text + cursor, name_length);
    cursor += name_length;
    if (parens) {
        cursor = skip_macro_space(text, length, cursor);
        if (cursor >= length || text[cursor] != ')') return 0;
        ++cursor;
    }
    if (!buffer_append(output, defined ? " 1 " : " 0 ", 3)) return 0;
    *after = cursor;
    return 1;
}

/**
 * @brief Expands one macro invocation and rescans the substituted replacement.
 * @param pp Preprocessor supplying the macro definitions.
 * @param macro Definition to expand.
 * @param text Source containing the invocation.
 * @param length Total number of source bytes.
 * @param identifier_start Offset at the original macro name.
 * @param identifier_end Offset immediately after the macro name.
 * @param active Expansion stack used to suppress recursive expansion.
 * @param depth Number of active definitions.
 * @param output Buffer receiving expanded text; may be partially appended on failure.
 * @param after Output cursor past the invocation when expansion succeeds.
 * @return One on expansion, two for a function-like macro without '(', or -1 on failure.
 */
static int expand_macro(Preprocessor *pp, MacroDefinition *macro,
                        const char *text, size_t length, size_t identifier_start,
                        size_t identifier_end,
                        const MacroDefinition *const *active, size_t depth,
                        TextBuffer *output, size_t *after)
{
    size_t open = skip_macro_space(text, length, identifier_end);
    MacroArgument *arguments = NULL;
    size_t argument_count = 0;
    TextBuffer *expanded_arguments = NULL;
    size_t *argument_lengths = NULL;
    TextBuffer substituted = {0};
    int ok = 0;
    int variadic_omitted = 0;

    if (macro->function_like) {
        if (open >= length || text[open] != '(') return 2;
        int parse_status = parse_macro_arguments(text, length, open, after,
                                                 &arguments, &argument_count);
        if (parse_status <= 0) {
            if (parse_status < 0)
                fprintf(stderr, "crunch-pp: out of memory or too many arguments for macro '%s'\n",
                        macro->name);
            else
                fprintf(stderr, "crunch-pp: unterminated argument list for macro '%s'\n",
                        macro->name);
            return -1;
        }
        size_t fixed_count = macro->parameter_count - (macro->variadic ? 1 : 0);
        if (argument_count == 0 && fixed_count == 1) {
            if (!add_macro_argument(&arguments, &argument_count, text + open + 1, 0))
                goto done;
        }
        if (macro->variadic && argument_count >= fixed_count) {
            variadic_omitted = argument_count == fixed_count;
            if (variadic_omitted) {
                if (!add_macro_argument(&arguments, &argument_count, text + *after - 1, 0))
                    goto done;
            } else {
                const MacroArgument *last = &arguments[argument_count - 1];
                arguments[fixed_count].length =
                    (size_t)(last->text + last->length - arguments[fixed_count].text);
                argument_count = fixed_count + 1;
            }
        }
        if (argument_count != macro->parameter_count) {
            fprintf(stderr, "crunch-pp: macro '%s' expects %zu argument%s, got %zu\n",
                    macro->name, macro->parameter_count,
                    macro->parameter_count == 1 ? "" : "s", argument_count);
            goto done;
        }
        if (argument_count) {
            expanded_arguments = calloc(argument_count,
                                        sizeof(*expanded_arguments));
            argument_lengths = calloc(argument_count,
                                      sizeof(*argument_lengths));
            if (!expanded_arguments || !argument_lengths) goto done;
            for (size_t i = 0; i < argument_count; ++i) {
                int argument_comment = 0;
                if (parameter_needs_expansion(macro, i)) {
                    /* Argument tokens are expanded before defined is interpreted. */
                    ++pp->if_argument_depth;
                    size_t saved_line = pp->source_line;
                    int saved_markers = pp->emit_source_markers;
                    if (pp->track_source_lines)
                        pp->source_line += source_breaks(
                            pp, text, identifier_start,
                            (size_t)(arguments[i].text - text));
                    pp->emit_source_markers = 0;
                    int expanded = expand_text(pp, arguments[i].text, arguments[i].length,
                                               active, depth, &argument_comment,
                                               &expanded_arguments[i]);
                    pp->source_line = saved_line;
                    pp->emit_source_markers = saved_markers;
                    --pp->if_argument_depth;
                    if (!expanded) goto done;
                }
                argument_lengths[i] = expanded_arguments[i].length;
            }
            const char **expanded_text = calloc(argument_count,
                                                sizeof(*expanded_text));
            if (!expanded_text) goto done;
            for (size_t i = 0; i < argument_count; ++i)
                expanded_text[i] = expanded_arguments[i].data ?
                                   expanded_arguments[i].data : "";
            if (!substitute_macro_parameters(macro, expanded_text,
                                             argument_lengths, arguments,
                                             variadic_omitted, &substituted)) {
                free(expanded_text);
                goto done;
            }
            free(expanded_text);
        } else if (!substitute_macro_parameters(macro, NULL, NULL, NULL, 0,
                                                &substituted)) {
            goto done;
        }
    } else {
        *after = identifier_end;
        if (!substitute_macro_parameters(macro, NULL, NULL, NULL, 0, &substituted))
            goto done;
    }

    if (depth >= PP_MAX_MACRO_EXPANSION_DEPTH) {
        fprintf(stderr, "crunch-pp: macro expansion nesting limit exceeded at '%s'\n",
                macro->name);
        goto done;
    }
    {
        const MacroDefinition *next_active[PP_MAX_MACRO_EXPANSION_DEPTH];
        if (depth) memcpy(next_active, active,
                          depth * sizeof(*next_active));
        next_active[depth] = macro;
        int replacement_comment = 0;
        int saved_tracking = pp->track_source_lines;
        int saved_markers = pp->emit_source_markers;
        pp->track_source_lines = 0;
        pp->emit_source_markers = 0;
        ok = expand_text(pp, substituted.data ? substituted.data : "",
                         substituted.length, next_active, depth + 1,
                         &replacement_comment, output) ? 1 : -1;
        pp->track_source_lines = saved_tracking;
        pp->emit_source_markers = saved_markers;
    }

done:
    for (size_t i = 0; i < argument_count; ++i)
        free(expanded_arguments ? expanded_arguments[i].data : NULL);
    free(expanded_arguments);
    free(argument_lengths);
    free(arguments);
    free(substituted.data);
    return ok == 1 ? 1 : -1;
}

/**
 * @brief Expands macros in source text while preserving quoted text and comments.
 * @param pp Preprocessor supplying macro definitions.
 * @param text Source bytes to expand.
 * @param length Number of source bytes.
 * @param active Definitions already being expanded.
 * @param depth Number of active definitions.
 * @param in_block_comment Input/output comment state carried across source lines.
 * @param output Buffer receiving the expanded bytes.
 * @return One on success; zero on malformed expansion/allocation failure, possibly after appending.
 */
static int expand_text(Preprocessor *pp, const char *text, size_t length,
                       const MacroDefinition *const *active, size_t depth,
                       int *in_block_comment, TextBuffer *output)
{
    size_t source_line = pp->source_line, line_cursor = 0;
    for (size_t cursor = 0; cursor < length;) {
        if (pp->track_source_lines) {
            source_line += source_breaks(pp, text, line_cursor, cursor);
            line_cursor = cursor;
        }
        if (*in_block_comment) {
            if (cursor + 1 < length && text[cursor] == '*' &&
                text[cursor + 1] == '/') {
                if (!buffer_append(output, text + cursor, 2)) return 0;
                cursor += 2;
                *in_block_comment = 0;
            } else if (!buffer_append_char(output, text[cursor++])) {
                return 0;
            }
            continue;
        }
        if (text[cursor] == 'L' && cursor + 1 < length &&
            (text[cursor + 1] == '\'' || text[cursor + 1] == '"')) {
            if (!buffer_append_char(output, text[cursor++]) ||
                !append_quoted_text(output, text, length, &cursor)) return 0;
            continue;
        }
        if (text[cursor] == '"' || text[cursor] == '\'') {
            if (!append_quoted_text(output, text, length, &cursor)) return 0;
            continue;
        }
        if (pp->assembly && text[cursor] == '#') {
            size_t end = cursor;
            while (end < length && text[end] != '\n') ++end;
            if (!buffer_append(output, text + cursor, end - cursor)) return 0;
            cursor = end;
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' &&
            text[cursor + 1] == '/') {
            size_t end = cursor + 2;
            while (end < length && text[end] != '\n') ++end;
            if (!buffer_append(output, text + cursor, end - cursor)) return 0;
            cursor = end;
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' &&
            text[cursor + 1] == '*') {
            if (!buffer_append(output, text + cursor, 2)) return 0;
            cursor += 2;
            *in_block_comment = 1;
            continue;
        }
        size_t number_length = pp_number_length(text, length, cursor);
        if (number_length) {
            if (!buffer_append(output, text + cursor, number_length)) return 0;
            cursor += number_length;
            continue;
        }
        size_t identifier_length = 0;
        if (!identifier_at(text, length, cursor, &identifier_length)) {
            if (!buffer_append_char(output, text[cursor++])) return 0;
            continue;
        }
        MacroDefinition *macro = find_macro_definition(
            pp, text + cursor, identifier_length);
        size_t identifier_end = cursor + identifier_length;
        if (pp->if_expression && !pp->if_argument_depth && identifier_length == 7 &&
            !memcmp(text + cursor, "defined", 7)) {
            size_t after = 0;
            if (!expand_defined(pp, text, length, identifier_end, &after, output))
                return 0;
            cursor = after;
            continue;
        }
        int query_kind = builtin_query_kind(pp, text + cursor, identifier_length);
        if (query_kind && (pp->if_expression || query_kind >= 3) &&
            (!macro || !macro_is_active(macro, active, depth))) {
            size_t after = 0;
            if (query_kind >= 3) {
                size_t saved_line = pp->source_line;
                pp->source_line = source_line;
                if (!buffer_append_char(output, PP_TOKEN_BOUNDARY)) return 0;
                int ok = expand_feature_query(pp, text, length, cursor, identifier_end, active,
                                              depth, &after, output);
                pp->source_line = saved_line;
                if (!ok || !buffer_append_char(output, PP_TOKEN_BOUNDARY))
                    return 0;
                size_t lines = source_breaks(pp, text, cursor, after);
                if (lines && pp->emit_source_markers &&
                    !append_source_marker(output, pp->source_path, source_line + lines))
                    return 0;
                cursor = after;
                continue;
            }
            int found = 0;
            size_t saved_line = pp->source_line;
            pp->source_line = source_line;
            int ok = resolve_has_include(pp, text, length, cursor, identifier_end,
                                         query_kind == 2, &after, &found);
            pp->source_line = saved_line;
            if (!ok || !buffer_append_char(output, PP_TOKEN_BOUNDARY) ||
                !buffer_append_char(output, found ? '1' : '0') ||
                !buffer_append_char(output, PP_TOKEN_BOUNDARY))
                return 0;
            cursor = after;
            continue;
        }
        if (pp->source_path && !macro && identifier_length == 8 &&
            !memcmp(text + cursor, "__FILE__", 8) &&
            macro_defined(pp, text + cursor, identifier_length)) {
            if (!buffer_append_char(output, PP_TOKEN_BOUNDARY) ||
                !buffer_append_char(output, '"')) return 0;
            for (const char *p = pp->source_path; *p; ++p) {
                unsigned char ch = (unsigned char)*p;
                if (ch < 32 || ch == 127) {
                    char escaped[5];
                    octal_escape(ch, escaped);
                    if (!buffer_append(output, escaped, 4)) return 0;
                    continue;
                }
                if ((*p == '"' || *p == '\\') && !buffer_append_char(output, '\\'))
                    return 0;
                if (!buffer_append_char(output, *p)) return 0;
            }
            if (!buffer_append_char(output, '"') ||
                !buffer_append_char(output, PP_TOKEN_BOUNDARY)) return 0;
            cursor = identifier_end;
            continue;
        }
        if (pp->source_path && !macro && identifier_length == 8 &&
            !memcmp(text + cursor, "__LINE__", 8) &&
            macro_defined(pp, text + cursor, identifier_length)) {
            char number[32];
            int n = snprintf(number, sizeof number, "%zu", source_line);
            if (n < 0 || (size_t)n >= sizeof number ||
                !buffer_append_char(output, PP_TOKEN_BOUNDARY) ||
                !buffer_append(output, number, (size_t)n) ||
                !buffer_append_char(output, PP_TOKEN_BOUNDARY))
                return 0;
            cursor = identifier_end;
            continue;
        }
        if (!macro && identifier_length == 8 &&
            (!memcmp(text + cursor, "__DATE__", 8) ||
             !memcmp(text + cursor, "__TIME__", 8)) &&
            macro_defined(pp, text + cursor, identifier_length)) {
            if (!prepare_timestamp(pp))
                return 0;
            const char *value = text[cursor + 2] == 'D' ? pp->date_literal : pp->time_literal;
            if (!buffer_append_char(output, PP_TOKEN_BOUNDARY) ||
                !buffer_append_char(output, '"') ||
                !buffer_append(output, value, strlen(value)) ||
                !buffer_append_char(output, '"') ||
                !buffer_append_char(output, PP_TOKEN_BOUNDARY))
                return 0;
            cursor = identifier_end;
            continue;
        }
        if (!macro || macro_is_active(macro, active, depth)) {
            if (!buffer_append(output, text + cursor, identifier_length))
                return 0;
            cursor = identifier_end;
            continue;
        }
        size_t after = identifier_end;
        size_t output_start = output->length;
        if (!buffer_append_char(output, PP_TOKEN_BOUNDARY)) return 0;
        size_t saved_line = pp->source_line;
        pp->source_line = source_line;
        MacroDefinition *alias = function_macro_alias(pp, macro, active, depth);
        int saved_tracking = pp->track_source_lines;
        if (alias) {
            size_t open = skip_macro_space(text, length, identifier_end);
            if (open < length && text[open] == '(') {
                macro = alias;
                pp->track_source_lines = 0;
            }
        }
        int expansion = expand_macro(pp, macro, text, length, cursor, identifier_end,
                                     active, depth, output, &after);
        pp->track_source_lines = saved_tracking;
        pp->source_line = saved_line;
        if (expansion < 0) return 0;
        if (expansion == 2) {
            output->length = output_start;
            output->data[output_start] = '\0';
            if (!buffer_append(output, text + cursor, identifier_length))
                return 0;
            cursor = identifier_end;
            continue;
        }
        if (!expansion) {
            fprintf(stderr, "crunch-pp: failed expanding macro '%s'\n",
                    macro->name);
            return 0;
        }
        if (!buffer_append_char(output, PP_TOKEN_BOUNDARY))
            return 0;
        size_t lines = source_breaks(pp, text, cursor, after);
        if (lines)
            normalize_multiline_macro(output, output_start);
        if (lines && pp->emit_source_markers &&
            !append_source_marker(output, pp->source_path, source_line + lines))
            return 0;
        cursor = after;
    }
    return 1;
}

/**
 * @brief Determines whether ordinary source needs another line before expansion.
 * @param pp Preprocessor containing the macro namespace.
 * @param text Buffered source bytes.
 * @param length Number of source bytes.
 * @param in_block_comment Comment state at the beginning of the buffer.
 * @return One if a macro invocation may continue, zero if complete, or -1 on allocation failure.
 */
static int macro_text_needs_line(Preprocessor *pp, const char *text, size_t length,
                                 int in_block_comment)
{
    for (size_t cursor = 0; cursor < length;) {
        if (in_block_comment) {
            if (cursor + 1 < length && text[cursor] == '*' && text[cursor + 1] == '/') {
                in_block_comment = 0;
                cursor += 2;
            } else {
                ++cursor;
            }
            continue;
        }
        if (text[cursor] == '"' || text[cursor] == '\'') {
            char quote = text[cursor++];
            while (cursor < length) {
                if (text[cursor] == '\\' && cursor + 1 < length) cursor += 2;
                else if (text[cursor++] == quote) break;
            }
            continue;
        }
        if ((pp->assembly && text[cursor] == '#') ||
            (cursor + 1 < length && text[cursor] == '/' && text[cursor + 1] == '/')) {
            while (cursor < length && text[cursor] != '\n') ++cursor;
            continue;
        }
        if (cursor + 1 < length && text[cursor] == '/' && text[cursor + 1] == '*') {
            in_block_comment = 1;
            cursor += 2;
            continue;
        }
        size_t number_length = pp_number_length(text, length, cursor);
        if (number_length) {
            cursor += number_length;
            continue;
        }
        size_t name_length = 0;
        if (!identifier_at(text, length, cursor, &name_length)) {
            ++cursor;
            continue;
        }
        MacroDefinition *macro = find_macro_definition(pp, text + cursor, name_length);
        if (macro && !macro->function_like) {
            MacroDefinition *alias = function_macro_alias(pp, macro, NULL, 0);
            if (alias) macro = alias;
        }
        int kind = builtin_query_kind(pp, text + cursor, name_length);
        size_t end = cursor + name_length;
        if ((macro && macro->function_like) || kind >= 3) {
            size_t open = skip_macro_space(text, length, end);
            if (open == length) return 1;
            if (text[open] == '(') {
                MacroArgument *arguments = NULL;
                size_t count = 0, after = 0;
                int status = parse_macro_arguments(text, length, open, &after,
                                                   &arguments, &count);
                free(arguments);
                if (status <= 0) return status < 0 ? -1 : 1;
            }
        }
        cursor = end;
    }
    return 0;
}

typedef struct {
    uint64_t bits;
    int is_unsigned;
} IfValue;

typedef struct {
    const char *cursor;
    int ok;
    unsigned depth;
} IfParser;

typedef struct {
    const char *text;
    int precedence;
    int op;
} IfOperator;

enum {
    IF_OR = 1, IF_AND, IF_BIT_OR, IF_BIT_XOR, IF_BIT_AND, IF_EQ, IF_NE,
    IF_LT, IF_LE, IF_GT, IF_GE, IF_SHL, IF_SHR, IF_ADD, IF_SUB, IF_MUL,
    IF_DIV, IF_MOD
};

/**
 * @brief Skips whitespace and comments in a conditional expression.
 * @param parser Expression parser to advance.
 * @return No value; marks an unterminated block comment invalid.
 */
static void if_skip(IfParser *parser)
{
    for (;;) {
        while (isspace((unsigned char)*parser->cursor)) ++parser->cursor;
        if (parser->cursor[0] == '/' && parser->cursor[1] == '/') {
            parser->cursor += strlen(parser->cursor);
            return;
        }
        if (parser->cursor[0] != '/' || parser->cursor[1] != '*') return;
        const char *end = strstr(parser->cursor + 2, "*/");
        if (!end) {
            parser->ok = 0;
            return;
        }
        parser->cursor = end + 2;
    }
}

/**
 * @brief Interprets an unsigned bit pattern as a signed 64-bit value.
 * @param bits Value bits.
 * @return Corresponding signed value without implementation-defined conversion.
 */
static int64_t if_signed(uint64_t bits)
{
    return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
}

/**
 * @brief Reads an integer or character constant in a conditional expression.
 * @param parser Parser positioned at the constant.
 * @return The value and signedness; marks invalid constants in parser.
 */
static IfValue if_constant(IfParser *parser)
{
    IfValue result = {0, 0};
    const char *start = parser->cursor;
    if (*start == '\'') {
        const char *p = start + 1;
        unsigned value = 0;
        if (*p == '\\') {
            ++p;
            switch (*p) {
            case 'a': value = 7; ++p; break;
            case 'b': value = 8; ++p; break;
            case 'f': value = 12; ++p; break;
            case 'n': value = 10; ++p; break;
            case 'r': value = 13; ++p; break;
            case 't': value = 9; ++p; break;
            case 'v': value = 11; ++p; break;
            case 'e': case 'E': value = 27; ++p; break;
            case '\\': case '\'': case '"': case '?':
                value = (unsigned char)*p++; break;
            case 'x': {
                ++p;
                if (!isxdigit((unsigned char)*p)) { parser->ok = 0; return result; }
                while (isxdigit((unsigned char)*p)) {
                    unsigned digit = isdigit((unsigned char)*p) ? (unsigned)(*p - '0') :
                                     (unsigned)(tolower((unsigned char)*p) - 'a' + 10);
                    if (value > (255U - digit) / 16U) { parser->ok = 0; return result; }
                    value = value * 16U + digit;
                    ++p;
                }
                break;
            }
            default:
                if (*p < '0' || *p > '7') { parser->ok = 0; return result; }
                for (int i = 0; i < 3 && *p >= '0' && *p <= '7'; ++i)
                    value = value * 8U + (unsigned)(*p++ - '0');
                if (value > 255) { parser->ok = 0; return result; }
                break;
            }
        } else {
            if (!*p || *p == '\'' || *p == '\n') { parser->ok = 0; return result; }
            value = (unsigned char)*p++;
        }
        if (*p != '\'') { parser->ok = 0; return result; }
        parser->cursor = p + 1;
        result.bits = value;
        return result;
    }
    if (!isdigit((unsigned char)*start)) { parser->ok = 0; return result; }
    errno = 0;
    char *end;
    unsigned long long value = strtoull(start, &end, 0);
    if (errno == ERANGE || end == start) { parser->ok = 0; return result; }
    char suffix[4];
    size_t n = 0;
    while (isalnum((unsigned char)*end) || *end == '_') {
        if (n == sizeof suffix - 1) { parser->ok = 0; return result; }
        if (n && suffix[n - 1] == 'l' && (*end == 'l' || *end == 'L') &&
            end[-1] != *end) { parser->ok = 0; return result; }
        suffix[n++] = (char)tolower((unsigned char)*end++);
    }
    suffix[n] = '\0';
    if (strcmp(suffix, "") && strcmp(suffix, "u") && strcmp(suffix, "l") &&
        strcmp(suffix, "ll") && strcmp(suffix, "ul") && strcmp(suffix, "lu") &&
        strcmp(suffix, "ull") && strcmp(suffix, "llu")) {
        parser->ok = 0;
        return result;
    }
    result.bits = (uint64_t)value;
    result.is_unsigned = strchr(suffix, 'u') != NULL;
    if (!result.is_unsigned && result.bits > INT64_MAX) {
        if (*start != '0') { parser->ok = 0; return result; }
        result.is_unsigned = 1;
    }
    parser->cursor = end;
    return result;
}

static IfValue if_conditional(IfParser *parser, int evaluate);

/**
 * @brief Parses a primary or unary expression.
 * @param parser Expression parser.
 * @param evaluate Whether arithmetic errors should be evaluated.
 * @return Parsed value and signedness.
 */
static IfValue if_unary(IfParser *parser, int evaluate)
{
    IfValue value = {0, 0};
    if (++parser->depth > 128) {
        parser->ok = 0;
        --parser->depth;
        return value;
    }
    if_skip(parser);
    char ch = *parser->cursor;
    if (ch == '+' || ch == '-' || ch == '!' || ch == '~') {
        ++parser->cursor;
        value = if_unary(parser, evaluate);
        if (ch == '!') {
            value.bits = !value.bits;
            value.is_unsigned = 0;
        } else if (ch == '~') {
            value.bits = ~value.bits;
        } else if (ch == '-') {
            if (evaluate && !value.is_unsigned && value.bits == (UINT64_C(1) << 63))
                parser->ok = 0;
            value.bits = UINT64_C(0) - value.bits;
        }
    } else if (ch == '(') {
        ++parser->cursor;
        value = if_conditional(parser, evaluate);
        if_skip(parser);
        if (*parser->cursor != ')') parser->ok = 0;
        else ++parser->cursor;
    } else if (crunch_identifier_start((unsigned char)ch)) {
        ++parser->cursor;
        while (crunch_identifier_continue((unsigned char)*parser->cursor))
            ++parser->cursor;
    } else {
        value = if_constant(parser);
    }
    --parser->depth;
    return value;
}

/**
 * @brief Applies a binary operator using C-style 64-bit preprocessing arithmetic.
 * @param parser Parser receiving arithmetic errors.
 * @param op Operator to apply.
 * @param left Left operand.
 * @param right Right operand.
 * @param evaluate Whether to check and compute the operation.
 * @return Result with the appropriate signedness.
 */
static IfValue if_apply(IfParser *parser, int op, IfValue left, IfValue right,
                        int evaluate)
{
    int uns = left.is_unsigned || right.is_unsigned;
    IfValue out = {0, uns};
    uint64_t a = left.bits, b = right.bits, result = 0;
    if (op == IF_SHL || op == IF_SHR) out.is_unsigned = left.is_unsigned;
    if (op == IF_OR || op == IF_AND || (op >= IF_EQ && op <= IF_GE))
        out.is_unsigned = 0;
    if (!evaluate) return out;
    switch (op) {
    case IF_OR: out.bits = a != 0 || b != 0; break;
    case IF_AND: out.bits = a != 0 && b != 0; break;
    case IF_BIT_OR: out.bits = a | b; break;
    case IF_BIT_XOR: out.bits = a ^ b; break;
    case IF_BIT_AND: out.bits = a & b; break;
    case IF_EQ: out.bits = a == b; break;
    case IF_NE: out.bits = a != b; break;
    case IF_LT: out.bits = uns ? a < b : if_signed(a) < if_signed(b); break;
    case IF_LE: out.bits = uns ? a <= b : if_signed(a) <= if_signed(b); break;
    case IF_GT: out.bits = uns ? a > b : if_signed(a) > if_signed(b); break;
    case IF_GE: out.bits = uns ? a >= b : if_signed(a) >= if_signed(b); break;
    case IF_SHL:
    case IF_SHR:
        if ((!right.is_unsigned && if_signed(b) < 0) || b >= 64) {
            parser->ok = 0;
            break;
        }
        if (op == IF_SHL) {
            if (!left.is_unsigned && (if_signed(a) < 0 || a > ((uint64_t)INT64_MAX >> b)))
                parser->ok = 0;
            out.bits = a << b;
        } else {
            out.bits = a >> b;
            if (!left.is_unsigned && if_signed(a) < 0 && b)
                out.bits |= UINT64_MAX << (64 - b);
        }
        break;
    case IF_ADD:
        result = a + b;
        if (!uns && ((~(a ^ b) & (a ^ result)) >> 63)) parser->ok = 0;
        out.bits = result;
        break;
    case IF_SUB:
        result = a - b;
        if (!uns && (((a ^ b) & (a ^ result)) >> 63)) parser->ok = 0;
        out.bits = result;
        break;
    case IF_MUL:
        if (!uns) {
            int negative = (int)((a ^ b) >> 63);
            uint64_t x = a >> 63 ? UINT64_C(0) - a : a;
            uint64_t y = b >> 63 ? UINT64_C(0) - b : b;
            uint64_t limit = negative ? UINT64_C(1) << 63 : (uint64_t)INT64_MAX;
            if (x && y > limit / x) parser->ok = 0;
        }
        out.bits = a * b;
        break;
    case IF_DIV: case IF_MOD:
        if (!b || (!uns && a == (UINT64_C(1) << 63) && b == UINT64_MAX)) {
            parser->ok = 0;
            break;
        }
        if (uns) out.bits = op == IF_DIV ? a / b : a % b;
        else out.bits = (uint64_t)(op == IF_DIV ? if_signed(a) / if_signed(b) :
                                                  if_signed(a) % if_signed(b));
        break;
    default: parser->ok = 0; break;
    }
    return out;
}

/**
 * @brief Parses a binary expression using precedence climbing.
 * @param parser Expression parser.
 * @param minimum Minimum precedence accepted at this level.
 * @param evaluate Whether arithmetic should be evaluated.
 * @return Parsed value and signedness.
 */
static IfValue if_binary(IfParser *parser, int minimum, int evaluate)
{
    static const IfOperator ops[] = {
        {"||", 1, IF_OR}, {"&&", 2, IF_AND}, {"==", 6, IF_EQ}, {"!=", 6, IF_NE},
        {"<=", 7, IF_LE}, {">=", 7, IF_GE}, {"<<", 8, IF_SHL}, {">>", 8, IF_SHR},
        {"|", 3, IF_BIT_OR}, {"^", 4, IF_BIT_XOR}, {"&", 5, IF_BIT_AND},
        {"<", 7, IF_LT}, {">", 7, IF_GT}, {"+", 9, IF_ADD}, {"-", 9, IF_SUB},
        {"*", 10, IF_MUL}, {"/", 10, IF_DIV}, {"%", 10, IF_MOD}
    };
    IfValue left = if_unary(parser, evaluate);
    while (parser->ok) {
        if_skip(parser);
        const IfOperator *found = NULL;
        for (size_t i = 0; i < sizeof ops / sizeof ops[0]; ++i) {
            size_t len = strlen(ops[i].text);
            if (strncmp(parser->cursor, ops[i].text, len) == 0) {
                found = &ops[i];
                break;
            }
        }
        if (!found || found->precedence < minimum) break;
        parser->cursor += strlen(found->text);
        int right_evaluate = evaluate &&
            !(found->op == IF_AND && !left.bits) &&
            !(found->op == IF_OR && left.bits);
        IfValue right = if_binary(parser, found->precedence + 1, right_evaluate);
        left = if_apply(parser, found->op, left, right, evaluate);
    }
    return left;
}

/**
 * @brief Parses a conditional expression, including the ternary operator.
 * @param parser Expression parser.
 * @param evaluate Whether arithmetic should be evaluated.
 * @return Parsed value and signedness.
 */
static IfValue if_conditional(IfParser *parser, int evaluate)
{
    IfValue condition = if_binary(parser, 1, evaluate);
    if_skip(parser);
    if (*parser->cursor != '?') return condition;
    if (++parser->depth > 128) {
        parser->ok = 0;
        --parser->depth;
        return condition;
    }
    ++parser->cursor;
    IfValue yes = if_conditional(parser, evaluate && condition.bits != 0);
    if_skip(parser);
    if (*parser->cursor != ':') parser->ok = 0;
    else ++parser->cursor;
    IfValue no = if_conditional(parser, evaluate && condition.bits == 0);
    IfValue result = condition.bits ? yes : no;
    result.is_unsigned = yes.is_unsigned || no.is_unsigned;
    --parser->depth;
    return result;
}

/**
 * @brief Expands and evaluates an active preprocessor condition.
 * @param pp Preprocessor containing macro definitions.
 * @param text Condition bytes.
 * @param length Number of condition bytes.
 * @param result Output receiving whether the condition is nonzero.
 * @return One on success, zero for invalid expressions or allocation failure.
 */
static int evaluate_if(Preprocessor *pp, const char *text, size_t length, int *result)
{
    TextBuffer expanded = {0};
    const MacroDefinition *active_macros[PP_MAX_MACRO_EXPANSION_DEPTH] = {0};
    int comment = 0, ok = 0, previous_if_expression = pp->if_expression;
    pp->if_expression = 1;
    if (!expand_text(pp, text, length, active_macros, 0, &comment, &expanded))
        goto done;
    for (size_t i = 0; i < expanded.length; ++i)
        if (expanded.data[i] == PP_TOKEN_BOUNDARY) expanded.data[i] = ' ';
    IfParser parser = {expanded.data ? expanded.data : "", 1, 0};
    IfValue value = if_conditional(&parser, 1);
    if_skip(&parser);
    if (parser.ok && !*parser.cursor) {
        *result = value.bits != 0;
        ok = 1;
    }
done:
    pp->if_expression = previous_if_expression;
    free(expanded.data);
    return ok;
}

static int process_file(Preprocessor *pp, const char *path, FILE *input,
                        int close_input, int append_final_newline, size_t origin);

/**
 * @brief Joins directory and header-name slices with a directory separator.
 * @param directory Directory bytes, or an empty slice for the current directory.
 * @param directory_length Number of directory bytes.
 * @param name Header-name bytes.
 * @param name_length Number of header-name bytes.
 * @return Caller-owned NUL-terminated path, or NULL on overflow/allocation failure.
 */
static char *make_include_path(const char *directory, size_t directory_length,
                               const char *name, size_t name_length)
{
    size_t separator_length = directory_length == 0 ? 0 : 1;
    if (directory_length > (size_t)-1 - separator_length ||
        name_length > (size_t)-1 - directory_length - separator_length - 1)
        return NULL;
    char *path = malloc(directory_length + separator_length + name_length + 1);
    if (!path) return NULL;
    if (directory_length != 0) {
        memcpy(path, directory, directory_length);
        path[directory_length] = '/';
    }
    memcpy(path + directory_length + separator_length, name, name_length);
    path[directory_length + separator_length + name_length] = '\0';
    return path;
}

/**
 * @brief Identifies an open file for pragma-once comparisons.
 * @param input Open source or header stream.
 * @param identity Output file device and inode.
 * @return Nonzero on success; zero if the file cannot be inspected.
 */
static int once_file_identity(FILE *input, OnceFile *identity)
{
    struct stat status;
    if (fstat(fileno(input), &status) != 0) return 0;
    identity->device = (unsigned long)status.st_dev;
    identity->inode = (unsigned long)status.st_ino;
    return 1;
}

/**
 * @brief Checks whether a file has already reached an active pragma once.
 * @param pp Preprocessor storing file identities.
 * @param identity Identity to find.
 * @return Nonzero if the file is registered; otherwise zero.
 */
static int once_file_registered(const Preprocessor *pp, const OnceFile *identity)
{
    for (size_t i = 0; i < pp->once_count; ++i)
        if (pp->once_files[i].device == identity->device &&
            pp->once_files[i].inode == identity->inode)
            return 1;
    return 0;
}

/**
 * @brief Records an open file as included once.
 * @param pp Preprocessor storing file identities.
 * @param input Open file containing an active pragma once.
 * @return Nonzero on success; zero on inspection or allocation failure.
 */
static int register_once_file(Preprocessor *pp, FILE *input)
{
    OnceFile identity;
    if (!once_file_identity(input, &identity)) return 0;
    if (once_file_registered(pp, &identity)) return 1;
    if (pp->once_count >= SIZE_MAX / sizeof(*pp->once_files)) return 0;
    OnceFile *grown = realloc(pp->once_files,
                              (pp->once_count + 1) * sizeof(*pp->once_files));
    if (!grown) return 0;
    pp->once_files = grown;
    pp->once_files[pp->once_count++] = identity;
    return 1;
}

/**
 * @brief Attempts to open and preprocess a header in a single directory.
 * @param pp Preprocessor receiving the header output and macro updates.
 * @param directory Directory bytes to search.
 * @param directory_length Number of directory bytes.
 * @param name Header-name bytes.
 * @param length Number of header-name bytes.
 * @param origin Index of the search directory, or SIZE_MAX for a local include.
 * @return One if processed/guard-skipped, -1 if unopened, or zero on processing failure.
 */
static int include_from_directory(Preprocessor *pp, const char *directory,
                                  size_t directory_length, const char *name,
                                  size_t length, size_t origin)
{
    char *path = make_include_path(directory, directory_length, name, length);
    if (!path) {
        fprintf(stderr, "crunch-pp: out of memory or include path too long\n");
        return 0;
    }
    const char *guard = guard_for_path(pp, path);
    if (guard && macro_defined(pp, guard, strlen(guard))) {
        free(path);
        return 1;
    }
    FILE *header = fopen(path, "rb");
    if (!header) {
        free(path);
        return -1;
    }
    if (pp->once_count) {
        OnceFile identity;
        if (!once_file_identity(header, &identity)) {
            fprintf(stderr, "crunch-pp: cannot inspect included header '%s'\n", path);
            fclose(header);
            free(path);
            return 0;
        }
        if (once_file_registered(pp, &identity)) {
            fclose(header);
            free(path);
            return 1;
        }
    }
    int ok = process_file(pp, path, header, 1, 1, origin);
    free(path);
    return ok;
}

/**
 * @brief Adds a unique directory to the include search list.
 * @param pp Preprocessor receiving the directory.
 * @param directory Directory bytes, with an empty slice meaning the current directory.
 * @param length Number of directory bytes.
 * @return Nonzero on success; zero on allocation failure.
 */
static int add_search_directory(Preprocessor *pp, const char *directory, size_t length)
{
    char *path = copy_text(directory, length);
    if (!path) return 0;
    struct stat status;
    int stat_ok = stat(length ? path : ".", &status) == 0;
    for (size_t i = 0; i < pp->search_dir_count; ++i) {
        IncludeDirectory *existing = &pp->search_dirs[i];
        if (!strcmp(existing->path, path) ||
            (stat_ok && existing->stat_ok &&
             existing->device == (unsigned long)status.st_dev &&
             existing->inode == (unsigned long)status.st_ino)) {
            free(path);
            return 1;
        }
    }
    if (pp->search_dir_count >= SIZE_MAX / sizeof(*pp->search_dirs)) {
        free(path);
        return 0;
    }
    IncludeDirectory *grown = realloc(pp->search_dirs,
                                      (pp->search_dir_count + 1) * sizeof(*grown));
    if (!grown) {
        free(path);
        return 0;
    }
    pp->search_dirs = grown;
    IncludeDirectory item = {0};
    item.path = path;
    item.stat_ok = stat_ok;
    if (stat_ok) {
        item.device = (unsigned long)status.st_dev;
        item.inode = (unsigned long)status.st_ino;
    }
    pp->search_dirs[pp->search_dir_count++] = item;
    return 1;
}

/**
 * @brief Appends directories from a colon-separated include path.
 * @param pp Preprocessor receiving the directories.
 * @param path Directory list, or NULL for no directories.
 * @return Nonzero on success; zero on allocation failure.
 */
static int add_search_path(Preprocessor *pp, const char *path)
{
    while (path) {
        const char *separator = strchr(path, ':');
        size_t length = separator ? (size_t)(separator - path) : strlen(path);
        if (!add_search_directory(pp, path, length)) return 0;
        if (!separator) break;
        path = separator + 1;
    }
    return 1;
}

/**
 * @brief Finds and preprocesses a header using the configured include search order.
 * @param pp Preprocessor supplying search paths and receiving expanded output.
 * @param name Relative header-name bytes.
 * @param length Number of header-name bytes.
 * @param includer Including file path for quoted includes; NULL for angle-bracket includes.
 * @param include_next Nonzero to resume after the directory that supplied the current file.
 * @return One if processed/guard-skipped, or zero after reporting failure.
 * Searches the includer's directory, -I paths, CRUNCH_INCLUDE_PATH, the built-in
 * directory, configured include_path entries, then system directories;
 * angle-bracket includes skip the includer's directory.
 */
static int include_header(Preprocessor *pp, const char *name, size_t length,
                          const char *includer, int include_next)
{
    if (!safe_header_name(name, length)) {
        fprintf(stderr, "crunch-pp: invalid or overlong included header name\n");
        return 0;
    }
    int result = -1;
    if (includer && !include_next) {
        const char *slash = strrchr(includer, '/');
        size_t directory_length = slash ? (size_t)(slash - includer) : 0;
        if (slash && directory_length == 0) directory_length = 1;
        result = include_from_directory(pp, includer, directory_length,
                                        name, length, SIZE_MAX);
    }
    size_t start = 0;
    if (include_next && pp->depth && pp->origins[pp->depth - 1] != SIZE_MAX)
        start = pp->origins[pp->depth - 1] + 1;
    for (size_t i = start; result < 0 && i < pp->search_dir_count; ++i)
        result = include_from_directory(pp, pp->search_dirs[i].path,
                                        strlen(pp->search_dirs[i].path), name, length,
                                        i);
    if (result < 0) {
        fprintf(stderr, "crunch-pp: cannot open included header '%.*s'\n",
                (int)length, name);
        return 0;
    }
    return result;
}

/**
 * @brief Parses a literal or expanded header operand.
 * @param pp Preprocessor supplying macro definitions.
 * @param text Operand bytes.
 * @param length Number of operand bytes.
 * @param expanded Owned buffer used when macro expansion is needed.
 * @param name Output header-name slice, borrowing text or expanded.
 * @param name_length Output number of header-name bytes.
 * @param quoted Output flag for quoted rather than angle-bracket lookup.
 * @return Nonzero on success; zero for malformed input or allocation failure.
 */
static int parse_header_operand(Preprocessor *pp, const char *text, size_t length,
                                TextBuffer *expanded, const char **name,
                                size_t *name_length, int *quoted)
{
    size_t cursor = skip_macro_space(text, length, 0);
    if (cursor >= length || (text[cursor] != '<' && text[cursor] != '"')) {
        const MacroDefinition *active_macros[PP_MAX_MACRO_EXPANSION_DEPTH] = {0};
        int comment = 0;
        ++pp->if_argument_depth;
        int ok = expand_text(pp, text, length, active_macros, 0, &comment, expanded);
        --pp->if_argument_depth;
        if (!ok) return 0;
        text = expanded->data ? expanded->data : "";
        length = expanded->length;
        cursor = skip_macro_space(text, length, 0);
    }
    if (cursor >= length || (text[cursor] != '<' && text[cursor] != '"')) return 0;
    *quoted = text[cursor] == '"';
    char close = *quoted ? '"' : '>';
    *name = text + cursor + 1;
    const char *end = memchr(*name, close, length - cursor - 1);
    if (!end) return 0;
    size_t trailing = skip_macro_space(text, length, (size_t)(end - text) + 1);
    if (trailing < length &&
        !(trailing + 1 < length && text[trailing] == '/' && text[trailing + 1] == '/'))
        return 0;
    *name_length = (size_t)(end - *name);
    if (!*quoted && memchr(*name, PP_TOKEN_BOUNDARY, *name_length)) {
        TextBuffer clean = {0};
        for (size_t i = 0; i < *name_length; ++i) {
            if ((*name)[i] != PP_TOKEN_BOUNDARY &&
                !buffer_append_char(&clean, (*name)[i])) {
                free(clean.data);
                return 0;
            }
        }
        if (!clean.data && !buffer_append(&clean, "", 0)) return 0;
        free(expanded->data);
        *expanded = clean;
        *name = expanded->data;
        *name_length = expanded->length;
    }
    return 1;
}

/**
 * @brief Checks whether one include-search candidate can be opened as a header.
 * @param directory Directory bytes.
 * @param directory_length Number of directory bytes.
 * @param name Header-name bytes.
 * @param length Number of header-name bytes.
 * @return One if found, zero if absent or a directory, or -1 on allocation failure.
 */
static int header_exists_in_directory(const char *directory, size_t directory_length,
                                      const char *name, size_t length)
{
    char *path = make_include_path(directory, directory_length, name, length);
    if (!path) return -1;
    FILE *input = fopen(path, "rb");
    free(path);
    if (!input) return 0;
    struct stat status;
    int found = fstat(fileno(input), &status) == 0 && !S_ISDIR(status.st_mode);
    fclose(input);
    return found;
}

/**
 * @brief Checks header availability using the normal include-search order.
 * @param pp Preprocessor containing the include stack and search directories.
 * @param name Header-name bytes.
 * @param length Number of header-name bytes.
 * @param quoted Whether to search the current file's directory first.
 * @param include_next Whether to start after the current search directory.
 * @return One if found, zero if absent, or -1 for unsafe names/allocation failure.
 */
static int header_exists(Preprocessor *pp, const char *name, size_t length,
                         int quoted, int include_next)
{
    if (!safe_header_name(name, length)) return -1;
    if (quoted && !include_next && pp->depth) {
        const char *path = pp->stack[pp->depth - 1];
        const char *slash = strrchr(path, '/');
        size_t directory_length = slash ? (size_t)(slash - path) : 0;
        if (slash && !directory_length) directory_length = 1;
        int result = header_exists_in_directory(path, directory_length, name, length);
        if (result != 0) return result;
    }
    size_t start = 0;
    if (include_next && pp->depth && pp->origins[pp->depth - 1] != SIZE_MAX)
        start = pp->origins[pp->depth - 1] + 1;
    for (size_t i = start; i < pp->search_dir_count; ++i) {
        int result = header_exists_in_directory(pp->search_dirs[i].path,
                                                strlen(pp->search_dirs[i].path),
                                                name, length);
        if (result != 0) return result;
    }
    return 0;
}

/**
 * @brief Evaluates one header-availability operator in an expression.
 * @param pp Preprocessor containing include paths and macros.
 * @param text Expression bytes.
 * @param length Number of expression bytes.
 * @param identifier_start Offset at the operator name.
 * @param identifier_end Offset immediately after the operator name.
 * @param include_next Whether to search after the current header's directory.
 * @param after Output offset immediately after the closing parenthesis.
 * @param found Output flag indicating whether the header exists.
 * @return Nonzero on success; zero for malformed input or lookup failure.
 */
static int resolve_has_include(Preprocessor *pp, const char *text, size_t length,
                               size_t identifier_start, size_t identifier_end, int include_next,
                               size_t *after, int *found)
{
    size_t start = 0, end = 0, close = 0;
    if (!has_include_bounds(text, length, identifier_end, &start, &end, &close))
        return 0;
    TextBuffer expanded = {0};
    const char *name = NULL;
    size_t name_length = 0;
    int quoted = 0, ok = 0;
    size_t saved_line = pp->source_line;
    if (pp->track_source_lines)
        pp->source_line += source_breaks(pp, text, identifier_start, start);
    int parsed = parse_header_operand(pp, text + start, end - start, &expanded,
                                      &name, &name_length, &quoted);
    pp->source_line = saved_line;
    if (parsed) {
        int result = header_exists(pp, name, name_length, quoted, include_next);
        if (result >= 0) {
            *found = result;
            *after = close;
            ok = 1;
        }
    }
    free(expanded.data);
    return ok;
}

/**
 * @brief Expands an include operand if needed and preprocesses the resulting header.
 * @param pp Preprocessor supplying macro definitions and include paths.
 * @param directive Include directive and its unexpanded operand.
 * @param path Physical path of the including file.
 * @param logical_path Logical filename used in diagnostics.
 * @param logical_line Logical line number used in diagnostics.
 * @return Nonzero on success; zero after a diagnostic on failure.
 */
static int process_include_directive(Preprocessor *pp, const Directive *directive,
                                     const char *path, const char *logical_path,
                                     size_t logical_line)
{
    TextBuffer expanded = {0};
    const char *name = NULL;
    size_t name_length = 0;
    int quoted = 0;
    if (!parse_header_operand(pp, directive->argument, directive->argument_length,
                              &expanded, &name, &name_length, &quoted))
        goto invalid;
    int ok = include_header(pp, name, name_length, quoted ? path : NULL,
                            directive->kind == DIRECTIVE_INCLUDE_NEXT);
    free(expanded.data);
    return ok;
invalid:
    fprintf(stderr, "crunch-pp: malformed include operand in '%s' line %zu\n",
            logical_path, logical_line);
    free(expanded.data);
    return 0;
}

/**
 * @brief Removes leading and trailing spaces and line endings in place.
 * @param text Mutable NUL-terminated string to trim.
 * @return Pointer to the first non-whitespace character in text.
 */
static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t') ++text;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\n' || end[-1] == '\r'))
        --end;
    *end = '\0';
    return text;
}

/**
 * @brief Appends one include directory to the configuration search path.
 * @param pp Preprocessor state whose configuration path is extended.
 * @param value Directory path to append.
 * @return Nonzero on success; zero if allocation fails.
 */
static int append_conf_path(Preprocessor *pp, const char *value)
{
    size_t old_length = pp->conf_include_path
        ? strlen(pp->conf_include_path) : 0;
    size_t value_length = strlen(value);
    char *joined = realloc(pp->conf_include_path,
                           old_length + (old_length ? 1 : 0) + value_length + 1);
    if (!joined) return 0;
    if (old_length) joined[old_length++] = ':';
    memcpy(joined + old_length, value, value_length + 1);
    pp->conf_include_path = joined;
    return 1;
}

/**
 * @brief Reads configured include directories from a configuration file.
 * @param pp Preprocessor state whose configuration include path is populated.
 * @param conf_path Configuration file to read.
 * @return Nonzero on success or if the file is absent; zero on other errors.
 */
static int load_conf_file(Preprocessor *pp, const char *conf_path)
{
    FILE *conf = fopen(conf_path, "r");
    if (!conf) {
        if (errno == ENOENT) return 1;
        fprintf(stderr, "crunch-pp: cannot read configuration file '%s': %s\n",
                conf_path, strerror(errno));
        return 0;
    }
    char *line = NULL;
    size_t capacity = 0;
    unsigned long line_number = 0;
    int ok = 1;
    while (ok && getline(&line, &capacity, conf) != -1) {
        ++line_number;
        char *text = trim(line);
        if (*text == '\0' || *text == '#') continue;
        char *equals = strchr(text, '=');
        if (!equals) {
            fprintf(stderr, "crunch-pp: %s:%lu: expected 'key = value'\n",
                    conf_path, line_number);
            ok = 0;
            break;
        }
        *equals = '\0';
        char *key = trim(text), *value = trim(equals + 1);
        if (!strcmp(key, "assembler") || !strcmp(key, "linker")) {
            if (strcmp(value, "gnu") && strcmp(value, "crunch")) {
                fprintf(stderr, "crunch-pp: %s:%lu: %s must be gnu or crunch\n",
                        conf_path, line_number, key);
                ok = 0;
            }
            continue;
        }
        if (!strcmp(key, "assembler_path") || !strcmp(key, "linker_path") ||
            !strcmp(key, "archiver_path")) {
            if (value[0] != '/') {
                fprintf(stderr, "crunch-pp: %s:%lu: %s must be an absolute path\n",
                        conf_path, line_number, key);
                ok = 0;
            }
            continue;
        }
        int is_include_path = !strcmp(key, "include_path");
        if (!is_include_path && strcmp(key, "library_path") != 0) {
            fprintf(stderr, "crunch-pp: %s:%lu: unknown key '%s'\n",
                    conf_path, line_number, key);
            ok = 0;
            break;
        }
        for (const char *p = value; ; ++p) {
            if (*p == ':' || *p == '\0') {
                if (p == value || p[-1] == ':') {
                    fprintf(stderr, "crunch-pp: %s:%lu: empty directory in %s\n",
                            conf_path, line_number, key);
                    ok = 0;
                    break;
                }
                if (*p == '\0') break;
            }
        }
        if (ok && is_include_path && !append_conf_path(pp, value)) {
            fprintf(stderr, "crunch-pp: out of memory\n");
            ok = 0;
        }
    }
    if (ok && ferror(conf)) {
        fprintf(stderr, "crunch-pp: failed reading '%s'\n", conf_path);
        ok = 0;
    }
    free(line);
    fclose(conf);
    return ok;
}

/**
 * @brief Parses an expanded #line operand.
 * @param text Expanded operand bytes.
 * @param length Operand length.
 * @param line Output next logical line number.
 * @param filename Output allocated filename, or NULL if omitted.
 * @return Nonzero on success; zero for malformed input or allocation failure.
 */
static int parse_line_operand(const char *text, size_t length, size_t *line, char **filename)
{
    size_t cursor = skip_macro_space(text, length, 0), number = 0;
    TextBuffer name = {0};
    *filename = NULL;
    if (cursor >= length || text[cursor] < '0' || text[cursor] > '9') return 0;
    while (cursor < length && text[cursor] >= '0' && text[cursor] <= '9') {
        unsigned digit = (unsigned)(text[cursor++] - '0');
        if (number > (2147483647u - digit) / 10) return 0;
        number = number * 10 + digit;
    }
    if (!number) return 0;
    cursor = skip_macro_space(text, length, cursor);
    if (cursor < length && text[cursor] == '"') {
        ++cursor;
        while (cursor < length && text[cursor] != '"') {
            unsigned char ch = (unsigned char)text[cursor++];
            if (ch == '\n' || ch == '\r') goto fail;
            if (ch == '\\') {
                if (cursor == length) goto fail;
                ch = (unsigned char)text[cursor++];
                if (ch == '\\' || ch == '"' || ch == '\'') {
                    /* These escapes represent the escaped character. */
                } else if (ch == 'n') ch = '\n';
                else if (ch == 'r') ch = '\r';
                else if (ch == 't') ch = '\t';
                else if (ch == 'a') ch = '\a';
                else if (ch == 'b') ch = '\b';
                else if (ch == 'f') ch = '\f';
                else if (ch == 'v') ch = '\v';
                else if (ch >= '0' && ch <= '7') {
                    unsigned value = ch - '0';
                    for (int i = 0; i < 2 && cursor < length &&
                         text[cursor] >= '0' && text[cursor] <= '7'; ++i)
                        value = value * 8 + (unsigned)(text[cursor++] - '0');
                    if (value > 255) goto fail;
                    ch = (unsigned char)value;
                } else if (ch == 'x') {
                    unsigned value = 0, digits = 0;
                    while (cursor < length && isxdigit((unsigned char)text[cursor])) {
                        unsigned char d = (unsigned char)text[cursor++];
                        unsigned v = d <= '9' ? d - '0' : (d | 32) - 'a' + 10;
                        if (value > (255u - v) / 16) goto fail;
                        value = value * 16 + v;
                        ++digits;
                    }
                    if (!digits) goto fail;
                    ch = (unsigned char)value;
                } else goto fail;
            }
            if (!ch || !buffer_append_char(&name, (char)ch))
                goto fail;
        }
        if (cursor >= length || text[cursor++] != '"') goto fail;
        if (!name.data && !buffer_append(&name, "", 0)) goto fail;
        cursor = skip_macro_space(text, length, cursor);
    }
    if (cursor < length && !(cursor + 1 < length && text[cursor] == '/' &&
                             text[cursor + 1] == '/'))
        goto fail;
    *line = number;
    *filename = name.data;
    return 1;
fail:
    free(name.data);
    return 0;
}

/**
 * @brief Writes unexpanded warning text with comments replaced by whitespace.
 * @param text Warning message bytes.
 * @param length Number of bytes in text.
 * @return No value; writes the message to standard error.
 */
static void write_warning_text(const char *text, size_t length)
{
    char quote = '\0';
    for (size_t i = 0; i < length;) {
        char ch = text[i++];
        if (quote) {
            fputc(ch, stderr);
            if (ch == '\\' && i < length) fputc(text[i++], stderr);
            else if (ch == quote) quote = '\0';
        } else if (ch == '"' || ch == '\'') {
            quote = ch;
            fputc(ch, stderr);
        } else if (ch == '/' && i < length && text[i] == '/') {
            break;
        } else if (ch == '/' && i < length && text[i] == '*') {
            ++i;
            while (i + 1 < length && !(text[i] == '*' && text[i + 1] == '/'))
                ++i;
            if (i + 1 < length) i += 2;
            else i = length;
            fputc(' ', stderr);
        } else {
            fputc(ch, stderr);
        }
    }
}

/**
 * @brief Checks whether the last token in a buffer is a preprocessing number.
 * @param text Source bytes ending immediately before a boundary.
 * @param length Number of bytes in text.
 * @return Nonzero if the final token is a preprocessing number.
 */
static int ends_with_pp_number(const char *text, size_t length)
{
    size_t start = length;
    while (start) {
        unsigned char ch = (unsigned char)text[start - 1];
        if (crunch_identifier_continue(ch) || ch == '.') {
            --start;
        } else if ((ch == '+' || ch == '-') && start > 1 &&
                   (text[start - 2] == 'e' || text[start - 2] == 'E' ||
                    text[start - 2] == 'p' || text[start - 2] == 'P')) {
            --start;
        } else {
            break;
        }
    }
    for (size_t i = start; i < length; ++i) {
        if (i && text[i] != '.' && crunch_identifier_continue((unsigned char)text[i - 1]))
            continue;
        if (pp_number_length(text, length, i) == length - i) return 1;
    }
    return 0;
}

/**
 * @brief Tests whether adjacent bytes can merge preprocessing tokens.
 * @param text Source bytes ending immediately before a boundary.
 * @param length Number of bytes in text.
 * @param right First byte after the boundary.
 * @return Nonzero if a separating space is required.
 */
static int token_boundary_needs_space(const char *text, size_t length, unsigned char right)
{
    unsigned char left = (unsigned char)text[length - 1];
    if (crunch_identifier_continue(left) && crunch_identifier_continue(right)) return 1;
    if ((right == '.' && ends_with_pp_number(text, length)) ||
        (left == '.' && isdigit(right))) return 1;
    if (crunch_identifier_continue(left) && (right == '"' || right == '\'')) return 1;
    if ((left == 'e' || left == 'E' || left == 'p' || left == 'P') &&
        (right == '+' || right == '-') && ends_with_pp_number(text, length)) return 1;
    if (left == '.' && right == '.') return 1;
    switch (left) {
    case '+': return right == '+' || right == '=';
    case '-': return right == '-' || right == '=' || right == '>';
    case '*': case '/': case '%': case '^': case '!': case '=':
        return right == '=' || (left == '/' && (right == '/' || right == '*')) ||
               (left == '%' && (right == ':' || right == '>'));
    case '&': return right == '&' || right == '=';
    case '|': return right == '|' || right == '=';
    case '<': return right == '<' || right == '=' || right == ':' || right == '%';
    case '>': return right == '>' || right == '=';
    case ':': return right == '>' || right == '%';
    case '#': return right == '#';
    default: return 0;
    }
}

/**
 * @brief Serializes internal token boundaries without altering strings or comments.
 * @param text Expanded source buffer.
 * @param assembly Whether assembler comments should be preserved.
 * @param in_block_comment Whether the buffer starts inside a block comment.
 * @return No value; compacts the buffer in place.
 */
static void serialize_token_boundaries(TextBuffer *text, int assembly, int in_block_comment)
{
    size_t read = 0, written = 0;
    while (read < text->length) {
        char ch = text->data[read++];
        if (in_block_comment) {
            text->data[written++] = ch;
            if (ch == '*' && read < text->length && text->data[read] == '/') {
                text->data[written++] = text->data[read++];
                in_block_comment = 0;
            }
        } else if (ch == PP_TOKEN_BOUNDARY) {
            while (read < text->length && text->data[read] == PP_TOKEN_BOUNDARY) ++read;
            if (written && read < text->length &&
                token_boundary_needs_space(text->data, written,
                                           (unsigned char)text->data[read]))
                text->data[written++] = ' ';
        } else if (ch == '"' || ch == '\'') {
            char quote = ch;
            text->data[written++] = ch;
            while (read < text->length) {
                ch = text->data[read++];
                text->data[written++] = ch;
                if (ch == '\\' && read < text->length)
                    text->data[written++] = text->data[read++];
                else if (ch == quote)
                    break;
            }
        } else if ((ch == '/' && read < text->length && text->data[read] == '/') ||
                   (assembly && ch == '#')) {
            text->data[written++] = ch;
            while (read < text->length && text->data[read] != '\n')
                text->data[written++] = text->data[read++];
        } else if (ch == '/' && read < text->length && text->data[read] == '*') {
            text->data[written++] = ch;
            text->data[written++] = text->data[read++];
            in_block_comment = 1;
        } else {
            text->data[written++] = ch;
        }
    }
    text->length = written;
    if (text->data) text->data[written] = '\0';
}

/**
 * @brief Expands buffered ordinary source and restores its following location.
 * @param pp Preprocessor supplying macro definitions and output.
 * @param pending Buffered source; emptied on success.
 * @param splices Removed newline offsets in the buffered source.
 * @param path Logical source path.
 * @param line First logical line in the buffer.
 * @param next_line Logical line following the buffer.
 * @param in_block_comment Input/output macro-expansion comment state.
 * @return Nonzero on success; zero after an expansion or output failure.
 */
static int flush_macro_text(Preprocessor *pp, TextBuffer *pending, SpliceOffsets *splices,
                            const char *path,
                            size_t line, size_t next_line, int *in_block_comment)
{
    if (!pending->length) return 1;
    TextBuffer expanded = {0};
    const MacroDefinition *active[PP_MAX_MACRO_EXPANSION_DEPTH] = {0};
    size_t saved_line = pp->source_line;
    int saved_tracking = pp->track_source_lines, saved_markers = pp->emit_source_markers;
    const char *saved_text = pp->mapped_text;
    size_t saved_length = pp->mapped_length;
    const SpliceOffsets *saved_splices = pp->mapped_splices;
    pp->source_line = line + leading_splices(splices, 0);
    pp->mapped_text = pending->data;
    pp->mapped_length = pending->length;
    pp->mapped_splices = splices;
    pp->track_source_lines = 1;
    pp->emit_source_markers = !pp->assembly;
    int saved_comment = *in_block_comment;
    int ok = expand_text(pp, pending->data, pending->length, active, 0,
                         in_block_comment, &expanded);
    pp->source_line = saved_line;
    pp->track_source_lines = saved_tracking;
    pp->emit_source_markers = saved_markers;
    pp->mapped_text = saved_text;
    pp->mapped_length = saved_length;
    pp->mapped_splices = saved_splices;
    if (!ok) {
        fprintf(stderr, "crunch-pp: failed expanding macros in '%s' line %zu\n", path, line);
    } else {
        serialize_token_boundaries(&expanded, pp->assembly, saved_comment);
        if (expanded.length &&
            fwrite(expanded.data, 1, expanded.length, pp->out) != expanded.length)
            ok = 0;
    }
    if (ok && next_line > line + 1 && pending->data[pending->length - 1] == '\n') {
        if (*in_block_comment && fputs("*/\n", pp->out) == EOF) ok = 0;
        if (ok && !write_marker(pp, path, next_line)) ok = 0;
        if (ok && *in_block_comment && fputs("/*", pp->out) == EOF) ok = 0;
    }
    free(expanded.data);
    if (ok) {
        pending->length = 0;
        pending->data[0] = '\0';
        splices->count = 0;
    }
    return ok;
}

/**
 * @brief Applies a supported packing directive and emits its resulting state.
 * @param pp Shared preprocessing state, including the packing stack.
 * @param directive Directive operand to expand and parse.
 * @return Nonzero on success; zero for malformed input, stack underflow, or resource failure.
 */
static int process_pack_directive(Preprocessor *pp, const Directive *directive)
{
    TextBuffer expanded = {0};
    const MacroDefinition *active[PP_MAX_MACRO_EXPANSION_DEPTH] = {0};
    int comment = 0, action = 0, has_value = 0, ok = 0;
    unsigned int value = 0;
    size_t cursor, start, word_length;
    if (!expand_text(pp, directive->argument, directive->argument_length,
                     active, 0, &comment, &expanded))
        goto done;
    const char *text = expanded.data ? expanded.data : "";
    size_t length = expanded.length;
    cursor = skip_macro_space(text, length, 0);
    if (cursor >= length || text[cursor++] != '(') goto done;
    cursor = skip_macro_space(text, length, cursor);
    if (identifier_at(text, length, cursor, &word_length)) {
        if (word_length == 4 && !memcmp(text + cursor, "push", 4))
            action = 1;
        else if (word_length == 3 && !memcmp(text + cursor, "pop", 3))
            action = 2;
        else
            goto done;
        cursor = skip_macro_space(text, length, cursor + word_length);
        if (cursor < length && text[cursor] == ',') {
            if (action != 1) goto done;
            ++cursor;
            has_value = 1;
        }
    } else if (cursor < length && text[cursor] != ')') {
        has_value = 1;
    }
    if (has_value) {
        cursor = skip_macro_space(text, length, cursor);
        start = cursor;
        while (cursor < length && text[cursor] >= '0' && text[cursor] <= '9')
            ++cursor;
        size_t digits = cursor - start;
        if (digits == 1 && strchr("01248", text[start]))
            value = (unsigned int)(text[start] - '0');
        else if (digits == 2 && !memcmp(text + start, "16", 2))
            value = 16;
        else
            goto done;
        cursor = skip_macro_space(text, length, cursor);
    }
    if (cursor >= length || text[cursor++] != ')' ||
        skip_macro_space(text, length, cursor) != length)
        goto done;
    if (action == 1) {
        if (pp->pack_depth == pp->pack_capacity) {
            size_t capacity = pp->pack_capacity ? pp->pack_capacity * 2 : 16;
            if (capacity < pp->pack_capacity || capacity > SIZE_MAX / sizeof(*pp->pack_stack))
                goto done;
            unsigned int *stack = realloc(pp->pack_stack, capacity * sizeof(*stack));
            if (!stack) goto done;
            pp->pack_stack = stack;
            pp->pack_capacity = capacity;
        }
        pp->pack_stack[pp->pack_depth++] = pp->pack_alignment;
        if (has_value) pp->pack_alignment = value;
    } else if (action == 2) {
        if (!pp->pack_depth) goto done;
        pp->pack_alignment = pp->pack_stack[--pp->pack_depth];
    } else {
        pp->pack_alignment = value;
    }
    ok = pp->assembly ? fputc('\n', pp->out) != EOF
                      : fprintf(pp->out, "#pragma pack(%u)\n", pp->pack_alignment) >= 0;
done:
    free(expanded.data);
    return ok;
}

/**
 * @brief Expands source directives and macros, recursively processing includes.
 * @param pp Preprocessor holding output, macro tables, guards, and the include stack.
 * @param path Borrowed source path used while this call is active.
 * @param input Open source stream positioned at the beginning of the file.
 * @param close_input Whether to close input before returning, including on failure.
 * @param append_final_newline Whether to terminate a nonempty included file with a newline.
 * @param origin Search-directory index, or SIZE_MAX for a source or local include.
 * @return One on success, or zero on preprocessing/I/O/allocation failure; output may be partial.
 */
static int process_file(Preprocessor *pp, const char *path, FILE *input,
                        int close_input, int append_final_newline, size_t origin)
{
    Conditional conditionals[PP_MAX_INCLUDE_DEPTH];
    size_t conditional_depth = 0, source_line = 1, logical_line = 1;
    char *guard_candidate = NULL;
    int guard_defined = 0, guard_closed = 0, guard_valid = 1;
    char *logical_filename = NULL;
    int active = 1, ok = 1, had_input = 0, last_line_ended = 1, physical_ended = 1;
    int first_directive = 1;
    int in_block_comment = 0;
    int macro_expansion_block_comment = 0;
    int hidden_block_comment = 0;
    TextBuffer pending = {0};
    SpliceOffsets pending_splices = {0};
    size_t pending_line = 0, pending_next_line = 0;
    const char *saved_mapped_text = pp->mapped_text;
    size_t saved_mapped_length = pp->mapped_length;
    const SpliceOffsets *saved_mapped_splices = pp->mapped_splices;
    int saved_tracking = pp->track_source_lines;
    if (pp->depth >= PP_MAX_INCLUDE_DEPTH) {
        int cyclic = 0;
        for (size_t i = 0; i < pp->depth; ++i)
            if (!strcmp(pp->stack[i], path))
                cyclic = 1;
        fprintf(stderr, cyclic ? "crunch-pp: cyclic include of '%s'\n"
                               : "crunch-pp: maximum include depth exceeded at '%s'\n",
                path);
        if (close_input) fclose(input);
        return 0;
    }
    pp->stack[pp->depth] = path;
    pp->origins[pp->depth++] = origin;
    if (!write_marker(pp, path, source_line)) ok = 0;
    while (ok) {
        const char *logical_path = logical_filename ? logical_filename : path;
        size_t next_logical_line = 0;
        pp->source_path = logical_path;
        pp->source_line = logical_line;
        size_t length = 0;
        char *line = NULL;
        SpliceOffsets line_splices = {0};
        int line_status = read_line(input, &line, &length);
        if (line_status <= 0) {
            if (line_status < 0) {
                fprintf(stderr, "crunch-pp: failed reading '%s' or allocating input buffer\n", path);
                ok = 0;
            }
            break;
        }
        if (memchr(line, '\0', length)) {
            fprintf(stderr, "crunch-pp: %s:%zu: raw NUL byte in source\n",
                    path, source_line);
            free(line);
            ok = 0;
            break;
        }
        had_input = 1;
        physical_ended = length && line[length - 1] == '\n';
        size_t continuations = 0;
        if (continuation_suffix(line, length) &&
            !splice_lines(input, path, source_line, &line, &length,
                          &continuations, &physical_ended, &line_splices)) {
            free(line_splices.offsets);
            ok = 0;
            break;
        }
        last_line_ended = length && line[length - 1] == '\n';
        int has_code;
        int previous_block_comment = in_block_comment;
        Directive directive = parse_directive(line, length, &in_block_comment,
                                              &has_code, pp->assembly);
        if (guard_closed && (directive.kind != DIRECTIVE_NONE || has_code))
            guard_valid = 0;
        if (guard_candidate && conditional_depth == 1 &&
            (directive.kind == DIRECTIVE_ELSE || directive.kind == DIRECTIVE_ELIF))
            guard_valid = 0;
        if (directive.kind != DIRECTIVE_NONE && pending.length &&
            !flush_macro_text(pp, &pending, &pending_splices, logical_path, pending_line,
                              pending_next_line, &macro_expansion_block_comment))
            ok = 0;
        pp->mapped_text = line;
        pp->mapped_length = length;
        pp->mapped_splices = &line_splices;
        pp->track_source_lines = 1;
        if (directive.kind != DIRECTIVE_NONE && directive.argument)
            pp->source_line = logical_line +
                              leading_splices(&line_splices, (size_t)(directive.argument - line));
        if (directive.kind != DIRECTIVE_NONE && previous_block_comment &&
            !hidden_block_comment && fputs("*/", pp->out) == EOF)
            ok = 0;
        if (directive.kind != DIRECTIVE_NONE || !active)
            update_block_comment_state(line, line + length,
                                       &macro_expansion_block_comment,
                                       pp->assembly && directive.kind == DIRECTIVE_NONE);
        if (directive.kind != DIRECTIVE_NONE || !active)
            hidden_block_comment = in_block_comment;
        int was_first_directive = first_directive;
        if (directive.kind == DIRECTIVE_NONE) {
            if (has_code) first_directive = 0;
        } else {
            first_directive = 0;
        }
        int continuations_buffered = 0;
        if (!ok) {
            free(line_splices.offsets);
            free(line);
            break;
        }
        switch (directive.kind) {
        case DIRECTIVE_NONE:
            if (active) {
                const char *text = line;
                size_t text_length = length;
                if (hidden_block_comment) {
                    const char *end = strstr(line, "*/");
                    if (end) {
                        text = end + 2;
                        text_length = length - (size_t)(text - line);
                        for (const char *p = line; p < text; ++p)
                            if (fputc(' ', pp->out) == EOF) ok = 0;
                        hidden_block_comment = 0;
                        macro_expansion_block_comment = 0;
                    } else {
                        text = line + length;
                        text_length = 0;
                        if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
                    }
                }
                if (ok && text_length) {
                    size_t offset = (size_t)(text - line);
                    if (!pending.length) pending_line = logical_line;
                    size_t base = pending.length;
                    if (!buffer_append(&pending, text, text_length)) ok = 0;
                    for (size_t i = 0; ok && i < line_splices.count; ++i) {
                        size_t position = line_splices.offsets[i];
                        if (position < offset) {
                            if (base == 0) ++pending_line;
                        } else if (!add_splice_offset(&pending_splices, base + position - offset)) {
                            ok = 0;
                        }
                    }
                    if (!ok)
                        fprintf(stderr, "crunch-pp: out of memory buffering source in '%s' line %zu\n",
                                logical_path, pending_line);
                    continuations_buffered = 1;
                    pending_next_line = logical_line + 1 + continuations;
                    if (ok) {
                        int needs_line = macro_text_needs_line(
                            pp, pending.data, pending.length, macro_expansion_block_comment);
                        if (needs_line < 0) {
                            fprintf(stderr, "crunch-pp: out of memory inspecting macros in '%s' line %zu\n",
                                    logical_path, pending_line);
                            ok = 0;
                        }
                        else if (!needs_line)
                            ok = flush_macro_text(pp, &pending, &pending_splices, logical_path, pending_line,
                                                  pending_next_line, &macro_expansion_block_comment);
                    }
                }
            } else if (last_line_ended && fputc('\n', pp->out) == EOF) {
                ok = 0;
            }
            break;
        case DIRECTIVE_IF:
        case DIRECTIVE_IFDEF:
        case DIRECTIVE_IFNDEF:
            if (conditional_depth == PP_MAX_INCLUDE_DEPTH) {
                fprintf(stderr, "crunch-pp: conditional nesting limit exceeded in '%s'\n", path);
                ok = 0;
                break;
            }
            if (was_first_directive && directive.kind == DIRECTIVE_IFNDEF) {
                guard_candidate = copy_text(directive.argument,
                                            directive.argument_length);
                if (!guard_candidate) {
                    fprintf(stderr, "crunch-pp: out of memory recording header guard\n");
                    ok = 0;
                    break;
                }
            }
            {
                int condition = 0;
                if (active && directive.kind == DIRECTIVE_IF) {
                    if (!evaluate_if(pp, directive.argument, directive.argument_length,
                                     &condition)) {
                        fprintf(stderr, "crunch-pp: invalid #if expression in '%s' line %zu\n",
                                logical_path, logical_line);
                        ok = 0;
                        break;
                    }
                } else if (active) {
                    condition = macro_defined(pp, directive.argument,
                                              directive.argument_length) ==
                                (directive.kind == DIRECTIVE_IFDEF);
                }
                conditionals[conditional_depth++] = (Conditional){active, condition, 0};
                active = active && condition;
            }
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_DEFINE:
            if (active && !parse_macro_definition(pp, directive.argument,
                                                  directive.argument_length)) {
                fprintf(stderr, "crunch-pp: malformed or duplicate #define, or out of memory, in '%s' line %zu\n",
                        logical_path, logical_line);
                ok = 0;
            }
            if (ok && active && guard_candidate && conditional_depth == 1 &&
                strlen(guard_candidate) == directive.argument_length &&
                memcmp(guard_candidate, directive.argument,
                       directive.argument_length) == 0)
                guard_defined = 1;
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_UNDEF:
            if (active)
                undefine_macro(pp, directive.argument,
                               directive.argument_length);
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_DEFINE_VALUE:
        case DIRECTIVE_MACRO:
            if (active && !parse_macro_definition(pp, directive.argument,
                                                  directive.argument_length)) {
                fprintf(stderr, "crunch-pp: malformed or duplicate #%s, or out of memory, in '%s' line %zu\n",
                        directive.kind == DIRECTIVE_MACRO ? "macro" : "define",
                        logical_path, logical_line);
                ok = 0;
            }
            if (ok && active && directive.kind == DIRECTIVE_DEFINE_VALUE &&
                guard_candidate && conditional_depth == 1) {
                size_t name_length = 0;
                if (identifier_at(directive.argument, directive.argument_length,
                                  0, &name_length) &&
                    strlen(guard_candidate) == name_length &&
                    memcmp(guard_candidate, directive.argument, name_length) == 0)
                    guard_defined = 1;
            }
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_ELIF:
            if (!conditional_depth || conditionals[conditional_depth - 1].else_seen) {
                fprintf(stderr, "crunch-pp: unmatched #elif or #elif after #else in '%s' line %zu\n",
                        logical_path, logical_line);
                ok = 0;
                break;
            }
            {
                Conditional *conditional = &conditionals[conditional_depth - 1];
                int condition = 0;
                if (conditional->parent_active && !conditional->branch_taken &&
                    !evaluate_if(pp, directive.argument, directive.argument_length, &condition)) {
                    fprintf(stderr, "crunch-pp: invalid #elif expression in '%s' line %zu\n",
                            logical_path, logical_line);
                    ok = 0;
                    break;
                }
                active = conditional->parent_active && !conditional->branch_taken && condition;
                if (active) conditional->branch_taken = 1;
            }
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_ELSE:
            if (!conditional_depth ||
                conditionals[conditional_depth - 1].else_seen) {
                fprintf(stderr, "crunch-pp: unmatched or duplicate #else in '%s'\n", path);
                ok = 0;
                break;
            }
            conditionals[conditional_depth - 1].else_seen = 1;
            active = conditionals[conditional_depth - 1].parent_active &&
                     !conditionals[conditional_depth - 1].branch_taken;
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_ENDIF:
            if (!conditional_depth) {
                fprintf(stderr, "crunch-pp: unmatched #endif in '%s'\n", path);
                ok = 0;
                break;
            }
            if (guard_candidate && conditional_depth == 1)
                guard_closed = 1;
            active = conditionals[--conditional_depth].parent_active;
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_INCLUDE:
        case DIRECTIVE_INCLUDE_NEXT:
            if (active) {
                if (!process_include_directive(pp, &directive, path,
                                               logical_path, logical_line) ||
                    !write_marker(pp, logical_path, logical_line + 1)) {
                    ok = 0;
                }
            } else if (last_line_ended && fputc('\n', pp->out) == EOF) {
                ok = 0;
            }
            break;
        case DIRECTIVE_ERROR:
        case DIRECTIVE_WARNING:
            if (active) {
                size_t message_length = directive.argument_length;
                while (message_length &&
                       (directive.argument[message_length - 1] == '\n' ||
                        directive.argument[message_length - 1] == '\r'))
                    --message_length;
                fprintf(stderr, "crunch-pp: %s:%zu: %s: ", logical_path, logical_line,
                        directive.kind == DIRECTIVE_ERROR ? "error" : "warning");
                if (directive.kind == DIRECTIVE_ERROR)
                    fwrite(directive.argument, 1, message_length, stderr);
                else
                    write_warning_text(directive.argument, message_length);
                fputc('\n', stderr);
                if (directive.kind == DIRECTIVE_ERROR) ok = 0;
            }
            if (ok && last_line_ended && fputc('\n', pp->out) == EOF) {
                ok = 0;
            }
            break;
        case DIRECTIVE_PRAGMA_ONCE:
            if (active) {
                if (pp->depth == 1)
                    fprintf(stderr, "crunch-pp: %s:%zu: warning: #pragma once in main file\n",
                            logical_path, logical_line);
                if (!trailing_comment_only(directive.argument))
                    fprintf(stderr, "crunch-pp: %s:%zu: warning: extra tokens at end of #pragma directive\n",
                            logical_path, logical_line);
                if (!register_once_file(pp, input)) {
                    fprintf(stderr, "crunch-pp: cannot record #pragma once in '%s'\n", path);
                    ok = 0;
                }
            }
            if (ok && last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_PRAGMA_DIAGNOSTIC:
            if (last_line_ended && fputc('\n', pp->out) == EOF) ok = 0;
            break;
        case DIRECTIVE_PRAGMA_PACK:
            if (active) {
                if (!process_pack_directive(pp, &directive)) {
                    fprintf(stderr, "crunch-pp: %s:%zu: invalid or unsupported #pragma pack\n",
                            logical_path, logical_line);
                    ok = 0;
                }
            } else if (last_line_ended && fputc('\n', pp->out) == EOF) {
                ok = 0;
            }
            break;
        case DIRECTIVE_PRAGMA_GCC_OPTIONS:
            if (active && pp->assembly) {
                fprintf(stderr, "crunch-pp: %s:%zu: unsupported GCC option pragma in assembly\n",
                        logical_path, logical_line);
                ok = 0;
            } else if (active) {
                TextBuffer preserved = {0};
                if (!buffer_append(&preserved, directive.argument, directive.argument_length)) {
                    fprintf(stderr, "crunch-pp: out of memory preserving GCC option pragma\n");
                    ok = 0;
                } else {
                    normalize_multiline_macro(&preserved, 0);
                    if (fputs("#pragma GCC ", pp->out) == EOF ||
                        fwrite(preserved.data, 1, preserved.length, pp->out) != preserved.length ||
                        (last_line_ended && fputc('\n', pp->out) == EOF))
                        ok = 0;
                }
                free(preserved.data);
            } else if (last_line_ended && fputc('\n', pp->out) == EOF) {
                ok = 0;
            }
            break;
        case DIRECTIVE_LINE:
            if (active) {
                TextBuffer expanded = {0};
                const MacroDefinition *active_macros[PP_MAX_MACRO_EXPANSION_DEPTH];
                int comment = 0;
                char *new_filename = NULL;
                if (!expand_text(pp, directive.argument, directive.argument_length,
                                 active_macros, 0, &comment, &expanded) ||
                    !parse_line_operand(expanded.data ? expanded.data : "", expanded.length,
                                        &next_logical_line, &new_filename)) {
                    fprintf(stderr, "crunch-pp: %s:%zu: invalid #line directive\n",
                            logical_path, logical_line);
                    ok = 0;
                } else if (new_filename) {
                    free(logical_filename);
                    logical_filename = new_filename;
                }
                free(expanded.data);
            } else if (last_line_ended && fputc('\n', pp->out) == EOF) {
                ok = 0;
            }
            break;
        case DIRECTIVE_OTHER:
        case DIRECTIVE_UNSUPPORTED:
            if (active) {
                fprintf(stderr, "crunch-pp: unsupported preprocessor directive in '%s'\n", path);
                ok = 0;
            } else if (last_line_ended && fputc('\n', pp->out) == EOF) {
                ok = 0;
            }
            break;
        case DIRECTIVE_INVALID:
            fprintf(stderr, "crunch-pp: malformed preprocessor directive in '%s' line %zu\n",
                    logical_path, logical_line);
            ok = 0;
            break;
        }
        if (directive.kind == DIRECTIVE_INVALID && ok) ok = 0;
        if (directive.kind != DIRECTIVE_NONE && !ok)
            fprintf(stderr, "crunch-pp: while processing '%s'\n", path);
        for (size_t i = 0; ok && !continuations_buffered && i < continuations; ++i)
            if (fputc('\n', pp->out) == EOF) ok = 0;
        if (ok && next_logical_line &&
            !write_marker(pp, logical_filename ? logical_filename : path, next_logical_line))
            ok = 0;
        pp->mapped_text = NULL;
        pp->mapped_splices = NULL;
        free(line_splices.offsets);
        free(line);
        source_line += 1 + continuations;
        logical_line = next_logical_line ? next_logical_line : logical_line + 1 + continuations;
    }
    if (ok && pending.length &&
        !flush_macro_text(pp, &pending, &pending_splices, logical_filename ? logical_filename : path,
                          pending_line, pending_next_line, &macro_expansion_block_comment))
        ok = 0;
    if (ok && conditional_depth) {
        fprintf(stderr, "crunch-pp: unterminated conditional directive in '%s'\n", path);
        ok = 0;
    }
    if (ok && guard_candidate && guard_defined && guard_closed && guard_valid &&
        !register_guard(pp, path, guard_candidate, strlen(guard_candidate))) {
        fprintf(stderr, "crunch-pp: out of memory recording header guard\n");
        ok = 0;
    }
    free(guard_candidate);
    free(logical_filename);
    free(pending.data);
    free(pending_splices.offsets);
    pp->mapped_text = saved_mapped_text;
    pp->mapped_length = saved_mapped_length;
    pp->mapped_splices = saved_mapped_splices;
    pp->track_source_lines = saved_tracking;
    --pp->depth;
    if (close_input && fclose(input) != 0) {
        fprintf(stderr, "crunch-pp: failed closing '%s'\n", path);
        ok = 0;
    }
    if (append_final_newline && had_input && !physical_ended &&
        fputc('\n', pp->out) == EOF) ok = 0;
    if (!ok && ferror(pp->out))
        fprintf(stderr, "crunch-pp: failed writing expanded source\n");
    return ok;
}

/**
 * @brief Releases all heap allocations owned by a preprocessor instance.
 * @param pp Preprocessor state to destroy.
 * @return No value; does not free pp itself.
 */
static void destroy_preprocessor(Preprocessor *pp)
{
    for (size_t i = 0; i < pp->macro_count; ++i) free(pp->macros[i]);
    free(pp->macros);
    for (size_t i = 0; i < pp->definition_count; ++i) {
        MacroDefinition *definition = &pp->definitions[i];
        free(definition->name);
        for (size_t j = 0; j < definition->parameter_count; ++j)
            free(definition->parameters[j]);
        free(definition->parameters);
        free(definition->replacement);
    }
    free(pp->definitions);
    for (size_t i = 0; i < pp->guard_count; ++i) {
        free(pp->guards[i].path);
        free(pp->guards[i].macro);
    }
    free(pp->guards);
    free(pp->once_files);
    free(pp->pack_stack);
    for (size_t i = 0; i < pp->search_dir_count; ++i)
        free(pp->search_dirs[i].path);
    free(pp->search_dirs);
    free(pp->conf_include_path);
    free(pp->cli_dirs);
    free(pp->system_dirs);
}

/**
 * @brief Applies one command-line macro definition.
 * @param pp Preprocessor state whose macro tables are updated.
 * @param text Command-line definition in NAME or NAME=VALUE form.
 * @return Nonzero on success; zero for malformed definitions or allocation failure.
 */
static int apply_cli_define(Preprocessor *pp, const char *text)
{
    size_t name_length = 0;
    if (!identifier_at(text, strlen(text), 0, &name_length)) {
        fprintf(stderr, "crunch-pp: invalid macro name in -D '%s'\n", text);
        return 0;
    }
    if (text[name_length] && text[name_length] != '=') {
        fprintf(stderr, "crunch-pp: invalid -D argument '%s'\n", text);
        return 0;
    }
    const char *value = text[name_length] == '=' ? text + name_length + 1 : "1";
    TextBuffer definition = {0};
    if (!buffer_append(&definition, text, name_length) ||
        !buffer_append(&definition, " ", 1) ||
        !buffer_append(&definition, value, strlen(value))) {
        free(definition.data);
        fprintf(stderr, "crunch-pp: out of memory recording macro\n");
        return 0;
    }
    int ok = parse_macro_definition(pp, definition.data, definition.length);
    free(definition.data);
    if (!ok)
        fprintf(stderr, "crunch-pp: failed to define command-line macro '%s'\n", text);
    return ok;
}

/**
 * @brief Defines a target macro unless supplied on the command line.
 * @param pp Preprocessor state whose macro tables are updated.
 * @param text Macro definition, including its name and replacement.
 * @return Nonzero on success; zero for an invalid definition or allocation failure.
 */
static int define_target_macro(Preprocessor *pp, const char *text)
{
    size_t name_length = 0, length = strlen(text);
    if (!identifier_at(text, length, 0, &name_length)) return 0;
    if (macro_defined(pp, text, name_length)) return 1;
    if (parse_macro_definition(pp, text, length)) return 1;
    fprintf(stderr, "crunch-pp: failed to define target macro '%.*s'\n",
            (int)name_length, text);
    return 0;
}

/**
 * @brief Defines the compiler and target macros not supplied on the command line.
 * @param pp Preprocessor state whose macro tables are updated.
 * @return Nonzero on success; zero for an invalid release version or allocation failure.
 */
static int define_builtin_macros(Preprocessor *pp)
{
    if (!define_target_macro(pp, "_Static_assert _Static_assert"))
        return 0;
    if (!define_macro(pp, "__FILE__", 8) || !define_macro(pp, "__LINE__", 8) ||
        !define_macro(pp, "__DATE__", 8) || !define_macro(pp, "__TIME__", 8) ||
        !define_macro(pp, "__has_include", 13) ||
        !define_macro(pp, "__has_include_next", 18) ||
        !define_macro(pp, "__has_builtin", 13) ||
        !define_macro(pp, "__has_attribute", 15)) {
        fprintf(stderr, "crunch-pp: out of memory recording built-in macros\n");
        return 0;
    }
    const char *names[] = {"__CRUNCH_MAJOR__", "__CRUNCH_MINOR__", "__CRUNCH_PATCHLEVEL__"};
    const char *part = CRUNCH_VERSION;
    char definition[128];
    for (size_t i = 0; i < 3; ++i) {
        const char *end = part;
        while (*end >= '0' && *end <= '9') ++end;
        size_t name_length = strlen(names[i]), length = (size_t)(end - part);
        if (!length || name_length + length + 2 > sizeof definition ||
            (i < 2 ? *end != '.' : *end != '\0')) {
            fprintf(stderr, "crunch-pp: invalid compiler release version\n");
            return 0;
        }
        memcpy(definition, names[i], name_length);
        definition[name_length] = '=';
        memcpy(definition + name_length + 1, part, length);
        definition[name_length + 1 + length] = '\0';
        if (!macro_defined(pp, names[i], name_length) &&
            !apply_cli_define(pp, definition))
            return 0;
        part = i < 2 ? end + 1 : end;
    }
    const char *flag_names[] = {"__CRUNCH__", "__linux__", "__x86_64__", "__unix__", "__unix",
                                "__NO_INLINE__"};
    const char *values[] = {"__CRUNCH__=1", "__linux__=1", "__x86_64__=1",
                            "__unix__=1", "__unix=1", "__NO_INLINE__=1"};
    for (size_t i = 0; i < sizeof flag_names / sizeof flag_names[0]; ++i) {
        if (!macro_defined(pp, flag_names[i], strlen(flag_names[i])) &&
            !apply_cli_define(pp, values[i]))
            return 0;
    }
    const char *wchar_names[] = {
        "__WCHAR_TYPE__", "__WCHAR_WIDTH__", "__WCHAR_MAX__", "__WCHAR_MIN__"
    };
    const char *wchar_values[] = {
        "__WCHAR_TYPE__=int", "__WCHAR_WIDTH__=32",
        "__WCHAR_MAX__=0x7fffffff", "__WCHAR_MIN__=(-__WCHAR_MAX__ - 1)"
    };
    for (size_t i = 0; i < 4; ++i) {
        if (!macro_defined(pp, wchar_names[i], strlen(wchar_names[i])) &&
            !apply_cli_define(pp, wchar_values[i]))
            return 0;
    }
    const unsigned widths[] = {8, 16, 32, 64};
    const char *signed_types[] = {"signed char", "short int", "int", "long int"};
    const char *unsigned_types[] = {
        "unsigned char", "short unsigned int", "unsigned int", "long unsigned int"
    };
    const char *signed_max[] = {"0x7f", "0x7fff", "0x7fffffff", "0x7fffffffffffffffL"};
    const char *unsigned_max[] = {
        "0xff", "0xffff", "0xffffffffU", "0xffffffffffffffffUL"
    };
    const char *groups[] = {"", "_LEAST", "_FAST"};
    for (size_t group = 0; group < 3; ++group) {
        for (size_t i = 0; i < 4; ++i) {
            size_t index = group == 2 && i > 0 ? 3 : i;
            for (size_t u = 0; u < 2; ++u) {
                const char *type = u ? unsigned_types[index] : signed_types[index];
                const char *max = u ? unsigned_max[index] : signed_max[index];
                int n = snprintf(definition, sizeof definition, "__%sINT%s%u_TYPE__ %s",
                                 u ? "U" : "", groups[group], widths[i], type);
                if (n < 0 || (size_t)n >= sizeof definition ||
                    !define_target_macro(pp, definition)) return 0;
                n = snprintf(definition, sizeof definition, "__%sINT%s%u_MAX__ %s",
                             u ? "U" : "", groups[group], widths[i], max);
                if (n < 0 || (size_t)n >= sizeof definition ||
                    !define_target_macro(pp, definition)) return 0;
            }
            if (group) {
                int n = snprintf(definition, sizeof definition, "__INT%s%u_WIDTH__ %u",
                                 groups[group], widths[i], widths[index]);
                if (n < 0 || (size_t)n >= sizeof definition ||
                    !define_target_macro(pp, definition)) return 0;
            }
        }
    }
    const char *integer_definitions[] = {
        "__CHAR_BIT__ 8", "__CHAR_UNSIGNED__ 1",
        "__LP64__ 1", "_LP64 1",
        "__ORDER_LITTLE_ENDIAN__ 1234", "__ORDER_BIG_ENDIAN__ 4321",
        "__ORDER_PDP_ENDIAN__ 3412",
        "__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__",
        "__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__",
        "__SCHAR_MAX__ 0x7f", "__SHRT_MAX__ 0x7fff",
        "__INT_MAX__ 0x7fffffff", "__LONG_MAX__ 0x7fffffffffffffffL",
        "__LONG_LONG_MAX__ 0x7fffffffffffffffLL",
        "__SCHAR_WIDTH__ 8", "__SHRT_WIDTH__ 16", "__INT_WIDTH__ 32",
        "__LONG_WIDTH__ 64", "__LONG_LONG_WIDTH__ 64",
        "__SIZEOF_SHORT__ 2", "__SIZEOF_INT__ 4", "__SIZEOF_LONG__ 8",
        "__SIZEOF_LONG_LONG__ 8", "__SIZEOF_POINTER__ 8", "__SIZEOF_FLOAT__ 4",
        "__SIZEOF_DOUBLE__ 8", "__SIZEOF_LONG_DOUBLE__ 16",
        "__SIZEOF_SIZE_T__ 8", "__SIZEOF_PTRDIFF_T__ 8",
        "__SIZEOF_WCHAR_T__ 4", "__SIZEOF_WINT_T__ 4",
        "__INTPTR_TYPE__ long int", "__UINTPTR_TYPE__ long unsigned int",
        "__INTPTR_MAX__ 0x7fffffffffffffffL", "__UINTPTR_MAX__ 0xffffffffffffffffUL",
        "__INTPTR_WIDTH__ 64", "__INTMAX_TYPE__ long int",
        "__UINTMAX_TYPE__ long unsigned int", "__INTMAX_MAX__ 0x7fffffffffffffffL",
        "__UINTMAX_MAX__ 0xffffffffffffffffUL", "__INTMAX_WIDTH__ 64",
        "__PTRDIFF_TYPE__ long int", "__PTRDIFF_MAX__ 0x7fffffffffffffffL",
        "__PTRDIFF_WIDTH__ 64", "__SIZE_TYPE__ long unsigned int",
        "__SIZE_MAX__ 0xffffffffffffffffUL", "__SIZE_WIDTH__ 64",
        "__SIG_ATOMIC_TYPE__ int", "__SIG_ATOMIC_MAX__ 0x7fffffff",
        "__SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)", "__SIG_ATOMIC_WIDTH__ 32",
        "__WINT_TYPE__ unsigned int", "__WINT_WIDTH__ 32",
        "__WINT_MAX__ 0xffffffffU", "__WINT_MIN__ 0U",
        "__INT8_C(c) c", "__INT16_C(c) c", "__INT32_C(c) c", "__INT64_C(c) c ## L",
        "__UINT8_C(c) c", "__UINT16_C(c) c", "__UINT32_C(c) c ## U",
        "__UINT64_C(c) c ## UL", "__INTMAX_C(c) c ## L", "__UINTMAX_C(c) c ## UL"
    };
    for (size_t i = 0; i < sizeof integer_definitions / sizeof integer_definitions[0]; ++i)
        if (!define_target_macro(pp, integer_definitions[i])) return 0;
    const char *floating_definitions[] = {
        "__FLT_RADIX__ 2", "__FLT_EVAL_METHOD__ 0", "__DECIMAL_DIG__ 21",
        "__FLT_MANT_DIG__ 24", "__FLT_DIG__ 6", "__FLT_DECIMAL_DIG__ 9",
        "__FLT_MIN_EXP__ (-125)", "__FLT_MIN_10_EXP__ (-37)",
        "__FLT_MAX_EXP__ 128", "__FLT_MAX_10_EXP__ 38",
        "__FLT_MAX__ 0x1.fffffep127F", "__FLT_MIN__ 0x1p-126F",
        "__FLT_EPSILON__ 0x1p-23F", "__FLT_DENORM_MIN__ 0x1p-149F",
        "__FLT_HAS_DENORM__ 1", "__FLT_HAS_INFINITY__ 1", "__FLT_HAS_QUIET_NAN__ 1",
        "__DBL_MANT_DIG__ 53", "__DBL_DIG__ 15", "__DBL_DECIMAL_DIG__ 17",
        "__DBL_MIN_EXP__ (-1021)", "__DBL_MIN_10_EXP__ (-307)",
        "__DBL_MAX_EXP__ 1024", "__DBL_MAX_10_EXP__ 308",
        "__DBL_MAX__ 0x1.fffffffffffffp1023", "__DBL_MIN__ 0x1p-1022",
        "__DBL_EPSILON__ 0x1p-52", "__DBL_DENORM_MIN__ 0x1p-1074",
        "__DBL_HAS_DENORM__ 1", "__DBL_HAS_INFINITY__ 1", "__DBL_HAS_QUIET_NAN__ 1",
        "__LDBL_MANT_DIG__ 64", "__LDBL_DIG__ 18", "__LDBL_DECIMAL_DIG__ 21",
        "__LDBL_MIN_EXP__ (-16381)", "__LDBL_MIN_10_EXP__ (-4931)",
        "__LDBL_MAX_EXP__ 16384", "__LDBL_MAX_10_EXP__ 4932",
        "__LDBL_MAX__ 0x1.fffffffffffffffep16383L", "__LDBL_MIN__ 0x1p-16382L",
        "__LDBL_EPSILON__ 0x1p-63L", "__LDBL_DENORM_MIN__ 0x1p-16445L",
        "__LDBL_HAS_DENORM__ 1", "__LDBL_HAS_INFINITY__ 1", "__LDBL_HAS_QUIET_NAN__ 1"
    };
    for (size_t i = 0; i < sizeof floating_definitions / sizeof floating_definitions[0]; ++i)
        if (!define_target_macro(pp, floating_definitions[i])) return 0;
    return 1;
}

/**
 * @brief Finds a name in the process environment.
 * @param envp Null-terminated environment vector.
 * @param name Environment variable name.
 * @return Value within the environment, or NULL when absent.
 */
static const char *environment_value(char **envp, const char *name)
{
    size_t length = strlen(name);
    for (size_t i = 0; envp && envp[i]; ++i)
        if (!strncmp(envp[i], name, length) && envp[i][length] == '=')
            return envp[i] + length + 1;
    return NULL;
}

/**
 * @brief Parses preprocessor options and expands the selected source to stdout.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @param envp Process environment vector.
 * @return Process exit status: zero on success, two for usage/setup errors, otherwise one.
 */
int main(int argc, char **argv, char **envp)
{
    const char *path = "-";
    int have_path = 0;
    int default_include = 1;
    int glibc_include = 0;
    Preprocessor pp = {0};
    pp.cli_dirs = calloc((size_t)argc, sizeof *pp.cli_dirs);
    pp.system_dirs = calloc((size_t)argc, sizeof *pp.system_dirs);
    if (!pp.cli_dirs || !pp.system_dirs) {
        fprintf(stderr, "crunch-pp: out of memory\n");
        free(pp.cli_dirs);
        free(pp.system_dirs);
        return 2;
    }
    const char **defines = calloc((size_t)argc, sizeof *defines);
    size_t define_count = 0;
    if (!defines) {
        fprintf(stderr, "crunch-pp: out of memory\n");
        free(pp.cli_dirs);
        free(pp.system_dirs);
        return 2;
    }
    for (int i = 1; i < argc; ++i) {
        const char *directory = NULL;
        if (strcmp(argv[i], "--assembly") == 0) {
            pp.assembly = 1;
            continue;
        } else if (strcmp(argv[i], "--no-crunch-includes") == 0) {
            default_include = 0;
            continue;
        } else if (strcmp(argv[i], "--glibc-includes") == 0) {
            default_include = 0;
            glibc_include = 1;
            continue;
        } else if (strcmp(argv[i], "--system-include") == 0 && i + 1 < argc &&
                   argv[i + 1][0] != '\0') {
            pp.system_dirs[pp.system_dir_count++] = argv[++i];
            continue;
        } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            defines[define_count++] = argv[++i];
            continue;
        } else if (strncmp(argv[i], "-D", 2) == 0 && argv[i][2] != '\0') {
            defines[define_count++] = argv[i] + 2;
            continue;
        } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            directory = argv[++i];
        } else if (strncmp(argv[i], "-I", 2) == 0 && argv[i][2] != '\0') {
            directory = argv[i] + 2;
        } else if (!have_path && (argv[i][0] != '-' || argv[i][1] == '\0')) {
            path = argv[i];
            have_path = 1;
            continue;
        }
        if (!directory || directory[0] == '\0') {
            fprintf(stderr, "usage: crunch-pp [--assembly] [--no-crunch-includes|--glibc-includes] [--system-include dir]... [-I dir]... [-D name[=value]]... [source-file|-]\n");
            free(defines);
            free(pp.cli_dirs);
            free(pp.system_dirs);
            return 2;
        }
        pp.cli_dirs[pp.cli_dir_count++] = directory;
    }
    pp.out = stdout;
    pp.include_dir = CRUNCH_DEFAULT_INCLUDE_DIR;
    pp.include_path = environment_value(envp, "CRUNCH_INCLUDE_PATH");
    pp.source_date_epoch = environment_value(envp, "SOURCE_DATE_EPOCH");
    const char *conf_path = environment_value(envp, "CRUNCH_CONF_FILE");
    if (!conf_path) conf_path = CRUNCH_DEFAULT_CONF_FILE;
    if (!load_conf_file(&pp, conf_path)) {
        free(defines);
        destroy_preprocessor(&pp);
        return 2;
    }
    int paths_ok = 1;
    for (size_t i = 0; paths_ok && i < pp.cli_dir_count; ++i)
        paths_ok = add_search_directory(&pp, pp.cli_dirs[i], strlen(pp.cli_dirs[i]));
    if (paths_ok) paths_ok = add_search_path(&pp, pp.include_path);
    if (paths_ok && default_include)
        paths_ok = add_search_directory(&pp, pp.include_dir, strlen(pp.include_dir));
    if (paths_ok) paths_ok = add_search_path(&pp, pp.conf_include_path);
    if (paths_ok && glibc_include) {
        char *directory = make_include_path(pp.include_dir, strlen(pp.include_dir), "glibc", 5);
        paths_ok = directory && add_search_directory(&pp, directory, strlen(directory));
        free(directory);
    }
    for (size_t i = 0; paths_ok && i < pp.system_dir_count; ++i)
        paths_ok = add_search_directory(&pp, pp.system_dirs[i], strlen(pp.system_dirs[i]));
    if (!paths_ok) {
        fprintf(stderr, "crunch-pp: out of memory recording include directories\n");
        free(defines);
        destroy_preprocessor(&pp);
        return 2;
    }
    for (size_t i = 0; i < define_count; ++i) {
        if (!apply_cli_define(&pp, defines[i])) {
            free(defines);
            destroy_preprocessor(&pp);
            return 2;
        }
    }
    free(defines);
    if (!define_builtin_macros(&pp)) {
        destroy_preprocessor(&pp);
        return 2;
    }
    if (pp.assembly && !macro_defined(&pp, "__ASSEMBLER__", sizeof "__ASSEMBLER__" - 1) &&
        !apply_cli_define(&pp, "__ASSEMBLER__=1")) {
        destroy_preprocessor(&pp);
        return 2;
    }
    FILE *input = stdin;
    if (strcmp(path, "-") != 0) {
        input = fopen(path, "rb");
        if (!input) {
            fprintf(stderr, "crunch-pp: cannot open source file '%s'\n", path);
            destroy_preprocessor(&pp);
            return 2;
        }
    }
    int ok = process_file(&pp, path, input, 0, 0, SIZE_MAX);
    if (input != stdin && fclose(input) != 0) {
        fprintf(stderr, "crunch-pp: failed closing '%s'\n", path);
        ok = 0;
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "crunch-pp: failed writing expanded source\n");
        ok = 0;
    }
    destroy_preprocessor(&pp);
    return ok ? 0 : 1;
}
