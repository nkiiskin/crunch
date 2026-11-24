#define _POSIX_C_SOURCE 200809L
#include "codegen/x86_64.h"
#include "common/diagnostic.h"
#include "common/runtime_routines.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/**
 * @brief Scans the restricted formats used by the IR reader.
 * @param input Input text.
 * @param format Format containing whitespace, literals, %s, %u, and %n.
 * @return Number of assignments, or EOF on input failure before an assignment.
 */
static int scan_ir(const char *input, const char *format, ...) {
    const char *start = input;
    int assigned = 0;
    va_list arguments;
    va_start(arguments, format);
    while (*format) {
        if (isspace((unsigned char)*format)) {
            while (isspace((unsigned char)*format))
                format++;
            while (isspace((unsigned char)*input))
                input++;
            continue;
        }
        if (*format != '%') {
            if (!*input) {
                va_end(arguments);
                return assigned ? assigned : EOF;
            }
            if (*input != *format) {
                va_end(arguments);
                return assigned;
            }
            input++;
            format++;
            continue;
        }
        format++;
        int suppress = *format == '*';
        if (suppress)
            format++;
        unsigned width = 0;
        while (*format >= '0' && *format <= '9')
            width = width * 10 + (unsigned)(*format++ - '0');
        char conversion = *format++;
        if (conversion == 'n') {
            *va_arg(arguments, int *) = (int)(input - start);
            continue;
        }
        while (isspace((unsigned char)*input))
            input++;
        if (!*input) {
            va_end(arguments);
            return assigned ? assigned : EOF;
        }
        if (conversion == 's') {
            char *destination = suppress ? NULL : va_arg(arguments, char *);
            unsigned count = 0;
            while (*input && !isspace((unsigned char)*input) && (!width || count < width)) {
                if (destination)
                    destination[count] = *input;
                count++;
                input++;
            }
            if (destination) {
                destination[count] = 0;
                assigned++;
            }
        } else if (conversion == 'u') {
            char *end;
            unsigned long value = strtoul(input, &end, 10);
            if (end == input) {
                va_end(arguments);
                return assigned;
            }
            *va_arg(arguments, unsigned *) = (unsigned)value;
            assigned++;
            input = end;
        }
    }
    va_end(arguments);
    return assigned;
}

/**
 * @brief Checks whether an IR line consists of the requested instruction name.
 * @param line Text of the IR instruction or source line.
 * @param instruction Text of the IR instruction or source line.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int is_instruction(const char *line, const char *instruction) {
    while (*line == ' ' || *line == '\t')
        line++;
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ' ||
                 line[n - 1] == '\t'))
        n--;
    return strlen(instruction) == n && !strncmp(line, instruction, n);
}
/**
 * @brief Copies generated assembly while applying safe textual optimizations.
 * @param generated Temporary FILE stream containing generated assembly to optimize.
 * @param destination Output FILE stream receiving optimized assembly.
 * @param error Buffer receiving the failure reason.
 * @param error_size Capacity of the error buffer.
 * @return Zero on success; nonzero when output or translation fails.
 */
static int optimize_assembly(FILE *generated, FILE *destination, char *error, size_t error_size) {
    char *line = NULL, *pending = NULL;
    size_t cap = 0;
    ssize_t len;
    rewind(generated);
    for (;;) {
        errno = 0;
        len = getline(&line, &cap, generated);
        if (len < 0)
            break;
        (void)len;
        char *cur = strdup(line);
        if (!cur) {
            free(line);
            free(pending);
            snprintf(error, error_size, "out of memory optimizing assembly");
            return 1;
        }
        if (pending && is_instruction(pending, "push %rax") && is_instruction(cur, "pop %rax")) {
            free(pending);
            pending = NULL;
            free(cur);
            continue;
        }
        if (pending && fputs(pending, destination) == EOF) {
            free(pending);
            free(cur);
            free(line);
            snprintf(error, error_size, "failed to write generated assembly");
            return 1;
        }
        free(pending);
        pending = cur;
    }
    if (ferror(generated) || !feof(generated)) {
        int read_errno = errno;
        free(line);
        free(pending);
        snprintf(error, error_size, "%s", read_errno == ENOMEM
                     ? "out of memory reading generated assembly"
                     : "failed to read generated assembly");
        return 1;
    }
    if (pending && fputs(pending, destination) == EOF) {
        free(line);
        free(pending);
        snprintf(error, error_size, "failed to write generated assembly");
        return 1;
    }
    free(line);
    free(pending);
    if (ferror(destination)) {
        snprintf(error, error_size, "failed to write generated assembly");
        return 1;
    }
    return 0;
}
typedef enum { T_BAD, T_BYTE, T_INT, T_LONG, T_REAL, T_VOID, T_PTR, T_UINT, T_ULONG, T_CONST, T_FLOAT,
               T_SHORT, T_USHORT, T_SBYTE, T_UBYTE, T_EXT, T_AGG1, T_AGG16 = T_AGG1 + 63,
               T_MEM1, T_MEM_MAX = T_MEM1 + 999991,
               T_MEM_ALIGN16, T_MEM_ALIGN16_MAX = T_MEM_ALIGN16 + 999976,
               T_AGG16_ALIGN16, T_AGG16_ALIGN16_ONE_INT, T_AGG16_ALIGN16_ONE_SSE,
               T_AGG16_ALIGN16_SI, T_AGG16_ALIGN16_IS, T_AGG16_ALIGN16_SS } Ty;
static _Thread_local int unsigned_ir_types;
static _Thread_local int float_ir_types;
static _Thread_local int short_ir_types;
static _Thread_local int signed_byte_ir_types;
static _Thread_local int aggregate_ir_types;
static _Thread_local int sse_aggregate_ir_types;
static _Thread_local int extended_aggregate_ir_types;
static _Thread_local int function_pointer_ir_types;
static _Thread_local int promoted_format_ir;
static _Thread_local int precise_format_ir;
static _Thread_local int variadic_frame_ir;
static _Thread_local int variadic_aggregate_ir;
static _Thread_local int extended_real_ir;
static _Thread_local int aligned_globals_ir;
static _Thread_local int dynamic_stack_ir;
static _Thread_local int stackalloc_live_ir;
static _Thread_local int aligned_strings_ir;
static _Thread_local int version_36_ir;
static _Thread_local int pointer_integer_cast_ir;
static _Thread_local int flexible_aggregate_ir;
static _Thread_local int aligned_sse_aggregate_ir;
static _Thread_local int small_memory_aggregate_ir;
static _Thread_local int extended_fixed_precision_ir;
static _Thread_local int large_runtime_call_ir;
static int parse_u(const char *, unsigned *);
typedef struct {
    unsigned slot;
    Ty type;
} Param;
typedef struct {
    char *name;
    Ty type;
    char *value;
    int internal;
    int external;
    int declared_extern;
    int thread_local;
    size_t initial_bytes;
    int has_relocation;
    char relocation_target[128];
    long long relocation_addend;
} Global;
typedef struct {
    char *hex;
} GlobalBytes;
typedef struct {
    char *name;
    char *hex;
    unsigned alignment;
} StringLiteral;
typedef struct {
    char *name;
    size_t size, align;
    int internal;
    char *hex;
    size_t hex_bytes;
} GlobalBlock;
typedef struct {
    size_t block, offset;
    char target[128];
    long long addend;
} BlockRelocation;
typedef struct {
    char *name, *block;
    size_t offset;
    Ty type;
} GlobalAlias;
typedef struct {
    char *name;
    size_t size, align, frame_offset;
    unsigned owner;
} LocalBlock;
typedef struct {
    char *name, *block;
    size_t offset;
    Ty type;
    unsigned owner;
} LocalAlias;
typedef struct {
    char *name;
    size_t begin, end, param_count, local_block_bytes;
    size_t local_block_start, local_block_count, local_alias_start, local_alias_count;
    unsigned local_count, max_call_args;
    Ty ret;
    int internal;
    int external;
    int declared_extern;
    int variadic;
    int abi_variadic;
    int *stack_depths;
    Param params[130];
    /* Local slots contain only scalar IR types, all of which fit in a byte. */
    unsigned char local_types[CRUNCH_LOCAL_LIMIT];
} Function;

/**
 * @brief Grows the function table when an additional entry is needed.
 * @param functions Existing table, updated on success.
 * @param count Number of entries currently in use.
 * @param capacity Allocated entry capacity, updated on success.
 * @return Nonzero on success, or zero on overflow or allocation failure.
 */
static int reserve_function(Function **functions, size_t count, size_t *capacity) {
    if (count < *capacity)
        return 1;
    size_t next = *capacity ? *capacity * 2 : 16;
    if (next < *capacity || next > SIZE_MAX / sizeof(**functions))
        return 0;
    Function *grown = realloc(*functions, next * sizeof(**functions));
    if (!grown)
        return 0;
    *functions = grown;
    *capacity = next;
    return 1;
}

/**
 * @brief Converts an IR type spelling to a backend type enum.
 * @param s IR type spelling to convert to a backend type.
 * @return Enum value matching the input, or the documented invalid/fallback value.
 */
static Ty ty(const char *s) {
    if (!strcmp(s, "byte"))
        return T_BYTE;
    if (signed_byte_ir_types && !strcmp(s, "sbyte"))
        return T_SBYTE;
    if (signed_byte_ir_types && !strcmp(s, "ubyte"))
        return T_UBYTE;
    if (short_ir_types && !strcmp(s, "short"))
        return T_SHORT;
    if (short_ir_types && !strcmp(s, "ushort"))
        return T_USHORT;
    if (!strcmp(s, "int"))
        return T_INT;
    if (!strcmp(s, "long"))
        return T_LONG;
    if (unsigned_ir_types && !strcmp(s, "uint"))
        return T_UINT;
    if (unsigned_ir_types && !strcmp(s, "ulong"))
        return T_ULONG;
    if (!strcmp(s, "real"))
        return T_REAL;
    if (float_ir_types && !strcmp(s, "float"))
        return T_FLOAT;
    if (!strcmp(s, "void"))
        return T_VOID;
    if (!strcmp(s, "ptr"))
        return T_PTR;
    return T_BAD;
}

/**
 * @brief Parses a scalar or supported aggregate parameter type.
 * @param s Parameter type spelling.
 * @return Parameter type, or T_BAD if the spelling is invalid.
 */
static Ty parameter_ty(const char *s) {
    Ty scalar = ty(s);
    if (scalar == T_BAD && extended_real_ir && !strcmp(s, "ext"))
        return T_EXT;
    if (scalar == T_BAD && extended_real_ir && !strcmp(s, "agg16a"))
        return T_AGG16_ALIGN16;
    if (scalar == T_BAD && flexible_aggregate_ir && !strcmp(s, "agg16a1i"))
        return T_AGG16_ALIGN16_ONE_INT;
    if (scalar == T_BAD && flexible_aggregate_ir && !strcmp(s, "agg16a1s"))
        return T_AGG16_ALIGN16_ONE_SSE;
    if (scalar == T_BAD && aligned_sse_aggregate_ir) {
        if (!strcmp(s, "agg16asi"))
            return T_AGG16_ALIGN16_SI;
        if (!strcmp(s, "agg16ais"))
            return T_AGG16_ALIGN16_IS;
        if (!strcmp(s, "agg16ass"))
            return T_AGG16_ALIGN16_SS;
    }
    if (scalar == T_BAD && extended_aggregate_ir_types && !strncmp(s, "mem", 3)) {
        char *end;
        unsigned long size = strtoul(s + 3, &end, 10);
        if (extended_real_ir && !strcmp(end, "a") && s[3] >= '1' && s[3] <= '9' &&
            size >= 16 && size <= 999992 && size % 16 == 0)
            return (Ty)(T_MEM_ALIGN16 + size - 16);
        if (s[3] < '1' || s[3] > '9' || *end ||
            size < (small_memory_aggregate_ir ? 1UL : 17UL) || size > 999992)
            return T_BAD;
        return (Ty)(T_MEM1 + size - 1);
    }
    if (scalar != T_BAD || !aggregate_ir_types || strncmp(s, "agg", 3))
        return scalar;
    char *end;
    unsigned long size = strtoul(s + 3, &end, 10);
    if (s[3] < '1' || s[3] > '9' || end == s + 3 || size < 1 || size > 16)
        return T_BAD;
    unsigned mask = 0;
    if (*end) {
        unsigned words = size > 8 ? 2 : 1;
        if (!sse_aggregate_ir_types || strlen(end) != words)
            return T_BAD;
        for (unsigned i = 0; i < words; i++) {
            if (end[i] == 's')
                mask |= 1U << i;
            else if (end[i] != 'i')
                return T_BAD;
        }
        if (!mask)
            return T_BAD;
    }
    return (Ty)(T_AGG1 + mask * 16 + size - 1);
}

/**
 * @brief Parses an IR function return type.
 * @param s Return type spelling.
 * @return Scalar or aggregate type, or T_BAD.
 */
static Ty return_ty(const char *s) {
    return extended_aggregate_ir_types ? parameter_ty(s) : ty(s);
}

/**
 * @brief Determines whether a parameter is an aggregate.
 * @param type Parameter type.
 * @return Nonzero for an aggregate parameter.
 */
static int aggregate_param(Ty type) {
    return type >= T_AGG1 && type <= T_AGG16_ALIGN16_SS;
}

/**
 * @brief Identifies a signature type represented by an address in IR.
 * @param type Parameter or return type.
 * @return Nonzero for aggregates and binary80 values.
 */
static int address_parameter(Ty type) {
    return type == T_EXT || aggregate_param(type);
}

/**
 * @brief Identifies a memory-class aggregate.
 * @param type Parameter or return type.
 * @return Nonzero for a memory-class aggregate.
 */
static int memory_aggregate(Ty type) {
    return type >= T_MEM1 && type <= T_MEM_ALIGN16_MAX;
}

/**
 * @brief Returns the exact size of an aggregate.
 * @param type Aggregate type.
 * @return Size in bytes.
 */
static unsigned aggregate_size(Ty type) {
    return type >= T_AGG16_ALIGN16 && type <= T_AGG16_ALIGN16_SS ? 16
           : type >= T_MEM_ALIGN16 && type <= T_MEM_ALIGN16_MAX
               ? (unsigned)(type - T_MEM_ALIGN16 + 16)
           : memory_aggregate(type) ? (unsigned)(type - T_MEM1 + 1)
                                 : (unsigned)((type - T_AGG1) % 16 + 1);
}

/**
 * @brief Writes a stable IR-level ABI type spelling into an assembly string.
 * @param out Assembly output stream.
 * @param type Validated parameter or return type.
 * @return No value.
 */
static void emit_abi_type(FILE *out, Ty type) {
    static const char *const scalar[] = {
        "bad", "byte", "int", "long", "real", "void", "ptr", "uint", "ulong",
        "const", "float", "short", "ushort", "sbyte", "ubyte", "ext"
    };
    if (type <= T_EXT) {
        fputs(scalar[type], out);
    } else if (type == T_AGG16_ALIGN16) {
        fputs("agg16a", out);
    } else if (type == T_AGG16_ALIGN16_ONE_INT || type == T_AGG16_ALIGN16_ONE_SSE) {
        fputs(type == T_AGG16_ALIGN16_ONE_INT ? "agg16a1i" : "agg16a1s", out);
    } else if (type >= T_AGG16_ALIGN16_SI && type <= T_AGG16_ALIGN16_SS) {
        static const char *const names[] = {"agg16asi", "agg16ais", "agg16ass"};
        fputs(names[type - T_AGG16_ALIGN16_SI], out);
    } else if (type >= T_MEM_ALIGN16 && type <= T_MEM_ALIGN16_MAX) {
        fprintf(out, "mem%ua", aggregate_size(type));
    } else if (memory_aggregate(type)) {
        fprintf(out, "mem%u", aggregate_size(type));
    } else {
        unsigned mask = (unsigned)(type - T_AGG1) / 16;
        unsigned size = aggregate_size(type);
        fprintf(out, "agg%u", size);
        if (mask) {
            fputc(mask & 1 ? 's' : 'i', out);
            if (size > 8)
                fputc(mask & 2 ? 's' : 'i', out);
        }
    }
}

/**
 * @brief Counts the eightbyte ABI slots used by a parameter.
 * @param type Parameter type.
 * @return One or two eightbyte slots.
 */
static unsigned parameter_words(Ty type) {
    if (type == T_AGG16_ALIGN16_ONE_INT || type == T_AGG16_ALIGN16_ONE_SSE)
        return 1;
    return (type >= T_AGG16_ALIGN16 && type <= T_AGG16_ALIGN16_SS) ||
           (aggregate_param(type) && !memory_aggregate(type) && (type - T_AGG1) % 16 >= 8) ? 2 : 1;
}

/**
 * @brief Identifies aggregate types with 16-byte stack alignment.
 * @param type Parameter or return type.
 * @return Nonzero if the aggregate requires 16-byte alignment.
 */
static int aligned_aggregate(Ty type) {
    return (type >= T_AGG16_ALIGN16 && type <= T_AGG16_ALIGN16_SS) ||
           (type >= T_MEM_ALIGN16 && type <= T_MEM_ALIGN16_MAX);
}

/**
 * @brief Counts stack eightbytes occupied by an ABI parameter.
 * @param type Parameter type.
 * @return Number of eightbytes.
 */
static unsigned parameter_stack_words(Ty type) {
    return type == T_EXT || type == T_AGG16_ALIGN16_ONE_INT ||
           type == T_AGG16_ALIGN16_ONE_SSE ? 2 :
           memory_aggregate(type) ? (aggregate_size(type) + 7) / 8 : parameter_words(type);
}

/**
 * @brief Aligns an outgoing or incoming stack-argument offset.
 * @param type Parameter type.
 * @param words Current offset in eightbytes.
 * @return Aligned offset in eightbytes.
 */
static unsigned parameter_stack_offset(Ty type, unsigned words) {
    return type == T_EXT || aligned_aggregate(type)
               ? (words + 1) & ~1U : words;
}

/**
 * @brief Identifies an SSE-class aggregate eightbyte.
 * @param type Parameter type.
 * @param word Eightbyte index.
 * @return Nonzero for SSE class.
 */
static int parameter_sse_word(Ty type, unsigned word) {
    if (type == T_AGG16_ALIGN16_ONE_INT || type == T_AGG16_ALIGN16_ONE_SSE)
        return type == T_AGG16_ALIGN16_ONE_SSE && word == 0;
    if (type >= T_AGG16_ALIGN16_SI && type <= T_AGG16_ALIGN16_SS)
        return ((unsigned)(type - T_AGG16_ALIGN16_SI + 1) & (1U << word)) != 0;
    return type != T_AGG16_ALIGN16 && aggregate_param(type) && !memory_aggregate(type) &&
           (((type - T_AGG1) / 16) & (1U << word));
}

/**
 * @brief Allocates all registers for a parameter, or leaves both counts unchanged.
 * @param type Parameter type.
 * @param gp Number of integer registers already allocated.
 * @param fp Number of floating-point registers already allocated.
 * @return Nonzero if the entire parameter fits in registers.
 */
static int parameter_registers(Ty type, unsigned *gp, unsigned *fp) {
    unsigned g = 0, f = 0;
    if (memory_aggregate(type) || type == T_EXT)
        return 0;
    if (aggregate_param(type)) {
        for (unsigned i = 0; i < parameter_words(type); i++)
            if (parameter_sse_word(type, i))
                f++;
            else
                g++;
    } else if (type == T_REAL || type == T_FLOAT)
        f = 1;
    else
        g = 1;
    if (*gp + g > 6 || *fp + f > 8)
        return 0;
    *gp += g;
    *fp += f;
    return 1;
}

/**
 * @brief Parses an indirect-call signature from one IR instruction.
 * @param line Complete instruction.
 * @param signature Receives the parsed return and parameter types.
 * @return Nonzero for a valid signature.
 */
static int indirect_signature(const char *line, Function *signature) {
    char op[32], count_text[32], result[32], word[32];
    int offset = 0, used;
    unsigned count;
    if (scan_ir(line, "%31s %31s %31s %n", op, count_text, result, &offset) != 3 ||
        strcmp(op, "callptr") || !parse_u(count_text, &count) ||
        count > (large_runtime_call_ir ? 130U : 128U))
        return 0;
    memset(signature, 0, sizeof(*signature));
    signature->param_count = count;
    signature->ret = return_ty(result);
    if (signature->ret == T_BAD)
        return 0;
    for (unsigned i = 0; i < count; ++i) {
        used = 0;
        if (scan_ir(line + offset, " %31s%n", word, &used) != 1 || !used)
            return 0;
        offset += used;
        Ty type = parameter_ty(word);
        if (type == T_BAD || type == T_VOID)
            return 0;
        signature->params[i].type = type;
    }
    return scan_ir(line + offset, " %31s", word) != 1;
}

/**
 * @brief Returns the byte size of a stored IR scalar.
 * @param type Scalar type to inspect.
 * @return Storage size, or zero for a non-storable type.
 */
static size_t scalar_size(Ty type) {
    if (type == T_BYTE || type == T_SBYTE || type == T_UBYTE)
        return 1;
    if (type == T_SHORT || type == T_USHORT)
        return 2;
    if (type == T_INT || type == T_UINT || type == T_FLOAT)
        return 4;
    if (type == T_LONG || type == T_ULONG || type == T_REAL || type == T_PTR)
        return 8;
    return 0;
}
/**
 * @brief Validates an explicitly signed or unsigned byte global initializer.
 * @param text Initializer text.
 * @param type Global scalar type.
 * @return Nonzero when the initializer is representable.
 */
static int valid_character_initializer(const char *text, Ty type) {
    char *end;
    if (type == T_UBYTE) {
        if (*text == '-')
            return 0;
        errno = 0;
        unsigned long long value = strtoull(text, &end, 0);
        return !errno && end != text && !*end && value <= UINT8_MAX;
    }
    if (type != T_SBYTE)
        return 1;
    errno = 0;
    long long value = strtoll(text, &end, 0);
    return !errno && end != text && !*end && value >= INT8_MIN && value <= INT8_MAX;
}
/**
 * @brief Orders block relocations by block and byte offset.
 * @param a First relocation.
 * @param b Second relocation.
 * @return Negative, zero, or positive according to their order.
 */
static int compare_block_relocations(const BlockRelocation *a, const BlockRelocation *b) {
    if (a->block != b->block)
        return a->block < b->block ? -1 : 1;
    if (a->offset != b->offset)
        return a->offset < b->offset ? -1 : 1;
    return 0;
}

/**
 * @brief Emits one byte safely inside an assembler quoted string.
 * @param out Assembly output stream.
 * @param byte Byte to emit.
 * @return No value.
 */
static void emit_quoted_byte(FILE *out, unsigned char byte) {
    if (byte == '"' || byte == '\\') {
        fputc('\\', out);
        fputc(byte, out);
    } else if (byte >= 32 && byte <= 126) {
        fputc(byte, out);
    } else {
        fputc('\\', out);
        fputc('0' + (byte >> 6), out);
        fputc('0' + ((byte >> 3) & 7), out);
        fputc('0' + (byte & 7), out);
    }
}

/**
 * @brief Emits a range of hexadecimal bytes without altering its length.
 * @param out Assembly output stream.
 * @param hex Validated hexadecimal byte pairs.
 * @param begin First byte offset.
 * @param end Exclusive ending byte offset.
 * @return No value.
 */
static void emit_hex_string(FILE *out, const char *hex, size_t begin, size_t end) {
    int terminated = end > begin && hex[(end - 1) * 2] == '0' && hex[(end - 1) * 2 + 1] == '0';
    if (terminated) --end;
    fputs(terminated ? "  .string \"" : "  .ascii \"", out);
    for (size_t i = begin; i < end; ++i) {
        unsigned char high = (unsigned char)hex[i * 2], low = (unsigned char)hex[i * 2 + 1];
        unsigned value = (high <= '9' ? high - '0' : (high | 32) - 'a' + 10) * 16;
        value += low <= '9' ? low - '0' : (low | 32) - 'a' + 10;
        emit_quoted_byte(out, (unsigned char)value);
    }
    fputs("\"\n", out);
}

/**
 * @brief Emits a range of initialized block bytes and implicit zero padding.
 * @param out Assembly output stream.
 * @param hex Hexadecimal initialized block prefix, or NULL.
 * @param initialized Length of the initialized prefix in bytes.
 * @param begin First byte offset.
 * @param end Exclusive ending byte offset.
 * @return No value.
 */
static void emit_block_bytes(FILE *out, const char *hex, size_t initialized,
                             size_t begin, size_t end) {
    size_t limit = end < initialized ? end : initialized;
    if (begin < limit)
        emit_hex_string(out, hex, begin, limit);
    if (limit < begin) limit = begin;
    if (limit < end)
        fprintf(out, "  .zero %zu\n", end - limit);
}
/**
 * @brief Checks whether text is a plain assembler-safe symbol identifier.
 * @param text Symbol spelling to validate.
 * @return One when text is a nonempty ASCII identifier; otherwise zero.
 */
static int valid_asm_symbol(const char *text) {
    if (!text ||
        !((*text >= 'A' && *text <= 'Z') || (*text >= 'a' && *text <= 'z') || *text == '_'))
        return 0;
    for (const char *p = text + 1; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
              *p == '_'))
            return 0;
    return 1;
}
/**
 * @brief Validates an external global symbol, including array-length symbols.
 * @param text Symbol spelling to validate.
 * @param type Declared scalar type.
 * @return Nonzero if the symbol and type are valid.
 */
static int valid_external_global_symbol(const char *text, Ty type) {
    if (valid_asm_symbol(text))
        return 1;
    const char *suffix = text ? strstr(text, "$length") : NULL;
    if (!suffix || strcmp(suffix, "$length") || type != T_INT)
        return 0;
    size_t length = (size_t)(suffix - text);
    char base[128];
    if (!length || length >= sizeof base)
        return 0;
    memcpy(base, text, length);
    base[length] = '\0';
    return valid_asm_symbol(base);
}
/**
 * @brief Identifies linker symbols reserved for runtime initialization and finalization.
 * @param name Assembly symbol name to inspect.
 * @return Nonzero if the name is reserved for the runtime.
 */
static int is_array_registry_symbol(const char *name) {
    return !strcmp(name, "__start___crunch_static_arrays") ||
           !strcmp(name, "__stop___crunch_static_arrays") ||
           !strcmp(name, "__start___crunch_init_array") ||
           !strcmp(name, "__stop___crunch_init_array") ||
           !strcmp(name, "__crunch_run_finalizers");
}
/**
 * @brief Identifies names reserved for the module runtime and entry point.
 * @param name Assembly symbol name to inspect.
 * @return Nonzero if the name is reserved in module IR.
 */
static int is_module_reserved_symbol(const char *name) {
    return !strncmp(name, "__crunch_", 9) || !strcmp(name, "_start");
}
/**
 * @brief Identifies a special runtime fallback marker allowed in module IR.
 * @param name Marker symbol name.
 * @return Nonzero for a recognized fallback marker.
 */
static int is_runtime_fallback_marker(const char *name) {
    return !strcmp(name, "__crunch_fallback_printf") ||
           !strcmp(name, "__crunch_fallback_fprintf") ||
           !strcmp(name, "__crunch_fallback_scanf") ||
           !strcmp(name, "__crunch_fallback_fscanf") ||
           !strcmp(name, "__crunch_fallback_sprintf") ||
           !strcmp(name, "__crunch_fallback_snprintf") ||
           !strcmp(name, "__crunch_fallback_vfork");
}
/**
 * @brief Identifies private symbols used by file syscall wrappers.
 * @param name Assembly symbol name to inspect.
 * @return Nonzero for a reserved wrapper or raw syscall symbol.
 */
static int is_file_syscall_symbol(const char *name) {
    static const char *const names[] = {
        "_crunch_open", "_crunch_read", "_crunch_write", "_crunch_close", "_crunch_stat",
        "_crunch_lseek", "__crunch_open", "__crunch_read", "__crunch_write", "__crunch_close",
        "__crunch_stat", "__crunch_lseek",
        "_crunch_fstat", "_crunch_unlink", "_crunch_rename", "_crunch_mkdir", "_crunch_rmdir",
        "__crunch_fstat", "__crunch_unlink", "__crunch_rename", "__crunch_mkdir", "__crunch_rmdir",
        "_crunch_openat", "_crunch_lstat", "_crunch_readlink", "_crunch_getdents64",
        "__crunch_openat", "__crunch_lstat", "__crunch_readlink", "__crunch_getdents64",
        "__crunch_dirent64_next",
        "_crunch_fstatat", "_crunch_readlinkat", "_crunch_unlinkat",
        "_crunch_mkdirat", "_crunch_renameat", "_crunch_symlinkat",
        "__crunch_fstatat", "__crunch_readlinkat", "__crunch_unlinkat",
        "__crunch_mkdirat", "__crunch_renameat", "__crunch_symlinkat",
        "_crunch_access", "_crunch_dup", "_crunch_dup2", "_crunch_pipe",
        "_crunch_poll", "_crunch_select", "_crunch_fork", "_crunch_getpid", "_crunch_kill",
        "__crunch_access", "__crunch_dup", "__crunch_dup2", "__crunch_pipe",
        "__crunch_poll", "__crunch_select", "__crunch_fork", "__crunch_getpid", "__crunch_kill"
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (!strcmp(name, names[i]))
            return 1;
    return 0;
}
/**
 * @brief Parses an unsigned decimal integer.
 * @param s Unsigned decimal text to parse.
 * @param v Output receiving the parsed unsigned integer.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int parse_u(const char *s, unsigned *v) {
    char *end;
    errno = 0;
    unsigned long x = strtoul(s, &end, 10);
    if (errno || end == s || *end || x > 1000000UL)
        return 0;
    *v = (unsigned)x;
    return 1;
}

/**
 * @brief Parses a bounded local block size.
 * @param s Unsigned decimal text to parse.
 * @param v Output receiving the parsed size.
 * @return Nonzero if the size is valid.
 */
static int parse_local_block_size(const char *s, unsigned *v) {
    char *end;
    errno = 0;
    unsigned long x = strtoul(s, &end, 10);
    if (errno || end == s || *end || x > 16777216UL)
        return 0;
    *v = (unsigned)x;
    return 1;
}

/**
 * @brief Parses a bounded global block size or structure-array stride.
 * @param s Unsigned decimal text to parse.
 * @param v Output receiving the parsed size.
 * @return Nonzero if the size is valid.
 */
static int parse_storage_size(const char *s, unsigned *v) {
    char *end;
    errno = 0;
    unsigned long x = strtoul(s, &end, 10);
    if (errno || end == s || *end || x > 67108864UL)
        return 0;
    *v = (unsigned)x;
    return 1;
}

/**
 * @brief Parses an unsigned IR integer literal with an explicit range.
 * @param s Decimal literal text.
 * @param maximum Largest permitted value.
 * @param value Receives the parsed value.
 * @return Nonzero for a valid literal in range.
 */
static int parse_uint_literal(const char *s, uint64_t maximum, uint64_t *value) {
    char *end;
    if (!s || !*s)
        return 0;
    for (const char *p = s; *p; ++p)
        if (*p < '0' || *p > '9')
            return 0;
    errno = 0;
    unsigned long long parsed = strtoull(s, &end, 10);
    if (errno == ERANGE || *end || parsed > maximum)
        return 0;
    *value = (uint64_t)parsed;
    return 1;
}

/**
 * @brief Checks an exact-width binary32 hexadecimal bit pattern.
 * @param text Literal text to inspect.
 * @return Nonzero for 0x followed by exactly eight hexadecimal digits.
 */
static int valid_float_bits(const char *text) {
    if (strlen(text) != 10 || text[0] != '0' || text[1] != 'x')
        return 0;
    for (size_t i = 2; i < 10; i++)
        if (!((text[i] >= '0' && text[i] <= '9') ||
              (text[i] >= 'a' && text[i] <= 'f') ||
              (text[i] >= 'A' && text[i] <= 'F')))
            return 0;
    return 1;
}

/**
 * @brief Identifies a type spelling in the type operand positions of an IR line.
 * @param line IR line to inspect.
 * @param spelling Type name to find.
 * @return Nonzero if the type is used in a recognized operand position.
 */
static int has_type_operand(const char *line, const char *spelling) {
    char op[32] = {0}, a[128] = {0}, b[128] = {0}, c[128] = {0}, d[128] = {0};
    if (scan_ir(line, "%31s %127s %127s %127s %127s", op, a, b, c, d) < 1)
        return 0;
    const char *type = NULL;
    if (!strcmp(op, "func") || !strcmp(op, "param"))
        type = c;
    else if (!strcmp(op, "global") || !strcmp(op, "extern_global") ||
             !strcmp(op, "local") || !strcmp(op, "store") ||
             !strcmp(op, "gload") || !strcmp(op, "gstore") ||
             !strcmp(op, "lload") || !strcmp(op, "lstore"))
        type = b;
    else if (!strcmp(op, "globalalias") || !strcmp(op, "localalias"))
        type = d;
    else if (!strcmp(op, "cast"))
        return !strcmp(a, spelling) || !strcmp(b, spelling);
    else if (!strcmp(op, "neg") || !strcmp(op, "not") || !strcmp(op, "bitnot") ||
             !strcmp(op, "bool") || !strcmp(op, "add") || !strcmp(op, "sub") ||
             !strcmp(op, "mul") || !strcmp(op, "div") || !strcmp(op, "rem") ||
             !strcmp(op, "band") || !strcmp(op, "bor") || !strcmp(op, "bxor") ||
             !strcmp(op, "shl") || !strcmp(op, "shr") || !strcmp(op, "eq") ||
             !strcmp(op, "ne") || !strcmp(op, "lt") || !strcmp(op, "le") ||
             !strcmp(op, "gt") || !strcmp(op, "ge") || !strcmp(op, "ret") ||
             !strcmp(op, "loadind") || !strcmp(op, "storeind") ||
             !strcmp(op, "ptradd") || !strcmp(op, "ptrsub") ||
             !strcmp(op, "array_alloc") || !strcmp(op, "array_resize"))
        type = a;
    return type && !strcmp(type, spelling);
}

/**
 * @brief Identifies unsigned type names in legacy IR type operand positions.
 * @param line IR line to inspect.
 * @return Nonzero if the line uses an unsigned type or instruction.
 */
static int legacy_unsigned_syntax(const char *line) {
    char op[32] = {0};
    if (scan_ir(line, "%31s", op) < 1)
        return 0;
    return !strcmp(op, "pushu") || has_type_operand(line, "uint") ||
           has_type_operand(line, "ulong");
}
/* Library routines linked from libcrunch.a. Arguments are popped into the System V argument
 * registers; result is 'i' int, 'l' long, 'U' unsigned long, 'p' pointer,
 * 'c' discarded constant, 'n' none. */
typedef struct {
    const char *name;
    unsigned argc;
    char result;
} RuntimeRoutine;

static const RuntimeRoutine runtime_routines[] = {
#define RUNTIME_RECORD(name, argc, result, arguments) {name, argc, result},
    CRUNCH_RUNTIME_ROUTINES(RUNTIME_RECORD)
#undef RUNTIME_RECORD
};

/**
 * @brief Looks up a runtime routine by its Crunch symbol name.
 * @param name Function name to search for.
 * @return Matching routine metadata, or NULL if the name is not a runtime routine.
 */
static const RuntimeRoutine *find_runtime_routine(const char *name) {
    static const RuntimeRoutine legacy_end = {"end", 1, 'n'};
    static const RuntimeRoutine legacy_print = {"print", 1, 'i'};
    if (!strcmp(name, "end"))
        return &legacy_end;
    if (!strcmp(name, "print"))
        return &legacy_print;
    for (size_t i = 0; i < sizeof(runtime_routines) / sizeof(runtime_routines[0]); i++)
        if (!strcmp(runtime_routines[i].name, name))
            return (((runtime_routines[i].result == 'U' || runtime_routines[i].result == 'u') && !unsigned_ir_types) ||
                    (runtime_routines[i].result == 'f' && !float_ir_types) ||
                    (runtime_routines[i].result == 'e' && !extended_real_ir))
                       ? NULL : &runtime_routines[i];
    return NULL;
}

/**
 * @brief Emits the stack-to-register setup and call for a runtime routine.
 * @param out Assembly stream receiving the call sequence.
 * @param r Runtime routine metadata describing arguments and result handling.
 * @return No value; writes assembly to out.
 */
static void emit_runtime_call(FILE *out, const RuntimeRoutine *r) {
    static const char *const regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    if (!strcmp(r->name, "vfork")) {
        fputs("  mov $58, %eax\n  syscall\n  cmp $-4095, %rax\n"
              "  jb 1f\n  neg %eax\n  push %rax\n"
              "  mov %rsp, %r11\n  and $-16, %rsp\n  sub $16, %rsp\n"
              "  mov %r11, (%rsp)\n  call __crunch___errno_location\n"
              "  mov (%rsp), %rsp\n  pop %rdx\n  mov %edx, (%rax)\n"
              "  mov $-1, %rax\n1:\n  push %rax\n", out);
        return;
    }
    for (unsigned i = r->argc; i > 0; i--)
        fprintf(out, "  pop %s\n", regs[i - 1]);
    fprintf(out, "  mov %%rsp, %%r11\n  and $-16, %%rsp\n"
                 "  sub $16, %%rsp\n  mov %%r11, (%%rsp)\n"
                 "  call __crunch_%s\n  mov (%%rsp), %%rsp\n", r->name);
    if (r->result == 'e') {
        fputs("  mov (%rsp), %rax\n  fstpt (%rax)\n", out);
        return;
    }
    if (r->result == 'r' || r->result == 'f') {
        fputs("  movq %xmm0, %rax\n", out);
        if (r->result == 'f')
            fputs("  mov %eax, %eax\n", out);
    }
    if (r->result != 'n')
        fputs("  push %rax\n", out);
}

/**
 * @brief Finds a function record by name.
 * @param f Function table to search.
 * @param n Number of entries in f.
 * @param name Function name to match.
 * @return Matching function record, or NULL when no entry matches.
 */
static Function *findfn(Function *f, size_t n, const char *name) {
    for (size_t i = 0; i < n; i++)
        if (!strcmp(f[i].name, name))
            return &f[i];
    return NULL;
}
static _Thread_local Diagnostic *active_backend_diagnostic;

/**
 * @brief Formats a backend diagnostic with source-line and function context.
 * @param buffer Writable character buffer receiving output.
 * @param size Capacity or size in bytes.
 * @param line One-based IR line number for diagnostic context.
 * @param function Function name for diagnostic context.
 * @param format printf-style format string.
 * @return No value; the function performs the documented output or state update.
 */
static void backend_error(char *buffer, size_t size, size_t line, const char *function,
                          const char *format, ...) {
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (active_backend_diagnostic) {
        active_backend_diagnostic->ir_line = line;
        if (function) {
            snprintf(active_backend_diagnostic->function_storage,
                     sizeof(active_backend_diagnostic->function_storage), "%s", function);
            active_backend_diagnostic->function = active_backend_diagnostic->function_storage;
        }
        snprintf(buffer, size, "%s", message);
    } else if (line && function) {
        snprintf(buffer, size, "IR line %zu (function '%s'): %s", line, function, message);
    } else if (line) {
        snprintf(buffer, size, "IR line %zu: %s", line, message);
    } else {
        snprintf(buffer, size, "%s", message);
    }
}

/**
 * @brief Parses a branch-label identifier.
 * @param text Text spelling or source text being parsed.
 * @param id Output parameter receiving the id value.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int parse_label_id(const char *text, unsigned *id) {
    if (!text || text[0] != 'L' || !text[1])
        return 0;
    for (const char *p = text + 1; *p; ++p)
        if (*p < '0' || *p > '9')
            return 0;
    return parse_u(text + 1, id);
}
/**
 * @brief Checks label definitions, uniqueness, and branch targets.
 * @param lines IR instruction lines to validate or translate.
 * @param functions Function metadata being inspected.
 * @param function_count Number of elements or records.
 * @param error Writable character buffer receiving output.
 * @param error_size Capacity or size in bytes.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int validate_labels(char **lines, const Function *functions, size_t function_count,
                           char *error, size_t error_size) {
    unsigned *defined = calloc(32768, sizeof(*defined));
    unsigned *targets = calloc(32768, sizeof(*targets));
    size_t *defined_owner = calloc(32768, sizeof(*defined_owner));
    size_t *target_owner = calloc(32768, sizeof(*target_owner));
    size_t *defined_lines = calloc(32768, sizeof(*defined_lines));
    size_t *target_lines = calloc(32768, sizeof(*target_lines));
    size_t defined_count = 0, target_count = 0;
    if (!defined || !targets || !defined_owner || !target_owner || !defined_lines ||
        !target_lines) {
        free(defined);
        free(targets);
        free(defined_owner);
        free(target_owner);
        free(defined_lines);
        free(target_lines);
        backend_error(error, error_size, 0, NULL, "out of memory validating branch labels");
        return 0;
    }
    for (size_t f = 0; f < function_count; ++f) {
        const Function *fn = &functions[f];
        for (size_t i = fn->begin; i < fn->end; ++i) {
            char op[32] = {0}, operand[128] = {0};
            if (scan_ir(lines[i], "%31s %127s", op, operand) < 1)
                continue;
            if (!strcmp(op, "label")) {
                unsigned id;
                if (!parse_label_id(operand, &id)) {
                    backend_error(error, error_size, i + 1, fn->name, "invalid label '%s'",
                                  operand);
                    goto invalid;
                }
                for (size_t j = 0; j < defined_count; ++j) {
                    if (defined[j] == id) {
                        backend_error(error, error_size, i + 1, fn->name,
                                      "duplicate label '%s' (first defined on line %zu)", operand,
                                      defined_lines[j]);
                        goto invalid;
                    }
                }
                if (defined_count == 32768) {
                    backend_error(error, error_size, i + 1, fn->name, "too many labels");
                    goto invalid;
                }
                defined[defined_count] = id;
                defined_owner[defined_count] = f;
                defined_lines[defined_count++] = i + 1;
            } else if (!strcmp(op, "jmp") || !strcmp(op, "jz") || !strcmp(op, "jnz")) {
                unsigned id;
                if (!parse_label_id(operand, &id)) {
                    backend_error(error, error_size, i + 1, fn->name, "invalid branch target '%s'",
                                  operand);
                    goto invalid;
                }
                if (target_count == 32768) {
                    backend_error(error, error_size, i + 1, fn->name, "too many branch targets");
                    goto invalid;
                }
                targets[target_count] = id;
                target_owner[target_count] = f;
                target_lines[target_count++] = i + 1;
            }
        }
    }
    for (size_t i = 0; i < target_count; ++i) {
        size_t j;
        for (j = 0; j < defined_count; ++j)
            if (targets[i] == defined[j] && target_owner[i] == defined_owner[j])
                break;
        if (j == defined_count) {
            backend_error(error, error_size, target_lines[i], functions[target_owner[i]].name,
                          "branch target 'L%u' is not defined in this function", targets[i]);
            goto invalid;
        }
    }
    free(defined);
    free(targets);
    free(defined_owner);
    free(target_owner);
    free(defined_lines);
    free(target_lines);
    return 1;
invalid:
    free(defined);
    free(targets);
    free(defined_owner);
    free(target_owner);
    free(defined_lines);
    free(target_lines);
    return 0;
}
/**
 * @brief Classifies the address-based binary80 operations.
 * @param op IR instruction name.
 * @return Copy/negate, arithmetic, comparison, or boolean class, or zero.
 */
static int extended_operation(const char *op) {
    if (!strcmp(op, "extcopy") || !strcmp(op, "extneg"))
        return 1;
    if (!strcmp(op, "extadd") || !strcmp(op, "extsub") ||
        !strcmp(op, "extmul") || !strcmp(op, "extdiv"))
        return 2;
    if (!strcmp(op, "exteq") || !strcmp(op, "extne") || !strcmp(op, "extlt") ||
        !strcmp(op, "extle") || !strcmp(op, "extgt") || !strcmp(op, "extge"))
        return 3;
    return !strcmp(op, "extbool") ? 4 : 0;
}

/* Verify evaluation-stack depth through each function's control-flow graph.
 * Values are 64-bit cells in IR v2; type-flow validation is a follow-up layer. */
/**
 * @brief Describes the operand-stack requirements and effects of an IR instruction.
 * @param op Text input to parse, compare, or serialize.
 * @param a First operand type.
 * @param b Second operand type.
 * @param need Output parameter receiving the need value.
 * @param delta Output parameter receiving the delta value.
 * @param terminal Output parameter receiving the terminal value.
 * @param conditional Output parameter receiving the conditional value.
 * @param module Nonzero for module-mode IR.
 * @param runtime_noreturn Bit flags identifying unshadowed v2 runtime calls.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int ir_stack_effect(const char *op, const char *a, const char *b, int *need, int *delta,
                           int *terminal, int *conditional, int module, int runtime_noreturn) {
    unsigned n;
    *need = 0;
    *delta = 0;
    *terminal = 0;
    *conditional = 0;
    if (dynamic_stack_ir && !strcmp(op, "stackmark")) {
        *delta = 1;
        return 1;
    }
    if (dynamic_stack_ir && !strcmp(op, "stackalloc")) {
        *need = 1;
        return 1;
    }
    if (stackalloc_live_ir && !strcmp(op, "stackalloc_live")) {
        *need = 1;
        return 1;
    }
    if (dynamic_stack_ir && !strcmp(op, "stackrestore")) {
        *need = 1;
        *delta = -1;
        return 1;
    }
    if (!strcmp(op, "func") || !strcmp(op, "end") || !strcmp(op, "local") ||
        !strcmp(op, "localblock") || !strcmp(op, "localalias") || !strcmp(op, "param") ||
        !strcmp(op, "label") || !strcmp(op, "stringlit") ||
        (variadic_frame_ir && !strcmp(op, "variadic")))
        return 1;
    if (variadic_frame_ir && !strcmp(op, "vastart")) {
        *need = 1;
        *delta = -1;
        return 1;
    }
    if (variadic_frame_ir && !strcmp(op, "vaarg")) {
        *need = 1;
        return 1;
    }
    if (variadic_frame_ir && !strcmp(op, "vacopy")) {
        *need = 2;
        *delta = -2;
        return 1;
    }
    if (variadic_aggregate_ir && !strcmp(op, "vaargagg")) {
        *need = 2;
        *delta = -1;
        return 1;
    }
    if (extended_real_ir) {
        if (!strcmp(op, "extfrom") || !strcmp(op, "extto")) {
            *need = !strcmp(op, "extfrom") ? 2 : 1;
            *delta = 1 - *need;
            return 1;
        }
        int kind = extended_operation(op);
        if (kind) {
            *need = kind == 2 ? 3 : kind == 4 ? 1 : 2;
            *delta = 1 - *need;
            return 1;
        }
    }
    if (!strcmp(op, "push") || !strcmp(op, "pushu") || !strcmp(op, "pushr") ||
        (float_ir_types && !strcmp(op, "pushf")) || !strcmp(op, "straddr") ||
        (function_pointer_ir_types && !strcmp(op, "fnaddr")) ||
        (large_runtime_call_ir && !strcmp(op, "weak_fnaddr")) ||
        !strcmp(op, "gload") || !strcmp(op, "gaddr") || !strcmp(op, "load") ||
        !strcmp(op, "lload") || !strcmp(op, "laddr") || !strcmp(op, "addr")) {
        *delta = 1;
        return 1;
    }
    if (!strcmp(op, "drop") || !strcmp(op, "store") || !strcmp(op, "lstore") ||
        !strcmp(op, "gstore")) {
        *need = 1;
        *delta = -1;
        return 1;
    }
    if (!strcmp(op, "dup")) {
        *need = 1;
        *delta = 1;
        return 1;
    }
    if (!strcmp(op, "loadind")) {
        *need = 1;
        return 1;
    }
    if (!strcmp(op, "storeind")) {
        *need = 2;
        *delta = -1;
        return 1;
    }
    if (!strcmp(op, "cast") || !strcmp(op, "neg") || !strcmp(op, "not") || !strcmp(op, "bitnot") ||
        !strcmp(op, "bool")) {
        *need = 1;
        return 1;
    }
    if (!strcmp(op, "pow")) {
        if (strcmp(a, "real") || *b)
            return 0;
        *need = 2;
        *delta = -1;
        return 1;
    }
    if (!strcmp(op, "add") || !strcmp(op, "sub") || !strcmp(op, "mul") || !strcmp(op, "div") ||
        !strcmp(op, "rem") || !strcmp(op, "band") || !strcmp(op, "bor") || !strcmp(op, "bxor") ||
        !strcmp(op, "shl") || !strcmp(op, "shr") || !strcmp(op, "eq") || !strcmp(op, "ne") ||
        !strcmp(op, "lt") || !strcmp(op, "le") || !strcmp(op, "gt") || !strcmp(op, "ge")) {
        *need = 2;
        *delta = -1;
        return 1;
    }
    if (!strcmp(op, "ptradd") || !strcmp(op, "ptrsub") || !strcmp(op, "ptradd_bytes")) {
        *need = 2;
        *delta = -1;
        return 1;
    }
    if (version_36_ir && !strcmp(op, "ptrdiff")) {
        *need = 2;
        *delta = -1;
        return 1;
    }
    if (!strcmp(op, "jmp")) {
        *terminal = 1;
        return 1;
    }
    if (!strcmp(op, "jz") || !strcmp(op, "jnz")) {
        *need = 1;
        *delta = -1;
        *conditional = 1;
        return 1;
    }
    if (!strcmp(op, "ret") || (extended_aggregate_ir_types && !strcmp(op, "retagg"))) {
        *need = 1;
        *delta = -1;
        *terminal = 1;
        return 1;
    }
    if (function_pointer_ir_types && !strcmp(op, "callptr")) {
        Ty result = return_ty(b);
        if (!parse_u(a, &n) || n > (large_runtime_call_ir ? 130U : 128U) || result == T_BAD)
            return 0;
        int aggregate = address_parameter(result);
        *need = (int)n + 1 + aggregate;
        *delta = -(int)n - aggregate;
        return 1;
    }
    if (!strcmp(op, "call") || (extended_aggregate_ir_types && !strcmp(op, "callagg")) ||
        (module && !strcmp(op, "runtime_call"))) {
        if (!parse_u(b, &n))
            return 0;
        if ((!strcmp(a, "end") &&
             (!strcmp(op, "runtime_call") || (!module && (runtime_noreturn & 1)))) ||
            (!strcmp(a, "exit") &&
             (!strcmp(op, "runtime_call") || (!module && (runtime_noreturn & 2)))) ||
            (!strcmp(a, "crunch_exit_group") &&
             (!strcmp(op, "runtime_call") || (!module && (runtime_noreturn & 4))))) {
            if (n != 1)
                return 0;
            *need = 1;
            *delta = -1;
            *terminal = 1;
            return 1;
        }
        *need = (int)n + (!strcmp(op, "callagg") ? 1 : 0);
        *delta = !strcmp(op, "callagg") ? -(int)n : 1 - (int)n;
        return 1;
    }
    if (!strcmp(op, "array_alloc")) {
        *need = 1;
        *delta = 1;
        return 1;
    }
    if (!strcmp(op, "array_resize") || !strcmp(op, "array_resize_bytes")) {
        *need = 3;
        *delta = -1;
        return 1;
    }
    return 0;
}

/**
 * @brief Checks instruction operand grammar in float-capable IR versions.
 * @param line IR instruction to inspect.
 * @return Nonzero if recognized operands have valid syntax and types.
 */
static int valid_float_ir_instruction(const char *line) {
    char op[32] = {0}, a[128] = {0}, b[128] = {0}, c[128] = {0}, extra[2] = {0};
    int fields = scan_ir(line, "%31s %127s %127s %127s %1s", op, a, b, c, extra);
    unsigned number;
    Ty t = ty(a), u = ty(b);
    if (fields < 1)
        return 1;
    if ((dynamic_stack_ir && (!strcmp(op, "stackmark") || !strcmp(op, "stackalloc") ||
                              !strcmp(op, "stackrestore"))) ||
        (stackalloc_live_ir && !strcmp(op, "stackalloc_live")))
        return fields == 1;
    if (variadic_frame_ir && (!strcmp(op, "variadic") || !strcmp(op, "vastart") ||
                              !strcmp(op, "vacopy")))
        return fields == 1;
    if (variadic_frame_ir && !strcmp(op, "vaarg"))
        return fields == 2 && (!strcmp(a, "gp") || !strcmp(a, "fp") ||
                               (extended_real_ir && !strcmp(a, "ext")));
    if (variadic_aggregate_ir && !strcmp(op, "vaargagg"))
        return fields == 2 && aggregate_param(parameter_ty(a));
    if (extended_real_ir && extended_operation(op))
        return fields == 1;
    if (extended_real_ir && (!strcmp(op, "extfrom") || !strcmp(op, "extto")))
        return fields == 2 && (t == T_FLOAT || t == T_REAL || t == T_INT || t == T_UINT ||
                               t == T_LONG || t == T_ULONG);
    if (function_pointer_ir_types && !strcmp(op, "callptr")) {
        Function signature;
        return indirect_signature(line, &signature);
    }
    if (function_pointer_ir_types && !strcmp(op, "fnaddr"))
        return fields == 2 && valid_asm_symbol(a);
    if (large_runtime_call_ir && !strcmp(op, "weak_fnaddr"))
        return fields == 2 && is_runtime_fallback_marker(a);
    if (!strcmp(op, "push")) {
        char *end;
        errno = 0;
        (void)strtoll(a, &end, 10);
        return fields == 2 && !errno && end != a && !*end;
    }
    if (!strcmp(op, "pushr")) {
        char *end;
        errno = 0;
        (void)strtoull(a, &end, 0);
        return fields == 2 && !errno && end != a && !*end;
    }
    if (!strcmp(op, "pushf"))
        return fields == 2 && valid_float_bits(a);
    if (!strcmp(op, "pushu")) {
        uint64_t value;
        return fields == 3 && (u == T_UINT || u == T_ULONG) &&
               parse_uint_literal(a, u == T_UINT ? UINT32_MAX : UINT64_MAX, &value);
    }
    if (!strcmp(op, "dup") || !strcmp(op, "drop"))
        return fields == 1;
    if (version_36_ir && !strcmp(op, "ptrdiff"))
        return fields == 1;
    if (!strcmp(op, "load") || !strcmp(op, "addr"))
        return fields == 2 && parse_u(a, &number) && number < CRUNCH_LOCAL_LIMIT;
    if (!strcmp(op, "store") || !strcmp(op, "local"))
        return fields == 3 && parse_u(a, &number) && number < CRUNCH_LOCAL_LIMIT &&
               u != T_BAD && u != T_VOID;
    if (!strcmp(op, "param")) {
        unsigned slot;
        return fields == 4 && parse_u(a, &number) && number < 128 &&
               parse_u(b, &slot) && parameter_ty(c) != T_BAD &&
               parameter_ty(c) != T_VOID && slot <= CRUNCH_LOCAL_LIMIT - parameter_words(parameter_ty(c));
    }
    if (!strcmp(op, "cast"))
        return fields == 3 &&
               ((pointer_integer_cast_ir && t == T_PTR && u == T_ULONG) ||
                (t != T_BAD && t != T_VOID && t != T_PTR &&
                 u != T_BAD && u != T_VOID && u != T_PTR));
    if (!strcmp(op, "neg") || !strcmp(op, "bitnot"))
        return fields == 2 && t != T_BAD && t != T_VOID && t != T_PTR &&
               (strcmp(op, "bitnot") || (t != T_REAL && t != T_FLOAT));
    if (!strcmp(op, "not") || !strcmp(op, "bool"))
        return fields == 2 && t != T_BAD && t != T_VOID;
    if (!strcmp(op, "add") || !strcmp(op, "sub") || !strcmp(op, "mul") ||
        !strcmp(op, "div") || !strcmp(op, "pow") || !strcmp(op, "rem") ||
        !strcmp(op, "band") || !strcmp(op, "bor") || !strcmp(op, "bxor") ||
        !strcmp(op, "shl") || !strcmp(op, "shr") || !strcmp(op, "eq") ||
        !strcmp(op, "ne") || !strcmp(op, "lt") || !strcmp(op, "le") ||
        !strcmp(op, "gt") || !strcmp(op, "ge")) {
        int comparison = !strcmp(op, "eq") || !strcmp(op, "ne") ||
                         !strcmp(op, "lt") || !strcmp(op, "le") ||
                         !strcmp(op, "gt") || !strcmp(op, "ge");
        int integer_only = !strcmp(op, "rem") || !strcmp(op, "band") ||
                           !strcmp(op, "bor") || !strcmp(op, "bxor") ||
                           !strcmp(op, "shl") || !strcmp(op, "shr");
        return fields == 2 && t != T_BAD && t != T_VOID &&
               (t != T_PTR || comparison) &&
               (!integer_only || (t != T_REAL && t != T_FLOAT)) &&
               (strcmp(op, "pow") || t == T_REAL);
    }
    if (!strcmp(op, "ret"))
        return fields == 2 && t != T_BAD;
    if (extended_aggregate_ir_types && !strcmp(op, "retagg"))
        return fields == 2 && address_parameter(parameter_ty(a));
    if (!strcmp(op, "loadind") || !strcmp(op, "storeind") ||
        !strcmp(op, "ptradd") || !strcmp(op, "ptrsub") ||
        !strcmp(op, "array_alloc") || !strcmp(op, "array_resize"))
        return fields == 2 && t != T_BAD && t != T_VOID;
    if (!strcmp(op, "array_resize_bytes"))
        return fields == 2 && parse_u(a, &number) && number != 0;
    if (!strcmp(op, "ptradd_bytes"))
        return fields == 2 && parse_storage_size(a, &number) && number != 0;
    if (!strcmp(op, "gload") || !strcmp(op, "gstore") ||
        !strcmp(op, "lload") || !strcmp(op, "lstore"))
        return fields == 3 && u != T_BAD && u != T_VOID;
    if (!strcmp(op, "label") || !strcmp(op, "jmp") || !strcmp(op, "jz") ||
        !strcmp(op, "jnz") || !strcmp(op, "straddr") || !strcmp(op, "gaddr") ||
        !strcmp(op, "laddr"))
        return fields == 2;
    if (!strcmp(op, "call") || !strcmp(op, "runtime_call") ||
        (extended_aggregate_ir_types && !strcmp(op, "callagg")))
        return fields == 3 && parse_u(b, &number);
    if (!strcmp(op, "localblock"))
        return fields == 4;
    if (!strcmp(op, "localalias") || !strcmp(op, "stringlit"))
        return 1; /* These directives are validated by their declaration passes. */
    return 1;
}

/**
 * @brief Checks stack depth and control-flow merges for one function.
 * @param lines IR instruction lines to validate or translate.
 * @param fn Function metadata being inspected.
 * @param error Writable character buffer receiving output.
 * @param error_size Capacity or size in bytes.
 * @param module Nonzero for module-mode IR.
 * @param functions Function metadata for resolving legacy runtime calls.
 * @param function_count Number of entries in functions.
 * @param maximum_depth Receives the maximum reachable evaluation-stack depth.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int validate_function_stack(char **lines, Function *fn, char *error,
                                   size_t error_size, int module, const Function *functions,
                                   size_t function_count, size_t *maximum_depth) {
    size_t count = fn->end - fn->begin;
    int runtime_noreturn = (!findfn((Function *)functions, function_count, "end") ? 1 : 0) |
                           (!findfn((Function *)functions, function_count, "exit") ? 2 : 0) |
                           (!findfn((Function *)functions, function_count, "crunch_exit_group") ? 4 : 0);
    if (!count) {
        snprintf(error, error_size, "IR function '%s' is empty", fn->name);
        return 0;
    }
    int *depth = malloc(count * sizeof(*depth));
    size_t *queue = malloc(count * sizeof(*queue));
    if (!depth || !queue) {
        free(depth);
        free(queue);
        snprintf(error, error_size, "out of memory verifying stack in function '%s'", fn->name);
        return 0;
    }
    /* Validate instruction names even in unreachable blocks. */
    for (size_t i = 0; i < count; i++) {
        char op[32] = {0}, a[128] = {0}, b[128] = {0};
        int need, delta, terminal, conditional;
        if (scan_ir(lines[fn->begin + i], "%31s %127s %127s", op, a, b) < 1)
            continue;
        if (float_ir_types && !valid_float_ir_instruction(lines[fn->begin + i])) {
            backend_error(error, error_size, fn->begin + i + 1, fn->name,
                          "invalid operands for '%s'", op);
            free(depth);
            free(queue);
            return 0;
        }
        if (!strcmp(op, "retagg") && parameter_ty(a) != fn->ret) {
            backend_error(error, error_size, fn->begin + i + 1, fn->name,
                          "aggregate return type does not match function return type");
            free(depth);
            free(queue);
            return 0;
        }
        if (extended_aggregate_ir_types && !strcmp(op, "ret") && ty(a) != fn->ret) {
            backend_error(error, error_size, fn->begin + i + 1, fn->name,
                          "return type does not match function return type");
            free(depth);
            free(queue);
            return 0;
        }
        if (variadic_frame_ir && !strcmp(op, "vastart") && !fn->variadic) {
            backend_error(error, error_size, fn->begin + i + 1, fn->name,
                          "vastart requires a variadic function");
            free(depth);
            free(queue);
            return 0;
        }
        if (!ir_stack_effect(op, a, b, &need, &delta, &terminal, &conditional, module,
                             runtime_noreturn)) {
            backend_error(error, error_size, fn->begin + i + 1, fn->name,
                          "unknown instruction or invalid operands near '%s'", op);
            free(depth);
            free(queue);
            return 0;
        }
    }
    for (size_t i = 0; i < count; i++)
        depth[i] = -1;
    size_t head = 0, tail = 0;
    *maximum_depth = 0;
    depth[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        size_t rel = queue[head++], line_no = fn->begin + rel;
        char op[32] = {0}, a[128] = {0}, b[128] = {0};
        int fields = scan_ir(lines[line_no], "%31s %127s %127s", op, a, b);
        if (fields < 1) {
            if (rel + 1 < count && depth[rel + 1] < 0) {
                depth[rel + 1] = depth[rel];
                queue[tail++] = rel + 1;
            }
            continue;
        }
        int need, delta, terminal, conditional;
        if (!ir_stack_effect(op, a, b, &need, &delta, &terminal, &conditional, module,
                             runtime_noreturn)) {
            backend_error(error, error_size, line_no + 1, fn->name,
                          "unknown instruction or invalid operands near '%s'", op);
            goto invalid;
        }
        if (depth[rel] < need) {
            backend_error(error, error_size, line_no + 1, fn->name,
                          "stack underflow at '%s' (needs %d value%s, has %d)", op, need,
                          need == 1 ? "" : "s", depth[rel]);
            goto invalid;
        }
        if (dynamic_stack_ir &&
            ((!strcmp(op, "stackmark") && depth[rel] != 0) ||
             ((!strcmp(op, "stackalloc") || !strcmp(op, "stackrestore")) &&
              depth[rel] != 1))) {
            backend_error(error, error_size, line_no + 1, fn->name,
                          "dynamic stack operation requires an otherwise empty stack");
            goto invalid;
        }
        int next_depth = depth[rel] + delta;
        if ((size_t)next_depth > *maximum_depth)
            *maximum_depth = (size_t)next_depth;
        size_t successors[2], nsucc = 0;
        if (!terminal || conditional) {
            if (rel + 1 < count)
                successors[nsucc++] = rel + 1;
            else if (!terminal) {
                backend_error(error, error_size, line_no + 1, fn->name,
                              "reachable control flow falls through past 'end'");
                goto invalid;
            }
        }
        if (!strcmp(op, "jmp") || conditional) {
            char target[128] = {0};
            if (scan_ir(lines[line_no], "%*s %127s", target) != 1) {
                backend_error(error, error_size, line_no + 1, fn->name, "missing branch target");
                goto invalid;
            }
            size_t target_line = fn->end;
            for (size_t j = fn->begin; j < fn->end; j++) {
                char lop[32] = {0}, label[128] = {0};
                if (scan_ir(lines[j], "%31s %127s", lop, label) == 2 && !strcmp(lop, "label") &&
                    !strcmp(label, target)) {
                    target_line = j;
                    break;
                }
            }
            if (target_line == fn->end) {
                backend_error(error, error_size, line_no + 1, fn->name,
                              "branch target '%s' is not defined in this function", target);
                goto invalid;
            }
            successors[nsucc++] = target_line - fn->begin;
        }
        for (size_t k = 0; k < nsucc; k++) {
            size_t to = successors[k];
            if (depth[to] < 0) {
                depth[to] = next_depth;
                queue[tail++] = to;
            } else if (depth[to] != next_depth) {
                backend_error(
                    error, error_size, fn->begin + to + 1, fn->name,
                    "inconsistent stack depth at control-flow merge (incoming %d, existing %d)",
                    next_depth, depth[to]);
                goto invalid;
            }
        }
    }
    if (stackalloc_live_ir)
        fn->stack_depths = depth;
    else
        free(depth);
    free(queue);
    return 1;
invalid:
    free(depth);
    free(queue);
    return 0;
}

/**
 * @brief Checks call targets and argument counts against declarations.
 * @param lines IR instruction lines to validate or translate.
 * @param functions Function metadata being inspected.
 * @param count Number of elements or records.
 * @param error Writable character buffer receiving output.
 * @param error_size Capacity or size in bytes.
 * @param module Nonzero for module-mode IR.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int validate_call_sites(char **lines, const Function *functions, size_t count, char *error,
                               size_t error_size, int module) {
    for (size_t f = 0; f < count; f++) {
        const Function *fn = &functions[f];
        for (size_t i = fn->begin; i < fn->end; i++) {
            char op[32] = {0}, name[128] = {0}, argtext[32] = {0};
            int fields = scan_ir(lines[i], "%31s %127s %31s", op, name, argtext);
            if (function_pointer_ir_types && fields >= 2 && !strcmp(op, "fnaddr")) {
                if (!findfn((Function *)functions, count, name)) {
                    backend_error(error, error_size, i + 1, fn->name,
                                  "address of unknown function '%s'", name);
                    return 0;
                }
                continue;
            }
            if (fields < 3 ||
                (strcmp(op, "call") && strcmp(op, "callagg") &&
                 (!module || strcmp(op, "runtime_call"))))
                continue;
            unsigned argc;
            if (!parse_u(argtext, &argc)) {
                backend_error(error, error_size, i + 1, fn->name,
                              "invalid argument count for call '%s'", name);
                return 0;
            }
            Function *callee = !strcmp(op, "runtime_call")
                ? NULL : findfn((Function *)functions, count, name);
            if (callee) {
                if ((!strcmp(op, "callagg")) != address_parameter(callee->ret)) {
                    backend_error(error, error_size, i + 1, fn->name,
                                  "call kind does not match return type of '%s'", name);
                    return 0;
                }
                if (argc != callee->param_count) {
                    backend_error(error, error_size, i + 1, fn->name,
                                  "call to '%s' expects %zu argument%s, got %u", name,
                                  callee->param_count, callee->param_count == 1 ? "" : "s", argc);
                    return 0;
                }
                continue;
            }
            const RuntimeRoutine *routine =
                (!module || !strcmp(op, "runtime_call") || !strcmp(op, "callagg"))
                    ? find_runtime_routine(name) : NULL;
            if (routine && ((!strcmp(op, "callagg")) != (routine->result == 'e')))
                routine = NULL;
            unsigned expected = routine ? routine->argc : 0;
            int known = routine != NULL;
            if (!known) {
                backend_error(error, error_size, i + 1, fn->name, "call to unknown function '%s'",
                              name);
                return 0;
            }
            int is_printf = !strcmp(name, "printf");
            int is_fprintf = !strcmp(name, "fprintf");
            int is_scanf = !strcmp(name, "scanf");
            int is_fscanf = !strcmp(name, "fscanf");
            int is_sprintf = !strcmp(name, "sprintf");
            int is_snprintf = !strcmp(name, "snprintf");
            if (((is_printf || is_scanf) && (!argc || argc > 255 || !(argc & 1))) ||
                ((is_fprintf || is_fscanf || is_sprintf) && (argc < 2 || argc > 256 || (argc & 1))) ||
                (is_snprintf && (argc < 3 || argc > 257 || !(argc & 1))) ||
                (!is_printf && !is_fprintf && !is_scanf && !is_fscanf && !is_sprintf && !is_snprintf && argc != expected)) {
                backend_error(error, error_size, i + 1, fn->name,
                              "runtime call '%s' expects %u argument%s, got %u", name, expected,
                              expected == 1 ? "" : "s", argc);
                return 0;
            }
        }
    }
    return 1;
}

/* Typed stack analysis. Integer literals are initially T_CONST because the IR
 * intentionally leaves their contextual width to the consuming instruction. */
/**
 * @brief Checks whether an actual stack type can satisfy an expected type.
 * @param actual Actual operand type.
 * @param expected Required operand type.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int type_compatible(Ty actual, Ty expected) {
    if (expected == T_FLOAT)
        return actual == T_FLOAT;
    if (actual == expected || actual == T_CONST)
        return 1;
    if (expected == T_CONST)
        return actual != T_BAD && actual != T_VOID;
    return 0;
}
/**
 * @brief Infers the scalar type of a named IR storage operation.
 * @param lines IR instruction lines to validate or translate.
 * @param line_count Number of elements or records.
 * @param fn Current function, used to scope local aliases.
 * @param name Identifier name to search for or emit.
 * @param op Text input to parse, compare, or serialize.
 * @param explicit_type Text input to parse, compare, or serialize.
 * @return Inferred scalar backend type, or T_BAD when the name has no known scalar type.
 */
static Ty scalar_type_for_name(char **lines, size_t line_count, const Function *fn,
                               const char *name, const char *op, const char *explicit_type) {
    int local = !strcmp(op, "lload") || !strcmp(op, "lstore") || !strcmp(op, "laddr");
    size_t begin = float_ir_types && local ? fn->begin : 0;
    size_t end = float_ir_types && local ? fn->end : line_count;
    for (size_t i = begin; i < end; i++) {
        char word[32] = {0}, n[128] = {0}, b[128] = {0}, c[32] = {0}, d[32] = {0};
        int fields = scan_ir(lines[i], "%31s %127s %127s %31s %31s", word, n, b, c, d);
        if (fields >= 3 && !strcmp(n, name)) {
            if (local) {
                if (!strcmp(word, "localalias") && fields == 5)
                    return ty(d);
            } else if (!strcmp(op, "gload") || !strcmp(op, "gstore")) {
                if (!strcmp(word, "global") && fields >= 4)
                    return ty(b);
                if (!strcmp(word, "extern_global") && fields == 3)
                    return ty(b);
                if (!strcmp(word, "globalalias") && fields == 5)
                    return ty(d);
            }
        }
    }
    (void)explicit_type;
    return T_BAD;
}

/**
 * @brief Checks whether an IR global address names declared storage.
 * @param lines IR instruction lines.
 * @param line_count Number of lines.
 * @param name Storage symbol to find.
 * @return Nonzero if the global storage is declared.
 */
static int global_address_known(char **lines, size_t line_count, const char *name) {
    for (size_t i = 0; i < line_count; i++) {
        char word[32] = {0}, symbol[128] = {0};
        if (scan_ir(lines[i], "%31s %127s", word, symbol) == 2 &&
            !strcmp(symbol, name) &&
            (!strcmp(word, "global") || !strcmp(word, "extern_global") ||
             !strcmp(word, "globalblock") || !strcmp(word, "globalalias")))
            return 1;
    }
    return 0;
}
/**
 * @brief Checks whether a backend type can be used as an integer.
 * @param t Backend type whose integer suitability is tested.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int is_integerish(Ty t) {
    return t == T_CONST || t == T_BYTE || t == T_SBYTE || t == T_UBYTE || t == T_SHORT || t == T_USHORT || t == T_INT || t == T_LONG ||
           t == T_UINT || t == T_ULONG;
}
/**
 * @brief Checks type compatibility for a stored value and destination.
 * @param actual Actual operand type.
 * @param expected Required operand type.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int storage_compatible(Ty actual, Ty expected) {
    if (expected == T_FLOAT)
        return actual == T_FLOAT;
    if (actual == expected || actual == T_CONST)
        return 1;
    if (is_integerish(actual) && is_integerish(expected))
        return 1;
    return 0;
}
/**
 * @brief Checks type compatibility for an IR operation operand.
 * @param actual Actual operand type.
 * @param expected Required operand type.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int operation_compatible(Ty actual, Ty expected) {
    if (expected == T_FLOAT)
        return actual == T_FLOAT;
    if (actual == expected || actual == T_CONST)
        return 1;
    if (is_integerish(actual) && is_integerish(expected))
        return 1;
    return 0;
}
/**
 * @brief Validates one instruction while propagating typed stack state.
 * @param lines IR instruction lines to validate or translate.
 * @param line_count Number of elements or records.
 * @param fn Function metadata being inspected.
 * @param functions Function metadata being inspected.
 * @param function_count Number of elements or records.
 * @param line_no One-based IR line number associated with the typed stack transfer.
 * @param stack Text input to parse, compare, or serialize.
 * @param depth Current recursive nesting depth.
 * @param capacity Number of entries available in the stack.
 * @param error Writable character buffer receiving output.
 * @param error_size Capacity or size in bytes.
 * @param module Nonzero for module-mode IR.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int typed_transfer(char **lines, size_t line_count, const Function *fn,
                          const Function *functions, size_t function_count, size_t line_no,
                          signed char *stack, int *depth, size_t capacity, char *error, size_t error_size,
                          int module) {
    char op[32] = {0}, a[128] = {0}, b[128] = {0};
    if (scan_ir(lines[line_no], "%31s %127s %127s", op, a, b) < 1)
        return 1;
    Ty t = ty(a), u = ty(b), x = T_BAD, y = T_BAD;
    int d = *depth;
#define TYPE_ERROR(msg)                                                                            \
    do {                                                                                           \
        backend_error(error, error_size, line_no + 1, fn->name, "%s", msg);                        \
        return 0;                                                                                  \
    } while (0)
#define REQUIRE_TOP(expected, what)                                                                \
    do {                                                                                           \
        if (d < 1 || !type_compatible((Ty)stack[d - 1], (expected)))                               \
            TYPE_ERROR(what);                                                                      \
    } while (0)
#define POPN(n)                                                                                    \
    do {                                                                                           \
        d -= (n);                                                                                  \
    } while (0)
#define PUSH(v)                                                                                    \
    do {                                                                                           \
        if (d < 0 || (size_t)d >= capacity)                                                        \
            TYPE_ERROR("typed stack exceeds verified depth");                                     \
        stack[d++] = (signed char)(v);                                                             \
    } while (0)
    if (!strcmp(op, "func") || !strcmp(op, "end") || !strcmp(op, "label") || !strcmp(op, "local") ||
        !strcmp(op, "param") || !strcmp(op, "localblock") || !strcmp(op, "localalias") ||
        !strcmp(op, "global") || !strcmp(op, "globalblock") || !strcmp(op, "globalalias") ||
        !strcmp(op, "stringlit") || !strcmp(op, "jmp") ||
        (variadic_frame_ir && !strcmp(op, "variadic"))) {
        *depth = d;
        return 1;
    }
    if (dynamic_stack_ir && !strcmp(op, "stackmark")) {
        PUSH(T_PTR);
    } else if ((dynamic_stack_ir && !strcmp(op, "stackalloc")) ||
               (stackalloc_live_ir && !strcmp(op, "stackalloc_live"))) {
        if (d < 1 || (!strcmp(op, "stackalloc") && d != 1) ||
            !is_integerish((Ty)stack[d - 1]))
            TYPE_ERROR("stackalloc requires an integer size");
        stack[d - 1] = T_PTR;
    } else if (dynamic_stack_ir && !strcmp(op, "stackrestore")) {
        if (d != 1 || (Ty)stack[d - 1] != T_PTR)
            TYPE_ERROR("stackrestore requires a stack mark");
        POPN(1);
    } else if (variadic_frame_ir && !strcmp(op, "vastart")) {
        REQUIRE_TOP(T_PTR, "vastart requires a pointer");
        POPN(1);
    } else if (variadic_frame_ir && !strcmp(op, "vaarg")) {
        REQUIRE_TOP(T_PTR, "vaarg requires a pointer");
        stack[d - 1] = T_PTR;
    } else if (variadic_frame_ir && !strcmp(op, "vacopy")) {
        if (d < 2 || !type_compatible((Ty)stack[d - 2], T_PTR) ||
            !type_compatible((Ty)stack[d - 1], T_PTR))
            TYPE_ERROR("vacopy requires two pointers");
        POPN(2);
    } else if (variadic_aggregate_ir && !strcmp(op, "vaargagg")) {
        if (d < 2 || !type_compatible((Ty)stack[d - 2], T_PTR) ||
            !type_compatible((Ty)stack[d - 1], T_PTR))
            TYPE_ERROR("vaargagg requires destination and list pointers");
        POPN(1);
        stack[d - 1] = T_PTR;
    } else if (extended_real_ir && !strcmp(op, "extfrom")) {
        if (d < 2 || !type_compatible((Ty)stack[d - 2], T_PTR) ||
            !type_compatible((Ty)stack[d - 1], t))
            TYPE_ERROR("extfrom requires a destination pointer and matching value");
        POPN(2);
        PUSH(T_PTR);
    } else if (extended_real_ir && !strcmp(op, "extto")) {
        REQUIRE_TOP(T_PTR, "extto requires a source pointer");
        stack[d - 1] = (signed char)t;
    } else if (extended_real_ir && extended_operation(op)) {
        int kind = extended_operation(op);
        int needed = kind == 2 ? 3 : kind == 4 ? 1 : 2;
        if (d < needed)
            TYPE_ERROR("binary80 operation stack underflow");
        for (int j = 0; j < needed; ++j)
            if (!type_compatible((Ty)stack[d - needed + j], T_PTR))
                TYPE_ERROR("binary80 operation requires pointers");
        POPN(needed);
        PUSH(kind >= 3 ? T_INT : T_PTR);
    } else if (!strcmp(op, "push")) {
        PUSH(T_CONST);
    } else if (!strcmp(op, "pushu")) {
        PUSH(u);
    } else if (!strcmp(op, "pushr")) {
        PUSH(T_REAL);
    } else if (!strcmp(op, "pushf") && float_ir_types) {
        PUSH(T_FLOAT);
    } else if (!strcmp(op, "gaddr")) {
        if (!global_address_known(lines, line_count, a))
            TYPE_ERROR("address refers to an undeclared global");
        PUSH(T_PTR);
    } else if (!strcmp(op, "straddr") || !strcmp(op, "laddr") ||
               (function_pointer_ir_types && !strcmp(op, "fnaddr")) ||
               (large_runtime_call_ir && !strcmp(op, "weak_fnaddr"))) {
        PUSH(T_PTR);
    } else if (!strcmp(op, "addr")) {
        unsigned slot;
        if (!parse_u(a, &slot) || slot >= CRUNCH_LOCAL_LIMIT || slot >= fn->local_count)
            TYPE_ERROR("address refers to an undeclared local slot");
        PUSH(T_PTR);
    } else if (!strcmp(op, "load")) {
        unsigned slot;
        if (!parse_u(a, &slot) || slot >= CRUNCH_LOCAL_LIMIT || slot >= fn->local_count)
            TYPE_ERROR("load refers to an undeclared local slot");
        PUSH(fn->local_types[slot] == T_BAD ? T_LONG : fn->local_types[slot]);
    } else if (!strcmp(op, "store")) {
        unsigned slot;
        if (!parse_u(a, &slot) || slot >= CRUNCH_LOCAL_LIMIT || slot >= fn->local_count || u == T_BAD ||
            u == T_VOID)
            TYPE_ERROR("invalid local store slot or type");
        if (fn->local_types[slot] != T_BAD && d >= 1 &&
            !storage_compatible((Ty)stack[d - 1], fn->local_types[slot]))
            TYPE_ERROR("stored value type does not match local slot");
        if (fn->local_types[slot] != T_BAD && u != fn->local_types[slot])
            TYPE_ERROR("store type does not match local slot declaration");
        if (d < 1)
            TYPE_ERROR("stack underflow at local store");
        POPN(1);
    } else if (!strcmp(op, "lload") || !strcmp(op, "gload")) {
        x = scalar_type_for_name(lines, line_count, fn, a, op, b);
        if (x == T_BAD || x == T_VOID)
            TYPE_ERROR("unknown storage name or invalid load type");
        if (u != x)
            TYPE_ERROR("load type does not match storage declaration");
        PUSH(x);
    } else if (!strcmp(op, "lstore") || !strcmp(op, "gstore")) {
        x = scalar_type_for_name(lines, line_count, fn, a, op, b);
        if (x == T_BAD || x == T_VOID || u != x)
            TYPE_ERROR("store type does not match storage declaration");
        if (d < 1 || !storage_compatible((Ty)stack[d - 1], x))
            TYPE_ERROR("stored value type does not match storage declaration");
        POPN(1);
    } else if (!strcmp(op, "drop")) {
        if (d < 1)
            TYPE_ERROR("stack underflow at 'drop'");
        POPN(1);
    } else if (!strcmp(op, "dup")) {
        if (d < 1)
            TYPE_ERROR("stack underflow at 'dup'");
        Ty duplicate = (Ty)stack[d - 1];
        PUSH(duplicate);
    } else if (!strcmp(op, "cast")) {
        if (!(pointer_integer_cast_ir && t == T_PTR && u == T_ULONG) &&
            (t == T_BAD || u == T_BAD || t == T_VOID || u == T_VOID || t == T_PTR || u == T_PTR))
            TYPE_ERROR("invalid cast types");
        if (d < 1 || !operation_compatible((Ty)stack[d - 1], t))
            TYPE_ERROR("cast source type does not match stack value");
        stack[d - 1] = (signed char)u;
    } else if (!strcmp(op, "neg") || !strcmp(op, "bitnot")) {
        if (t == T_BAD || t == T_VOID || t == T_PTR)
            TYPE_ERROR("invalid unary operation type");
        if (t == T_FLOAT && !strcmp(op, "bitnot"))
            TYPE_ERROR("bitwise not is not valid for float");
        REQUIRE_TOP(t, "unary operation type does not match stack value");
        stack[d - 1] = (signed char)t;
    } else if (!strcmp(op, "not") || !strcmp(op, "bool")) {
        if (t == T_BAD && !strcmp(op, "bool")) {
            if (d < 1)
                TYPE_ERROR("boolean operation requires a stack value");
        } else {
            if (t == T_BAD || t == T_VOID)
                TYPE_ERROR("invalid boolean operation type");
            REQUIRE_TOP(t, "boolean operation type does not match stack value");
        }
        stack[d - 1] = T_INT;
    } else if (!strcmp(op, "add") || !strcmp(op, "sub") || !strcmp(op, "mul") || !strcmp(op, "pow") ||
               !strcmp(op, "div") || !strcmp(op, "rem") || !strcmp(op, "band") ||
               !strcmp(op, "bor") || !strcmp(op, "bxor") || !strcmp(op, "shl") ||
               !strcmp(op, "shr") || !strcmp(op, "eq") || !strcmp(op, "ne") || !strcmp(op, "lt") ||
               !strcmp(op, "le") || !strcmp(op, "gt") || !strcmp(op, "ge")) {
        if (t == T_BAD || t == T_VOID)
            TYPE_ERROR("invalid binary operation type");
        if (!strcmp(op, "pow") && t != T_REAL)
            TYPE_ERROR("power requires real operands");
        if (d < 2)
            TYPE_ERROR("stack underflow at binary operation");
        x = (Ty)stack[d - 2];
        y = (Ty)stack[d - 1];
        if (!strcmp(op, "pow") && (x != T_REAL || y != T_REAL))
            TYPE_ERROR("power requires real stack values");
        if (!operation_compatible(x, t) || !operation_compatible(y, t))
            TYPE_ERROR("binary operand types do not match instruction type");
        if ((!strcmp(op, "band") || !strcmp(op, "bor") || !strcmp(op, "bxor") ||
             !strcmp(op, "shl") || !strcmp(op, "shr") || !strcmp(op, "rem")) &&
            (t == T_REAL || t == T_FLOAT))
            TYPE_ERROR("operation is not valid for floating-point operands");
        POPN(2);
        Ty result = (!strcmp(op, "eq") || !strcmp(op, "ne") || !strcmp(op, "lt") ||
                     !strcmp(op, "le") || !strcmp(op, "gt") || !strcmp(op, "ge")) ? T_INT : t;
        PUSH(result);
    } else if (version_36_ir && !strcmp(op, "ptrdiff")) {
        if (d < 2 || !type_compatible((Ty)stack[d - 2], T_PTR) ||
            !type_compatible((Ty)stack[d - 1], T_PTR))
            TYPE_ERROR("pointer difference requires two pointers");
        POPN(2);
        PUSH(T_LONG);
    } else if (!strcmp(op, "ptradd") || !strcmp(op, "ptrsub") || !strcmp(op, "ptradd_bytes")) {
        unsigned stride;
        if ((!strcmp(op, "ptradd_bytes") && (!parse_storage_size(a, &stride) || !stride)) ||
            (!strcmp(op, "ptradd_bytes") ? 0 : (t == T_BAD || t == T_VOID)) || d < 2)
            TYPE_ERROR("invalid pointer arithmetic operands");
        if (((Ty)stack[d - 2] != T_PTR && (Ty)stack[d - 2] != T_CONST) ||
            !is_integerish((Ty)stack[d - 1]))
            TYPE_ERROR("pointer arithmetic requires pointer and integer offset");
        POPN(2);
        PUSH(T_PTR);
    } else if (!strcmp(op, "loadind")) {
        if (t == T_BAD || t == T_VOID)
            TYPE_ERROR("invalid indirect load type");
        REQUIRE_TOP(T_PTR, "indirect load requires a pointer");
        stack[d - 1] = (signed char)t;
    } else if (!strcmp(op, "storeind")) {
        if (t == T_BAD || t == T_VOID || d < 2)
            TYPE_ERROR("invalid indirect store operands");
        if ((Ty)stack[d - 2] != T_PTR || !type_compatible((Ty)stack[d - 1], t))
            TYPE_ERROR("indirect store requires pointer and matching value type");
        POPN(2);
        PUSH(t);
    } else if (!strcmp(op, "jz") || !strcmp(op, "jnz")) {
        REQUIRE_TOP(T_INT, "conditional branch requires an integer boolean");
        POPN(1);
    } else if (!strcmp(op, "ret")) {
        if (t != fn->ret)
            TYPE_ERROR("return instruction type does not match function return type");
        REQUIRE_TOP(t, "return value type does not match function return type");
        POPN(1);
    } else if (extended_aggregate_ir_types && !strcmp(op, "retagg")) {
        if (parameter_ty(a) != fn->ret || !address_parameter(fn->ret))
            TYPE_ERROR("aggregate return type does not match function return type");
        REQUIRE_TOP(T_PTR, "aggregate return requires a pointer");
        POPN(1);
    } else if (function_pointer_ir_types && !strcmp(op, "callptr")) {
        Function signature;
        if (!indirect_signature(lines[line_no], &signature))
            TYPE_ERROR("invalid indirect call signature");
        unsigned argc = (unsigned)signature.param_count;
        int aggregate = address_parameter(signature.ret);
        if (d < (int)argc + 1 + aggregate ||
            (Ty)stack[d - (int)argc - 1] != T_PTR)
            TYPE_ERROR("indirect call requires a function pointer");
        if (aggregate && (Ty)stack[d - (int)argc - 2] != T_PTR)
            TYPE_ERROR("aggregate indirect call requires a result pointer");
        for (unsigned i = 0; i < argc; ++i) {
            Ty actual = (Ty)stack[d - (int)argc + (int)i];
            Ty expected = signature.params[i].type;
            if (address_parameter(expected) ? actual != T_PTR
                                          : !type_compatible(actual, expected))
                TYPE_ERROR("indirect call argument type does not match signature");
        }
        POPN((int)argc + 1);
        if (!aggregate)
            PUSH(signature.ret == T_VOID ? T_CONST : signature.ret);
    } else if (!strcmp(op, "call") || (extended_aggregate_ir_types && !strcmp(op, "callagg")) ||
               (module && !strcmp(op, "runtime_call"))) {
        unsigned argc;
        int aggregate_call = !strcmp(op, "callagg");
        if (!parse_u(b, &argc) || argc + (unsigned)aggregate_call > (unsigned)d)
            TYPE_ERROR("invalid call argument count or stack depth");
        Function *callee = !strcmp(op, "runtime_call")
            ? NULL : findfn((Function *)functions, function_count, a);
        if ((!strcmp(a, "end") || !strcmp(a, "exit") || !strcmp(a, "crunch_exit_group")) &&
            (!strcmp(op, "runtime_call") || (!module && !callee))) {
            if (argc != 1 || !is_integerish((Ty)stack[d - 1]))
                TYPE_ERROR("exit requires one integer exit status");
            POPN(1);
        } else if (callee) {
            if (argc != callee->param_count)
                TYPE_ERROR("call argument count does not match function signature");
            if (aggregate_call != address_parameter(callee->ret))
                TYPE_ERROR("call kind does not match function return type");
            if (aggregate_call && (Ty)stack[d - (int)argc - 1] != T_PTR)
                TYPE_ERROR("aggregate call requires a result pointer");
            for (unsigned i = 0; i < argc; i++) {
                Ty actual = (Ty)stack[d - (int)argc + (int)i];
                Ty expected = callee->params[i].type;
                if (address_parameter(expected) ? actual != T_PTR
                                              : !type_compatible(actual, expected))
                    TYPE_ERROR("call argument type does not match function parameter");
            }
            POPN((int)argc);
            if (!aggregate_call)
                PUSH(callee->ret == T_VOID ? T_CONST : callee->ret);
        } else {
            const RuntimeRoutine *routine = find_runtime_routine(a);
            if (crunch_is_ctype_name(a) &&
                (argc != 1 || !is_integerish((Ty)stack[d - 1])))
                TYPE_ERROR("ctype routine requires one integer argument");
            if ((!strcmp(a, "strtol") || !strcmp(a, "strtoul") ||
                 !strcmp(a, "strtoll") || !strcmp(a, "strtoull") ||
                 !strcmp(a, "strtoimax") || !strcmp(a, "strtoumax")) &&
                (argc != 3 || !type_compatible((Ty)stack[d - 3], T_PTR) ||
                 !type_compatible((Ty)stack[d - 2], T_PTR) ||
                 !is_integerish((Ty)stack[d - 1]))) {
                if (!strcmp(a, "strtol") || !strcmp(a, "strtoul"))
                    TYPE_ERROR("strtol/strtoul requires two pointers and an integer base");
                TYPE_ERROR("integer conversion requires two pointers and an integer base");
            }
            if ((!strcmp(a, "strtod") || !strcmp(a, "strtof") || !strcmp(a, "strtold")) &&
                (argc != 2 || !type_compatible((Ty)stack[d - 2], T_PTR) ||
                 !type_compatible((Ty)stack[d - 1], T_PTR)))
                TYPE_ERROR("real conversion requires two pointers");
            if (aggregate_call && (!routine || routine->result != 'e' ||
                                   (Ty)stack[d - (int)argc - 1] != T_PTR))
                TYPE_ERROR("extended runtime call requires a result pointer");
            if (!strcmp(a, "vsnprintf") &&
                (argc != 4 || !type_compatible((Ty)stack[d - 4], T_PTR) ||
                 !is_integerish((Ty)stack[d - 3]) ||
                 !type_compatible((Ty)stack[d - 2], T_PTR) ||
                 !type_compatible((Ty)stack[d - 1], T_PTR)))
                TYPE_ERROR("vsnprintf requires a buffer, capacity, format and argument list");
            if (!strcmp(a, "scanf") || !strcmp(a, "fscanf")) {
                unsigned fixed = !strcmp(a, "fscanf") ? 2 : 1;
                if (argc < fixed || argc > fixed + 254 || ((argc - fixed) & 1))
                    TYPE_ERROR("invalid formatted input argument count");
                for (unsigned i = 0; i < fixed; ++i)
                    if (!type_compatible((Ty)stack[d - (int)argc + (int)i], T_PTR))
                        TYPE_ERROR("formatted input requires stream and format pointers");
                for (unsigned i = fixed; i < argc; i += 2)
                    if (!type_compatible((Ty)stack[d - (int)argc + (int)i], T_PTR) ||
                        !is_integerish((Ty)stack[d - (int)argc + (int)i + 1]))
                        TYPE_ERROR("formatted input requires pointer destinations");
            }
            if (!strcmp(a, "printf") || !strcmp(a, "fprintf") || !strcmp(a, "sprintf") || !strcmp(a, "snprintf")) {
                unsigned fixed = !strcmp(a, "snprintf") ? 3 :
                                 (!strcmp(a, "fprintf") || !strcmp(a, "sprintf")) ? 2 : 1;
                if (argc < fixed || argc > fixed + 254 || ((argc - fixed) & 1) ||
                    !type_compatible((Ty)stack[d - (int)argc], T_PTR) ||
                    (fixed == 2 && !type_compatible((Ty)stack[d - (int)argc + 1], T_PTR)) ||
                    (fixed == 3 && (!is_integerish((Ty)stack[d - (int)argc + 1]) ||
                                    !type_compatible((Ty)stack[d - (int)argc + 2], T_PTR))))
                    TYPE_ERROR("printf/fprintf/snprintf requires valid fixed arguments and value/tag pairs");
                for (unsigned i = fixed; i < argc; i += 2) {
                    Ty value = (Ty)stack[d - (int)argc + (int)i];
                    Ty tag = (Ty)stack[d - (int)argc + (int)i + 1];
                    if (value == T_FLOAT)
                        TYPE_ERROR("formatted float arguments must be promoted to real");
                    if (value == T_VOID || !is_integerish(tag))
                        TYPE_ERROR("invalid printf/fprintf/snprintf value/tag pair");
                }
            }
            Ty result = T_INT;
            if (routine) {
                switch (routine->result) {
                case 'p': result = T_PTR; break;
                case 'l': result = T_LONG; break;
                case 'u': result = T_UINT; break;
                case 'U': result = T_ULONG; break;
                case 'r': result = T_REAL; break;
                case 'f': result = T_FLOAT; break;
                case 'c':
                case 'n': result = T_CONST; break;
                default: break;
                }
            }
            if (argc) {
                for (unsigned i = 0; i < argc; i++)
                    if ((Ty)stack[d - (int)argc + (int)i] == T_VOID)
                        TYPE_ERROR("invalid runtime call argument");
            }
            POPN((int)argc);
            if (!aggregate_call)
                PUSH(result);
        }
    } else if (!strcmp(op, "array_alloc")) {
        REQUIRE_TOP(T_LONG, "array allocation count must be an integer");
        POPN(1);
        PUSH(T_LONG);
        PUSH(T_PTR);
    } else if (!strcmp(op, "array_resize") || !strcmp(op, "array_resize_bytes")) {
        unsigned stride;
        if ((!strcmp(op, "array_resize_bytes") && (!parse_u(a, &stride) || !stride)) || d < 3 ||
            ((Ty)stack[d - 3] != T_PTR && (Ty)stack[d - 3] != T_CONST) ||
            !is_integerish((Ty)stack[d - 2]) || !is_integerish((Ty)stack[d - 1]))
            TYPE_ERROR("array resize requires pointer, old length, and new length");
        POPN(3);
        PUSH(T_LONG);
        PUSH(T_PTR);
    } else { /* The structural pass reports unknown opcodes with its richer context. */
    }
    *depth = d;
#undef TYPE_ERROR
#undef REQUIRE_TOP
#undef POPN
#undef PUSH
    return 1;
}
/**
 * @brief Checks typed stack behavior and returns within a function.
 * @param lines IR instruction lines to validate or translate.
 * @param line_count Number of elements or records.
 * @param fn Function metadata being inspected.
 * @param functions Function metadata being inspected.
 * @param function_count Number of elements or records.
 * @param error Writable character buffer receiving output.
 * @param error_size Capacity or size in bytes.
 * @param module Nonzero for module-mode IR.
 * @param maximum_depth Maximum reachable evaluation-stack depth.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int validate_function_types(char **lines, size_t line_count, const Function *fn,
                                   const Function *functions, size_t function_count, char *error,
                                   size_t error_size, int module, size_t maximum_depth) {
    size_t n = fn->end - fn->begin, width = maximum_depth + 1;
    if (!n)
        return 1;
    if (width && n > 67108864u / width) {
        snprintf(error, error_size, "function '%s' is too large for typed IR verification",
                 fn->name);
        return 0;
    }
    size_t cells = n * width;
    signed char *states = malloc(cells);
    int *depths = malloc(n * sizeof(*depths));
    size_t *queue = malloc(n * sizeof(*queue));
    if (!states || !depths || !queue) {
        free(states);
        free(depths);
        free(queue);
        snprintf(error, error_size, "out of memory verifying stack types in function '%s'",
                 fn->name);
        return 0;
    }
    memset(states, 0, cells);
    for (size_t i = 0; i < n; i++)
        depths[i] = -1;
    size_t head = 0, tail = 0;
    depths[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        size_t rel = queue[head++], ln = fn->begin + rel;
        int d = depths[rel];
        signed char *st = states + rel * width;
        if (!typed_transfer(lines, line_count, fn, functions, function_count, ln, st, &d, width, error,
                            error_size, module))
            goto bad;
        char op[32] = {0}, target[128] = {0};
        scan_ir(lines[ln], "%31s %127s", op, target);
        size_t succ[2], ns = 0;
        int terminal = !strcmp(op, "ret") || !strcmp(op, "jmp") || !strcmp(op, "end") ||
                       ((!strcmp(target, "end") || !strcmp(target, "exit") ||
                         !strcmp(target, "crunch_exit_group")) &&
                        (!strcmp(op, "runtime_call") ||
                         (!module && !strcmp(op, "call") &&
                          !findfn((Function *)functions, function_count, target)))),
            cond = !strcmp(op, "jz") || !strcmp(op, "jnz");
        if (!terminal || cond) {
            if (rel + 1 < n)
                succ[ns++] = rel + 1;
        }
        if (!strcmp(op, "jmp") || cond) {
            size_t target_line = fn->end;
            for (size_t j = fn->begin; j < fn->end; j++) {
                char lo[32] = {0}, la[128] = {0};
                if (scan_ir(lines[j], "%31s %127s", lo, la) == 2 && !strcmp(lo, "label") &&
                    !strcmp(la, target)) {
                    target_line = j;
                    break;
                }
            }
            if (target_line < fn->end)
                succ[ns++] = target_line - fn->begin;
        }
        for (size_t k = 0; k < ns; k++) {
            size_t to = succ[k];
            signed char *dst = states + to * width;
            int old = depths[to];
            if (old < 0) {
                depths[to] = d;
                memcpy(dst, st, (size_t)d);
                queue[tail++] = to;
            } else {
                if (old != d) {
                    snprintf(error, error_size,
                             "IR line %zu (function '%s'): inconsistent typed stack depth",
                             fn->begin + to + 1, fn->name);
                    goto bad;
                }
                int changed = 0;
                for (int j = 0; j < d; j++) {
                    Ty a = (Ty)dst[j], b = (Ty)st[j];
                    if (a == b)
                        continue;
                    if (a == T_FLOAT || b == T_FLOAT) {
                        snprintf(error, error_size,
                                 "IR line %zu (function '%s'): incompatible stack types at "
                                 "control-flow merge (%d versus %d)",
                                 fn->begin + to + 1, fn->name, (int)a, (int)b);
                        goto bad;
                    } else if (a == T_CONST && b != T_VOID) {
                        dst[j] = (signed char)b;
                        changed = 1;
                    } else if (b == T_CONST && a != T_VOID)
                        continue;
                    else {
                        snprintf(error, error_size,
                                 "IR line %zu (function '%s'): incompatible stack types at "
                                 "control-flow merge (%d versus %d)",
                                 fn->begin + to + 1, fn->name, (int)a, (int)b);
                        goto bad;
                    }
                }
                (void)changed;
            }
        }
    }
    free(states);
    free(depths);
    free(queue);
    return 1;
bad:
    free(states);
    free(depths);
    free(queue);
    return 0;
}

/**
 * @brief Validates typed evaluation stacks for all parsed functions.
 * @param lines IR instruction lines to validate or translate.
 * @param line_count Number of elements or records.
 * @param functions Function metadata being inspected.
 * @param count Number of elements or records.
 * @param error Writable character buffer receiving output.
 * @param error_size Capacity or size in bytes.
 * @param module Nonzero for module-mode IR.
 * @return One when the operation succeeds or the condition holds; zero otherwise.
 */
static int validate_ir_stacks(char **lines, size_t line_count, Function *functions,
                              size_t count, char *error, size_t error_size, int module) {
    for (size_t i = 0; i < count; i++) {
        if (functions[i].external)
            continue;
        for (size_t j = functions[i].begin; j < functions[i].end; j++) {
            char op[32] = {0}, name[128] = {0}, type[32] = {0};
            int fields = scan_ir(lines[j], "%31s %127s %31s", op, name, type);
            if (!strcmp(op, "gaddr") &&
                (fields < 2 || !global_address_known(lines, line_count, name))) {
                backend_error(error, error_size, j + 1, functions[i].name,
                              "address refers to an undeclared global");
                return 0;
            }
            if ((!strcmp(op, "gload") || !strcmp(op, "gstore")) &&
                (fields < 3 || scalar_type_for_name(lines, line_count, &functions[i], name, op, type) !=
                                   ty(type) || ty(type) == T_BAD || ty(type) == T_VOID)) {
                backend_error(error, error_size, j + 1, functions[i].name,
                              "invalid global storage name or type");
                return 0;
            }
            if (float_ir_types &&
                (!strcmp(op, "laddr") || !strcmp(op, "lload") || !strcmp(op, "lstore"))) {
                Ty declared = scalar_type_for_name(lines, line_count, &functions[i], name, op, type);
                if (fields < 2 || declared == T_BAD ||
                    (strcmp(op, "laddr") && (fields < 3 || declared != ty(type)))) {
                    backend_error(error, error_size, j + 1, functions[i].name,
                                  "invalid local storage name or type");
                    return 0;
                }
            }
        }
        size_t maximum_depth;
        if (!validate_function_stack(lines, &functions[i], error, error_size, module,
                                     functions, count, &maximum_depth))
            return 0;
        if (!validate_function_types(lines, line_count, &functions[i], functions, count, error,
                                     error_size, module, maximum_depth))
            return 0;
    }
    return 1;
}

/**
 * @brief Emits normalization instructions for a backend value type.
 * @param out Output FILE stream receiving normalized assembly.
 * @param t Backend type whose representation is normalized.
 * @return No value; the function performs the documented output or state update.
 */
static void normalize(FILE *out, Ty t) {
    if (t == T_PTR)
        return;
    else if (t == T_BYTE || t == T_UBYTE)
        fputs("  and $255, %rax\n", out);
    else if (t == T_SBYTE)
        fputs("  movsbq %al, %rax\n", out);
    else if (t == T_SHORT)
        fputs("  movswq %ax, %rax\n", out);
    else if (t == T_USHORT)
        fputs("  movzwq %ax, %rax\n", out);
    else if (t == T_INT)
        fputs("  movslq %eax, %rax\n", out);
    else if (t == T_UINT || t == T_FLOAT)
        fputs("  mov %eax, %eax\n", out);
}
/**
 * @brief Selects masked binary32 exceptions, rounding and gradual underflow.
 * @param out Output FILE stream receiving assembly.
 * @return No value; emits the floating-point environment setup.
 */
static void emit_float_env_begin(FILE *out) {
    fputs("  sub $16, %rsp\n  stmxcsr (%rsp)\n  movl (%rsp), %edx\n"
          "  and $0xffff1fbf, %edx\n  or $0x1f80, %edx\n"
          "  movl %edx, 4(%rsp)\n  ldmxcsr 4(%rsp)\n", out);
}

/**
 * @brief Restores caller floating-point control and retains accrued exception flags.
 * @param out Output FILE stream receiving assembly.
 * @return No value; emits the floating-point environment restoration.
 */
static void emit_float_env_end(FILE *out) {
    fputs("  stmxcsr 4(%rsp)\n  movl 4(%rsp), %edx\n  and $63, %edx\n"
          "  orl %edx, (%rsp)\n  ldmxcsr (%rsp)\n  add $16, %rsp\n", out);
}

/**
 * @brief Emits a typed numeric conversion.
 * @param out Output FILE stream receiving assembly.
 * @param from Source type.
 * @param to Destination type.
 * @param label Unique local label number used by generated branches.
 * @param error Writable diagnostic buffer.
 * @param es Capacity of the diagnostic buffer.
 * @return No value; emits the conversion or reports an error.
 */
static void emit_cast(FILE *out, Ty from, Ty to, unsigned *label, char *error, size_t es) {
    unsigned id;
    int from_float = from == T_FLOAT;
    if (pointer_integer_cast_ir && from == T_PTR && to == T_ULONG)
        return;
    if (from == T_BAD || to == T_BAD || from == T_VOID || to == T_VOID || from == T_PTR ||
        to == T_PTR) {
        snprintf(error, es, "invalid cast type");
        return;
    }
    if (from == to) {
        if (to == T_SBYTE || to == T_UBYTE || to == T_SHORT || to == T_USHORT || to == T_UINT || to == T_FLOAT ||
            (unsigned_ir_types && (to == T_BYTE || to == T_INT)))
            normalize(out, to);
        return;
    }
    if (to == T_FLOAT) {
        if (from == T_SBYTE || from == T_UBYTE || from == T_SHORT || from == T_USHORT)
            normalize(out, from);
        if (from == T_REAL) {
            fputs("  movq %rax, %xmm0\n  cvtsd2ss %xmm0, %xmm0\n", out);
        } else if (from == T_ULONG) {
            id = (*label)++;
            fprintf(out,
                    "  test %%rax, %%rax\n  jns .Lcast_f32_u64_small_%u\n"
                    "  mov %%rax, %%rcx\n  and $1, %%eax\n  shr %%rcx\n"
                    "  or %%rax, %%rcx\n  cvtsi2ssq %%rcx, %%xmm0\n"
                    "  addss %%xmm0, %%xmm0\n  jmp .Lcast_f32_u64_done_%u\n"
                    ".Lcast_f32_u64_small_%u:\n  cvtsi2ssq %%rax, %%xmm0\n"
                    ".Lcast_f32_u64_done_%u:\n", id, id, id, id);
        } else {
            if (from == T_BYTE)
                fputs("  and $255, %eax\n", out);
            else if (from == T_INT)
                fputs("  movslq %eax, %rax\n", out);
            else if (from == T_UINT)
                fputs("  mov %eax, %eax\n", out);
            fputs("  cvtsi2ssq %rax, %xmm0\n", out);
        }
        fputs("  movq %xmm0, %rax\n  mov %eax, %eax\n", out);
        return;
    }
    if (from_float) {
        fputs("  movq %rax, %xmm0\n  cvtss2sd %xmm0, %xmm0\n  movq %xmm0, %rax\n", out);
        if (to == T_REAL)
            return;
        from = T_REAL;
    }
    if (to == T_REAL) {
        if (from == T_SBYTE || from == T_UBYTE || from == T_SHORT || from == T_USHORT)
            normalize(out, from);
        if (from == T_ULONG) {
            id = (*label)++;
            fprintf(out,
                    "  test %%rax, %%rax\n  jns .Lcast_u64_small_%u\n"
                    "  mov %%rax, %%rcx\n  and $1, %%eax\n  shr %%rcx\n"
                    "  or %%rax, %%rcx\n  cvtsi2sdq %%rcx, %%xmm0\n"
                    "  addsd %%xmm0, %%xmm0\n  jmp .Lcast_u64_done_%u\n"
                    ".Lcast_u64_small_%u:\n  cvtsi2sdq %%rax, %%xmm0\n"
                    ".Lcast_u64_done_%u:\n  movq %%xmm0, %%rax\n",
                    id, id, id, id);
            return;
        }
        if (from == T_BYTE)
            fputs("  and $255, %eax\n", out);
        else if (from == T_INT)
            fputs("  movslq %eax, %rax\n", out);
        else if (from == T_UINT)
            fputs("  mov %eax, %eax\n", out);
        fputs("  cvtsi2sdq %rax, %xmm0\n  movq %xmm0, %rax\n", out);
        return;
    }
    if (from == T_REAL) {
        id = (*label)++;
        fprintf(out, "  movq %%rax, %%xmm0\n  ucomisd %%xmm0, %%xmm0\n  jp .Lcast_invalid_%u\n",
                id);
        if (to == T_SBYTE) {
            fputs("  movabs $0xc060000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jb .Lcast_invalid_%u\n", id);
            fputs("  movabs $0x4060000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jae .Lcast_invalid_%u\n", id);
        } else if (to == T_SHORT) {
            fputs("  movabs $0xc0e0000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jb .Lcast_invalid_%u\n", id);
            fputs("  movabs $0x40e0000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jae .Lcast_invalid_%u\n", id);
        } else if (to == T_USHORT) {
            fputs("  movabs $0xbff0000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jbe .Lcast_invalid_%u\n", id);
            fputs("  movabs $0x40f0000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jae .Lcast_invalid_%u\n", id);
        } else if (to == T_UBYTE || (to == T_BYTE && from_float)) {
            fputs("  movabs $0xbff0000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jbe .Lcast_invalid_%u\n", id);
            fputs("  movabs $0x4070000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jae .Lcast_invalid_%u\n", id);
        } else if (to == T_UINT || to == T_ULONG) {
            fputs("  movabs $0xbff0000000000000, %rax\n  movq %rax, %xmm1\n"
                  "  ucomisd %xmm1, %xmm0\n", out);
            fprintf(out, "  jbe .Lcast_invalid_%u\n", id);
            fprintf(out, "  movabs $0x%s, %%rax\n  movq %%rax, %%xmm1\n"
                         "  ucomisd %%xmm1, %%xmm0\n  jae .Lcast_invalid_%u\n",
                    to == T_UINT ? "41f0000000000000" : "43f0000000000000", id);
            if (to == T_ULONG) {
                fputs("  movabs $0x43e0000000000000, %rax\n  movq %rax, %xmm1\n"
                      "  ucomisd %xmm1, %xmm0\n", out);
                fprintf(out, "  jb .Lcast_unsigned_small_%u\n"
                             "  subsd %%xmm1, %%xmm0\n  cvttsd2siq %%xmm0, %%rax\n"
                             "  movabs $0x8000000000000000, %%rcx\n  xor %%rcx, %%rax\n"
                             "  jmp .Lcast_done_%u\n.Lcast_unsigned_small_%u:\n",
                        id, id, id);
            }
        } else if (to == T_LONG) {
            fputs(
                "  movabs $0xc3e0000000000000, %rax\n  movq %rax, %xmm1\n  ucomisd %xmm1, %xmm0\n",
                out);
            fprintf(out, "  jb .Lcast_invalid_%u\n", id);
            fputs(
                "  movabs $0x43e0000000000000, %rax\n  movq %rax, %xmm1\n  ucomisd %xmm1, %xmm0\n",
                out);
            fprintf(out, "  jae .Lcast_invalid_%u\n", id);
        } else {
            fputs(
                "  movabs $0xc1e0000000000000, %rax\n  movq %rax, %xmm1\n  ucomisd %xmm1, %xmm0\n",
                out);
            fprintf(out, "  jb .Lcast_invalid_%u\n", id);
            fputs(
                "  movabs $0x41e0000000000000, %rax\n  movq %rax, %xmm1\n  ucomisd %xmm1, %xmm0\n",
                out);
            fprintf(out, "  jae .Lcast_invalid_%u\n", id);
        }
        fputs("  cvttsd2siq %xmm0, %rax\n", out);
        if (to == T_SBYTE || to == T_UBYTE || to == T_SHORT || to == T_USHORT)
            normalize(out, to);
        else if (to == T_INT)
            fputs("  movslq %eax, %rax\n", out);
        else if (to == T_UINT)
            fputs("  mov %eax, %eax\n", out);
        fprintf(out,
                "  jmp .Lcast_done_%u\n.Lcast_invalid_%u:\n  xor %%eax, %%eax\n.Lcast_done_%u:\n",
                id, id, id);
        return;
    }
    if (to == T_SBYTE || to == T_UBYTE || to == T_SHORT || to == T_USHORT) {
        normalize(out, to);
        return;
    }
    if (from == T_SBYTE || from == T_UBYTE || from == T_SHORT || from == T_USHORT)
        normalize(out, from);
    if (to == T_BYTE) {
        fputs("  and $255, %rax\n", out);
        return;
    }
    if (to == T_INT) {
        fputs("  movslq %eax, %rax\n", out);
        return;
    }
    if (to == T_UINT) {
        fputs("  mov %eax, %eax\n", out);
        return;
    }
    if (to == T_LONG || to == T_ULONG) {
        if (from == T_BYTE)
            fputs("  and $255, %eax\n", out);
        else if (from == T_INT)
            fputs("  movslq %eax, %rax\n", out);
        else if (from == T_UINT)
            fputs("  mov %eax, %eax\n", out);
    }
}
/**
 * @brief Emits machine instructions for a typed binary operation.
 * @param out Output FILE stream receiving assembly.
 * @param op Text input to parse, compare, or serialize.
 * @param t Backend type of the binary operation.
 * @param label Unique local label number used for generated branches.
 * @param error Writable character buffer receiving output.
 * @param es Capacity of the diagnostic-message buffer.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_binary(FILE *out, const char *op, Ty t, unsigned *label, char *error, size_t es) {
    if (t == T_REAL || t == T_FLOAT) {
        fputs("  pop %rcx\n  pop %rax\n", out);
        if (t == T_SHORT)
            fputs("  movswl %ax, %eax\n  movswl %cx, %ecx\n", out);
        else if (t == T_USHORT)
            fputs("  movzwl %ax, %eax\n  movzwl %cx, %ecx\n", out);
        if (t == T_FLOAT)
            emit_float_env_begin(out);
        fputs("  movq %rax, %xmm0\n  movq %rcx, %xmm1\n", out);
        if (!strcmp(op, "add"))
            fputs(t == T_FLOAT ? "  addss %xmm1, %xmm0\n" : "  addsd %xmm1, %xmm0\n", out);
        else if (!strcmp(op, "sub"))
            fputs(t == T_FLOAT ? "  subss %xmm1, %xmm0\n" : "  subsd %xmm1, %xmm0\n", out);
        else if (!strcmp(op, "mul"))
            fputs(t == T_FLOAT ? "  mulss %xmm1, %xmm0\n" : "  mulsd %xmm1, %xmm0\n", out);
        else if (!strcmp(op, "div"))
            fputs(t == T_FLOAT ? "  divss %xmm1, %xmm0\n" : "  divsd %xmm1, %xmm0\n", out);
        else if (!strcmp(op, "eq") || !strcmp(op, "ne") || !strcmp(op, "lt") || !strcmp(op, "le") ||
                 !strcmp(op, "gt") || !strcmp(op, "ge")) {
            unsigned id = (*label)++;
            fputs(t == T_FLOAT ? "  ucomiss %xmm1, %xmm0\n" : "  ucomisd %xmm1, %xmm0\n", out);
            if (!strcmp(op, "eq")) {
                fputs("  sete %al\n  setnp %cl\n  and %cl, %al\n", out);
            } else if (!strcmp(op, "ne")) {
                fputs("  setne %al\n  setp %cl\n  or %cl, %al\n", out);
            } else {
                const char *cc = !strcmp(op, "lt")   ? "setb"
                                 : !strcmp(op, "le") ? "setbe"
                                 : !strcmp(op, "gt") ? "seta"
                                                     : "setae";
                fprintf(out, "  %s %%al\n", cc);
                if (strcmp(op, "gt")) {
                    fputs("  setnp %cl\n  and %cl, %al\n", out);
                }
            }
            fprintf(out, ".Lfc_done_%u:\n  movzbq %%al, %%rax\n", id);
            if (t == T_FLOAT)
                emit_float_env_end(out);
            fputs("  push %rax\n", out);
            return;
        } else {
            snprintf(error, es, "unsupported real operation '%s'", op);
            return;
        }
        fputs("  movq %xmm0, %rax\n", out);
        if (t == T_FLOAT)
            normalize(out, t);
        if (t == T_FLOAT)
            emit_float_env_end(out);
        fputs("  push %rax\n", out);
        return;
    }
    fputs("  pop %rcx\n  pop %rax\n", out);
    if (unsigned_ir_types && (t == T_BYTE || t == T_UBYTE))
        fputs("  and $255, %eax\n  and $255, %ecx\n", out);
    if (t == T_SBYTE)
        fputs("  movsbl %al, %eax\n  movsbl %cl, %ecx\n", out);
    int wide = t == T_LONG || t == T_ULONG || t == T_BYTE || t == T_UBYTE || t == T_PTR;
    const char *suffix = wide ? "q" : "l";
    if (!strcmp(op, "add"))
        fprintf(out, "  add%s %%%s, %%%s\n", suffix,
                wide ? "rcx" : "ecx", wide ? "rax" : "eax");
    else if (!strcmp(op, "sub"))
        fprintf(out, "  sub%s %%%s, %%%s\n", suffix,
                wide ? "rcx" : "ecx", wide ? "rax" : "eax");
    else if (!strcmp(op, "mul"))
        fprintf(out, "  imul%s %%%s, %%%s\n", suffix,
                wide ? "rcx" : "ecx", wide ? "rax" : "eax");
    else if (!strcmp(op, "div") || !strcmp(op, "rem")) {
        if (t == T_BYTE || t == T_UBYTE) {
            fputs("  xor %edx, %edx\n  div %rcx\n", out);
            if (!strcmp(op, "rem"))
                fputs("  mov %rdx, %rax\n", out);
        } else if (t == T_UINT || t == T_USHORT) {
            fputs("  xor %edx, %edx\n  div %ecx\n", out);
            if (!strcmp(op, "rem"))
                fputs("  mov %edx, %eax\n", out);
        } else if (t == T_ULONG) {
            fputs("  xor %edx, %edx\n  div %rcx\n", out);
            if (!strcmp(op, "rem"))
                fputs("  mov %rdx, %rax\n", out);
        } else if (t == T_INT || t == T_SHORT || t == T_SBYTE) {
            fputs("  cltd\n  idiv %ecx\n", out);
            if (!strcmp(op, "rem"))
                fputs("  mov %edx, %eax\n", out);
        } else {
            fputs("  cqto\n  idiv %rcx\n", out);
            if (!strcmp(op, "rem"))
                fputs("  mov %rdx, %rax\n", out);
        }
    } else if (!strcmp(op, "band"))
        fputs("  and %rcx, %rax\n", out);
    else if (!strcmp(op, "bor"))
        fputs("  or %rcx, %rax\n", out);
    else if (!strcmp(op, "bxor"))
        fputs("  xor %rcx, %rax\n", out);
    else if (!strcmp(op, "shl"))
        fprintf(out, "  shl%s %%cl, %%%s\n", suffix, wide ? "rax" : "eax");
    else if (!strcmp(op, "shr"))
        fprintf(out, "  %s%s %%cl, %%%s\n",
                t == T_BYTE || t == T_UBYTE || t == T_USHORT || t == T_UINT || t == T_ULONG ? "shr" : "sar", suffix,
                wide ? "rax" : "eax");
    else if (!strcmp(op, "eq") || !strcmp(op, "ne") || !strcmp(op, "lt") || !strcmp(op, "le") ||
             !strcmp(op, "gt") || !strcmp(op, "ge")) {
        const char *cc;
        if (t == T_BYTE || t == T_UBYTE || t == T_USHORT || t == T_UINT || t == T_ULONG)
            cc = !strcmp(op, "eq")   ? "sete"
                 : !strcmp(op, "ne") ? "setne"
                 : !strcmp(op, "lt") ? "setb"
                 : !strcmp(op, "le") ? "setbe"
                 : !strcmp(op, "gt") ? "seta"
                                     : "setae";
        else if (t == T_PTR)
            cc = !strcmp(op, "eq")   ? "sete"
                 : !strcmp(op, "ne") ? "setne"
                 : !strcmp(op, "lt") ? "setb"
                 : !strcmp(op, "le") ? "setbe"
                 : !strcmp(op, "gt") ? "seta"
                                     : "setae";
        else
            cc = !strcmp(op, "eq")   ? "sete"
                 : !strcmp(op, "ne") ? "setne"
                 : !strcmp(op, "lt") ? "setl"
                 : !strcmp(op, "le") ? "setle"
                 : !strcmp(op, "gt") ? "setg"
                                     : "setge";
        fprintf(out, "  cmp%s %%%s, %%%s\n  %s %%al\n  movzbq %%al, %%rax\n", suffix,
                wide ? "rcx" : "ecx", wide ? "rax" : "eax", cc);
    } else {
        snprintf(error, es, "unknown integer operation '%s'", op);
        return;
    }
    normalize(out, t);
    fputs("  push %rax\n", out);
}
/**
 * @brief Emits overflow-checked allocation and prefix-copy code for fixed-stride elements.
 * @param out Output FILE stream receiving assembly.
 * @param stride Byte size of one array element.
 * @param id Id identifier or count.
 * @param free_name Assembly symbol used to release array storage.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_array_resize_bytes(FILE *out, unsigned stride, unsigned id,
                                    const char *free_name) {
    fprintf(out,
            "  pop %%rdi\n  pop %%rsi\n  pop %%rdx\n"
            "  push %%rdx\n  push %%rsi\n  push %%rdi\n"
            "  test %%rdi, %%rdi\n  jle .Lstruct_resize_clear_%u\n"
            "  imul $%u, %%rdi, %%rdi\n  jo .Lstruct_resize_clear_%u\n"
            "  call __crunch_alloc_bytes\n  test %%rax, %%rax\n"
            "  jz .Lstruct_resize_clear_%u\n"
            "  pop %%r10\n  pop %%r8\n  pop %%r9\n"
            "  mov %%r8, %%rcx\n  cmp %%r10, %%rcx\n"
            "  cmova %%r10, %%rcx\n"
            "  imul $%u, %%rcx, %%rcx\n"
            "  mov %%r9, %%rsi\n  mov %%rax, %%rdi\n"
            "  mov %%rax, %%r11\n  rep movsb\n"
            "  push %%r11\n  mov %%r9, %%rdi\n"
            "  mov %%r8, %%rsi\n  imul $%u, %%rsi, %%rsi\n"
            "  call %s\n  pop %%rax\n"
            "  push %%r10\n  push %%rax\n"
            "  jmp .Lstruct_resize_done_%u\n"
            ".Lstruct_resize_clear_%u:\n"
            "  pop %%r10\n  pop %%r8\n  pop %%r9\n"
            "  test %%r9, %%r9\n  jz .Lstruct_resize_zero_%u\n"
            "  test %%r8, %%r8\n  jle .Lstruct_resize_zero_%u\n"
            "  mov %%r8, %%rsi\n  imul $%u, %%rsi, %%rsi\n"
            "  jo .Lstruct_resize_zero_%u\n"
            "  mov %%r9, %%rdi\n  call %s\n"
            ".Lstruct_resize_zero_%u:\n  push $0\n  push $0\n"
            ".Lstruct_resize_done_%u:\n",
            id, stride, id, id, stride, stride, free_name, id, id, id, id, stride, id,
            free_name, id, id);
}
/**
 * @brief Adds IR line and function information to a backend error.
 * @param error Writable character buffer receiving output.
 * @param error_size Capacity or size in bytes.
 * @param line One-based IR line number of the error.
 * @param function Function name for diagnostic context.
 * @return No value; the function performs the documented output or state update.
 */
static void add_ir_error_context(char *error, size_t error_size, size_t line,
                                 const char *function) {
    char message[256];
    if (!error || !error_size || !error[0] || !line)
        return;
    if (active_backend_diagnostic) {
        active_backend_diagnostic->ir_line = line;
        if (function) {
            snprintf(active_backend_diagnostic->function_storage,
                     sizeof(active_backend_diagnostic->function_storage), "%s", function);
            active_backend_diagnostic->function = active_backend_diagnostic->function_storage;
        }
        return;
    }
    if (!strncmp(error, "IR line ", 8))
        return;
    snprintf(message, sizeof(message), "%s", error);
    snprintf(error, error_size, "IR line %zu (function '%s'): %s", line, function, message);
}
/**
 * @brief Finds whether a global symbol is declared but not defined in this module.
 * @param globals Global symbol table.
 * @param count Number of entries in the table.
 * @param name Symbol to look up.
 * @return Nonzero for an external global, zero otherwise.
 */
static int external_global(const Global *globals, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++)
        if (!strcmp(globals[i].name, name))
            return globals[i].external;
    return 0;
}

/**
 * @brief Identifies a global using local-exec thread-local storage.
 * @param globals Global symbol table.
 * @param count Number of entries in the table.
 * @param name Symbol to look up.
 * @return Nonzero for thread-local storage, zero otherwise.
 */
static int thread_local_global(const Global *globals, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++)
        if (!strcmp(globals[i].name, name))
            return globals[i].thread_local;
    return 0;
}

/**
 * @brief Emits an address-based binary80 operation.
 * @param out Destination assembly stream.
 * @param op Validated binary80 instruction.
 * @return No value.
 */
static void emit_extended_operation(FILE *out, const char *op) {
    int kind = extended_operation(op);
    if (kind == 1) {
        fputs("  pop %rsi\n  pop %rdi\n"
              "  mov (%rsi), %rax\n  movzwl 8(%rsi), %ecx\n", out);
        if (!strcmp(op, "extneg"))
            fputs("  xor $32768, %ecx\n", out);
        fputs("  mov %rax, (%rdi)\n  mov %cx, 8(%rdi)\n  push %rdi\n", out);
        return;
    }
    if (kind == 2)
        fputs("  pop %rdx\n  pop %rsi\n  pop %rdi\n", out);
    else if (kind == 3)
        fputs("  pop %rdx\n  pop %rsi\n", out);
    else
        fputs("  pop %rsi\n", out);
    fputs("  sub $16, %rsp\n  fnstcw (%rsp)\n  movzwl (%rsp), %ecx\n"
          "  and $0xf0ff, %ecx\n  or $0x33f, %ecx\n"
          "  mov %cx, 2(%rsp)\n  fldcw 2(%rsp)\n", out);
    if (kind == 2) {
        fputs("  fldt (%rsi)\n  fldt (%rdx)\n", out);
        fputs(!strcmp(op, "extadd") ? "  faddp\n"
              : !strcmp(op, "extsub") ? "  fsubrp\n"
              : !strcmp(op, "extmul") ? "  fmulp\n" : "  fdivrp\n", out);
        fputs("  fstpt (%rdi)\n", out);
    } else {
        fputs(kind == 4 ? "  fldz\n" : "  fldt (%rdx)\n", out);
        fputs("  fldt (%rsi)\n  fucomip\n  fstp %st(0)\n", out);
        if (!strcmp(op, "exteq"))
            fputs("  sete %al\n  setnp %cl\n  and %cl, %al\n", out);
        else if (!strcmp(op, "extne") || kind == 4)
            fputs("  setne %al\n  setp %cl\n  or %cl, %al\n", out);
        else {
            const char *condition = !strcmp(op, "extlt") ? "setb"
                                    : !strcmp(op, "extle") ? "setbe"
                                    : !strcmp(op, "extgt") ? "seta" : "setae";
            fprintf(out, "  %s %%al\n  setnp %%cl\n  and %%cl, %%al\n", condition);
        }
        fputs("  movzbq %al, %rax\n", out);
    }
    fputs("  fldcw (%rsp)\n  add $16, %rsp\n", out);
    fputs(kind == 2 ? "  push %rdi\n" : "  push %rax\n", out);
}

/**
 * @brief Translates one function body from validated IR to x86-64.
 * @param out Output FILE stream receiving assembly.
 * @param lines IR instruction lines to validate or translate.
 * @param fn Function metadata being inspected.
 * @param funcs Function metadata being inspected.
 * @param nf Number of functions in the function table.
 * @param globals Global symbol metadata for external references.
 * @param ng Number of global symbols.
 * @param lblocks Local stack-block metadata used to emit this function.
 * @param laliases Local field-alias metadata used to emit this function.
 * @param error Writable character buffer receiving output.
 * @param es Capacity of the diagnostic-message buffer.
 * @param labels Label table used to resolve branch targets.
 * @param error_line One-based IR line number to report on failure.
 * @param free_name Assembly symbol used to release array storage.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_function_impl(FILE *out, char **lines, const Function *fn, Function *funcs,
                               size_t nf, Global *globals, size_t ng,
                               LocalBlock *lblocks, LocalAlias *laliases, char *error,
                               size_t es, unsigned *labels, size_t *error_line,
                               const char *free_name) {
    unsigned frame_extra = function_pointer_ir_types ? 4 : extended_aggregate_ir_types ? 3 : 2;
    size_t block_base = ((size_t)(fn->local_count + fn->max_call_args + frame_extra) * 8 + 15) &
                        ~(size_t)15;
    size_t frame = block_base + fn->local_block_bytes;
    if (frame % 16)
        frame += 16 - frame % 16;
    if (fn->variadic)
        frame += 176;
    fprintf(out, ".%s %s\n%s:\n  push %%rbp\n  mov %%rsp, %%rbp\n",
            fn->internal ? "local" : "global", fn->name, fn->name);
    if (frame)
        fprintf(out, "  sub $%zu, %%rsp\n", frame);
    if (fn->variadic) {
        static const char *const gp[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        static const char *const fp[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3",
                                         "%xmm4", "%xmm5", "%xmm6", "%xmm7"};
        for (unsigned j = 0; j < 6; ++j)
            fprintf(out, "  mov %s, -%zu(%%rbp)\n", gp[j], frame - j * 8);
        for (unsigned j = 0; j < 8; ++j)
            fprintf(out, "  movups %s, -%zu(%%rbp)\n", fp[j], frame - 48 - j * 16);
    }
    if (memory_aggregate(fn->ret))
        fprintf(out, "  mov %%rdi, -%u(%%rbp)\n",
                (fn->local_count + fn->max_call_args + 3) * 8);
    for (size_t i = fn->begin; i < fn->end; i++) {
        if (error_line)
            *error_line = i + 1;
        char op[32] = {0}, a[128] = {0}, b[128] = {0}, c[128] = {0};
        unsigned slot = 0, index = 0;
        long long val;
        if (scan_ir(lines[i], "%31s %127s %127s %127s", op, a, b, c) < 1)
            continue;
        if (!strcmp(op, "func") || !strcmp(op, "end") || !strcmp(op, "local") ||
            !strcmp(op, "localblock") || !strcmp(op, "localalias") || !strcmp(op, "stringlit") ||
            (variadic_frame_ir && !strcmp(op, "variadic")))
            continue;
        if (dynamic_stack_ir && !strcmp(op, "stackmark")) {
            fputs("  mov %rsp, %rax\n  push %rax\n", out);
            continue;
        }
        if (dynamic_stack_ir && !strcmp(op, "stackalloc")) {
            unsigned id = (*labels)++;
            fprintf(out, "  pop %%rax\n  add $15, %%rax\n  jc .Lstack_overflow_%u\n"
                         "  and $-16, %%rax\n  sub %%rax, %%rsp\n"
                         "  jc .Lstack_overflow_%u\n  mov %%rsp, %%rax\n  push %%rax\n"
                         "  jmp .Lstack_done_%u\n.Lstack_overflow_%u:\n  ud2\n"
                         ".Lstack_done_%u:\n", id, id, id, id, id);
            continue;
        }
        if (stackalloc_live_ir && !strcmp(op, "stackalloc_live")) {
            unsigned id = (*labels)++;
            int depth = fn->stack_depths[i - fn->begin];
            size_t bytes = depth > 0 ? (size_t)(depth - 1) * 8 : 0;
            fprintf(out, "  pop %%rax\n  add $15, %%rax\n  jc .Lstack_overflow_%u\n"
                         "  and $-16, %%rax\n  mov %%rsp, %%rsi\n  sub %%rax, %%rsp\n"
                         "  jc .Lstack_overflow_%u\n  mov %%rsp, %%rdi\n"
                         "  mov $%zu, %%rcx\n  rep movsb\n"
                         "  lea %zu(%%rsp), %%rax\n  push %%rax\n"
                         "  jmp .Lstack_done_%u\n.Lstack_overflow_%u:\n  ud2\n"
                         ".Lstack_done_%u:\n", id, id, bytes, bytes, id, id, id);
            continue;
        }
        if (dynamic_stack_ir && !strcmp(op, "stackrestore")) {
            fputs("  pop %rax\n  mov %rax, %rsp\n", out);
            continue;
        }
        if (variadic_frame_ir && !strcmp(op, "vastart")) {
            unsigned gp = memory_aggregate(fn->ret) ? 1 : 0, fp = 0, stack = 0;
            for (size_t j = 0; j < fn->param_count; ++j)
                if (!parameter_registers(fn->params[j].type, &gp, &fp)) {
                    Ty type = fn->params[j].type;
                    stack = parameter_stack_offset(type, stack) + parameter_stack_words(type);
                }
            fprintf(out, "  pop %%rax\n  movl $%u, (%%rax)\n  movl $%u, 4(%%rax)\n"
                         "  lea %u(%%rbp), %%r10\n  mov %%r10, 8(%%rax)\n"
                         "  lea -%zu(%%rbp), %%r10\n  mov %%r10, 16(%%rax)\n",
                    gp * 8, 48 + fp * 16, 16 + stack * 8, frame);
            continue;
        }
        if (variadic_frame_ir && !strcmp(op, "vaarg")) {
            if (extended_real_ir && !strcmp(a, "ext")) {
                fputs("  pop %rax\n  mov 8(%rax), %r10\n"
                      "  add $15, %r10\n  and $-16, %r10\n"
                      "  lea 16(%r10), %r11\n  mov %r11, 8(%rax)\n"
                      "  push %r10\n", out);
                continue;
            }
            unsigned id = (*labels)++;
            unsigned offset = !strcmp(a, "fp") ? 4 : 0;
            unsigned limit = offset ? 176 : 48;
            unsigned stride = offset ? 16 : 8;
            fprintf(out, "  pop %%rax\n  mov %u(%%rax), %%r10d\n"
                         "  cmp $%u, %%r10d\n  jae .Lva_stack_%u\n"
                         "  addl $%u, %u(%%rax)\n  add 16(%%rax), %%r10\n"
                         "  push %%r10\n  jmp .Lva_done_%u\n"
                         ".Lva_stack_%u:\n  mov 8(%%rax), %%r10\n"
                         "  lea 8(%%r10), %%r11\n  mov %%r11, 8(%%rax)\n"
                         "  push %%r10\n.Lva_done_%u:\n",
                    offset, limit, id, stride, offset, id, id, id);
            continue;
        }
        if (variadic_frame_ir && !strcmp(op, "vacopy")) {
            fputs("  pop %r10\n  pop %rax\n"
                  "  mov 0(%r10), %r11\n  mov %r11, 0(%rax)\n"
                  "  mov 8(%r10), %r11\n  mov %r11, 8(%rax)\n"
                  "  mov 16(%r10), %r11\n  mov %r11, 16(%rax)\n", out);
            continue;
        }
        if (extended_real_ir && extended_operation(op)) {
            emit_extended_operation(out, op);
            continue;
        }
        if (extended_real_ir && (!strcmp(op, "extfrom") || !strcmp(op, "extto"))) {
            int from = !strcmp(op, "extfrom");
            Ty type = ty(a);
            int integer = type == T_INT || type == T_UINT || type == T_LONG || type == T_ULONG;
            if (from)
                fputs("  pop %rax\n  pop %rdi\n", out);
            else
                fputs("  pop %rsi\n", out);
            fputs("  sub $16, %rsp\n  fnstcw (%rsp)\n  movzwl (%rsp), %ecx\n"
                  "  and $0xf0ff, %ecx\n", out);
            fputs(!from && integer ? "  or $0xf3f, %ecx\n" : "  or $0x33f, %ecx\n", out);
            fputs("  mov %cx, 2(%rsp)\n  fldcw 2(%rsp)\n", out);
            if (from) {
                if (type == T_INT)
                    fputs("  movslq %eax, %rax\n", out);
                else if (type == T_UINT)
                    fputs("  mov %eax, %eax\n", out);
                fputs("  mov %rax, 8(%rsp)\n", out);
                if (type == T_ULONG) {
                    unsigned id = (*labels)++;
                    fprintf(out, "  test %%rax, %%rax\n  js .Lextfrom_unsigned_%u\n"
                                 "  fildll 8(%%rsp)\n  jmp .Lextfrom_done_%u\n"
                                 ".Lextfrom_unsigned_%u:\n  fildll 8(%%rsp)\n"
                                 "  movabs $0x43f0000000000000, %%rcx\n"
                                 "  mov %%rcx, 8(%%rsp)\n  fldl 8(%%rsp)\n"
                                 "  faddp\n.Lextfrom_done_%u:\n", id, id, id, id);
                } else if (integer)
                    fputs("  fildll 8(%rsp)\n", out);
                else
                    fputs(type == T_FLOAT ? "  flds 8(%rsp)\n" : "  fldl 8(%rsp)\n", out);
                fputs("  fstpt (%rdi)\n", out);
            } else if (type == T_ULONG) {
                unsigned id = (*labels)++;
                fprintf(out, "  movabs $0x43e0000000000000, %%rcx\n"
                             "  mov %%rcx, 8(%%rsp)\n  fldl 8(%%rsp)\n"
                             "  fldt (%%rsi)\n  fucomip\n  fstp %%st(0)\n"
                             "  jb .Lextto_unsigned_small_%u\n"
                             "  fldt (%%rsi)\n  fldl 8(%%rsp)\n  fsubrp\n"
                             "  fistpll 8(%%rsp)\n  mov 8(%%rsp), %%rax\n"
                             "  movabs $0x8000000000000000, %%rcx\n"
                             "  xor %%rcx, %%rax\n  jmp .Lextto_unsigned_done_%u\n"
                             ".Lextto_unsigned_small_%u:\n  fldt (%%rsi)\n"
                             "  fistpll 8(%%rsp)\n  mov 8(%%rsp), %%rax\n"
                             ".Lextto_unsigned_done_%u:\n", id, id, id, id);
            } else if (integer) {
                fputs("  fldt (%rsi)\n  fistpll 8(%rsp)\n  mov 8(%rsp), %rax\n", out);
                if (type == T_INT)
                    fputs("  movslq %eax, %rax\n", out);
                else if (type == T_UINT)
                    fputs("  mov %eax, %eax\n", out);
            } else {
                fputs("  fldt (%rsi)\n", out);
                fputs(type == T_FLOAT ? "  fstps 8(%rsp)\n  mov 8(%rsp), %eax\n"
                                      : "  fstpl 8(%rsp)\n  mov 8(%rsp), %rax\n", out);
            }
            fputs("  fldcw (%rsp)\n  add $16, %rsp\n", out);
            fputs(from ? "  push %rdi\n" : "  push %rax\n", out);
            continue;
        }
        if (variadic_aggregate_ir && !strcmp(op, "vaargagg")) {
            Ty type = parameter_ty(a);
            unsigned id = (*labels)++;
            unsigned words = parameter_words(type), gp = 0, fp = 0;
            fputs("  pop %rax\n  pop %r11\n", out);
            if (!memory_aggregate(type)) {
                for (unsigned j = 0; j < words; ++j)
                    if (parameter_sse_word(type, j))
                        ++fp;
                    else
                        ++gp;
                fprintf(out, "  mov (%%rax), %%r10d\n  mov 4(%%rax), %%edx\n");
                if (gp)
                    fprintf(out, "  cmp $%u, %%r10d\n  ja .Lvaagg_stack_%u\n",
                            48 - gp * 8, id);
                if (fp)
                    fprintf(out, "  cmp $%u, %%edx\n  ja .Lvaagg_stack_%u\n",
                            176 - fp * 16, id);
                fputs("  mov 16(%rax), %rcx\n", out);
                for (unsigned j = 0; j < words; ++j) {
                    const char *reg = parameter_sse_word(type, j) ? "%rdx" : "%r10";
                    fprintf(out, "  mov (%%rcx,%s), %%rdi\n  mov %%rdi, %u(%%r11)\n"
                                 "  add $%u, %s\n", reg, j * 8,
                            parameter_sse_word(type, j) ? 16 : 8, reg);
                }
                fputs("  mov %r10d, (%rax)\n  mov %edx, 4(%rax)\n", out);
                fprintf(out, "  push %%r11\n  jmp .Lvaagg_done_%u\n.Lvaagg_stack_%u:\n",
                        id, id);
            }
            unsigned bytes = (aggregate_size(type) + 7) & ~7U;
            fputs("  mov 8(%rax), %rsi\n", out);
            if (aligned_aggregate(type))
                fputs("  add $15, %rsi\n  and $-16, %rsi\n", out);
            if (memory_aggregate(type) && aggregate_size(type) <= 16) {
                fprintf(out, "  lea %u(%%rsi), %%r10\n"
                             "  mov %%r10, 8(%%rax)\n  push %%r11\n"
                             "  mov %%r11, %%rdi\n  mov $%u, %%ecx\n  rep movsb\n",
                        bytes, aggregate_size(type));
                continue;
            }
            fprintf(out, "  lea %u(%%rsi), %%r10\n"
                         "  mov %%r10, 8(%%rax)\n  push %%r11\n"
                         "  mov %%r11, %%r10\n  mov $%u, %%ecx\n"
                         ".Lvaagg_copy_%u:\n"
                         "  mov (%%rsi), %%rdi\n  mov %%rdi, (%%r10)\n"
                         "  add $8, %%rsi\n  add $8, %%r10\n"
                         "  sub $8, %%ecx\n  jne .Lvaagg_copy_%u\n",
                    bytes, bytes, id, id);
            if (!memory_aggregate(type))
                fprintf(out, ".Lvaagg_done_%u:\n", id);
            continue;
        }
        if (!strcmp(op, "param")) {
            if (!parse_u(a, &index) || !parse_u(b, &slot)) {
                snprintf(error, es, "invalid param instruction");
                return;
            }
            Ty pt = parameter_ty(c);
            unsigned gp = memory_aggregate(fn->ret) ? 1 : 0, fp = 0, stack = 0;
            for (unsigned j = 0; j < index; j++) {
                Ty prior = fn->params[j].type;
                if (!parameter_registers(prior, &gp, &fp))
                    stack = parameter_stack_offset(prior, stack) + parameter_stack_words(prior);
            }
            stack = parameter_stack_offset(pt, stack);
            unsigned next_gp = gp, next_fp = fp;
            int in_registers = parameter_registers(pt, &next_gp, &next_fp);
            if (memory_aggregate(pt) || pt == T_EXT) {
                fprintf(out, "  lea %u(%%rbp), %%rax\n  mov %%rax, -%u(%%rbp)\n",
                        16 + stack * 8, (slot + 1) * 8);
            } else if (aggregate_param(pt)) {
                static const char *regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
                static const char *xregs[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3",
                                              "%xmm4", "%xmm5", "%xmm6", "%xmm7"};
                unsigned words = parameter_words(pt);
                for (unsigned j = 0; j < words; j++) {
                    if (in_registers && parameter_sse_word(pt, j))
                        fprintf(out, "  movq %s, %%rax\n", xregs[fp++]);
                    else if (in_registers)
                        fprintf(out, "  mov %s, %%rax\n", regs[gp++]);
                    else
                        fprintf(out, "  mov %u(%%rbp), %%rax\n", 16 + (stack + j) * 8);
                    fprintf(out, "  mov %%rax, -%u(%%rbp)\n", (slot + words - j) * 8);
                }
            } else if (pt == T_REAL || pt == T_FLOAT) {
                if (in_registers) {
                    static const char *xregs[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3",
                                                  "%xmm4", "%xmm5", "%xmm6", "%xmm7"};
                    fprintf(out, "  movq %s, %%rax\n", xregs[fp]);
                } else
                    fprintf(out, "  mov %u(%%rbp), %%rax\n", 16 + stack * 8);
                normalize(out, pt);
                fprintf(out, "  mov %%rax, -%u(%%rbp)\n", (slot + 1) * 8);
            } else {
                static const char *regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
                if (in_registers)
                    fprintf(out, "  mov %s, %%rax\n", regs[gp]);
                else
                    fprintf(out, "  mov %u(%%rbp), %%rax\n", 16 + stack * 8);
                normalize(out, pt);
                fprintf(out, "  mov %%rax, -%u(%%rbp)\n", (slot + 1) * 8);
            }
            continue;
        }
        if (!strcmp(op, "label")) {
            if (scan_ir(a, "L%u", &slot) != 1) {
                snprintf(error, es, "invalid label");
                return;
            }
            fprintf(out, ".L%u:\n", slot);
        } else if (!strcmp(op, "jmp") || !strcmp(op, "jz") || !strcmp(op, "jnz")) {
            if (scan_ir(a, "L%u", &slot) != 1) {
                snprintf(error, es, "invalid branch label");
                return;
            }
            if (!strcmp(op, "jmp"))
                fprintf(out, "  jmp .L%u\n", slot);
            else
                fprintf(out, "  pop %%rax\n  test %%rax, %%rax\n  %s .L%u\n",
                        !strcmp(op, "jz") ? "jz" : "jnz", slot);
        } else if (!strcmp(op, "push")) {
            char *end;
            errno = 0;
            val = strtoll(a, &end, 10);
            if (errno || end == a || *end) {
                snprintf(error, es, "invalid push operand");
                return;
            }
            fprintf(out, "  movabs $%lld, %%rax\n  push %%rax\n", val);
        } else if (!strcmp(op, "pushu")) {
            uint64_t value;
            Ty t = ty(b);
            if ((t != T_UINT && t != T_ULONG) ||
                !parse_uint_literal(a, t == T_UINT ? UINT32_MAX : UINT64_MAX, &value)) {
                snprintf(error, es, "invalid unsigned integer literal");
                return;
            }
            fprintf(out, "  movabs $0x%016llx, %%rax\n  push %%rax\n",
                    (unsigned long long)value);
        } else if (!strcmp(op, "pushr")) {
            char *end;
            errno = 0;
            uint64_t bits = strtoull(a, &end, 0);
            if (errno || end == a || *end) {
                snprintf(error, es, "invalid real literal bits");
                return;
            }
            fprintf(out, "  movabs $0x%016llx, %%rax\n  push %%rax\n", (unsigned long long)bits);
        } else if (!strcmp(op, "pushf")) {
            fprintf(out, "  mov $%s, %%eax\n  push %%rax\n", a);
        } else if (!strcmp(op, "array_alloc")) {
            Ty et = ty(a);
            unsigned id = (*labels)++;
            fputs("  pop %rdi\n  test %rdi, %rdi\n", out);
            fprintf(out, "  jle .Larr_zero_%u\n  push %%rdi\n", id);
            if (et == T_SHORT || et == T_USHORT) {
                fprintf(out, "  mov %%rdi, %%rax\n  shl $1, %%rax\n  shr $1, %%rax\n"
                             "  cmp %%rdi, %%rax\n  jne .Larr_failed_%u\n  shl $1, %%rdi\n", id);
            } else if (et == T_INT || et == T_UINT || et == T_FLOAT) {
                fprintf(out,
                        "  mov %%rdi, %%rax\n  shl $2, %%rax\n  shr $2, %%rax\n  cmp %%rdi, "
                        "%%rax\n  jne .Larr_failed_%u\n  shl $2, %%rdi\n",
                        id);
            } else if (et == T_LONG || et == T_ULONG || et == T_REAL || et == T_PTR) {
                fprintf(out,
                        "  mov %%rdi, %%rax\n  shl $3, %%rax\n  shr $3, %%rax\n  cmp %%rdi, "
                        "%%rax\n  jne .Larr_failed_%u\n  shl $3, %%rdi\n",
                        id);
            }
            fputs("  call __crunch_alloc_bytes\n  test %rax, %rax\n", out);
            fprintf(out,
                    "  jz .Larr_failed_%u\n  pop %%rcx\n  push %%rcx\n  push %%rax\n  jmp "
                    ".Larr_done_%u\n.Larr_failed_%u:\n  add $8, %%rsp\n.Larr_zero_%u:\n  push $0\n "
                    " push $0\n.Larr_done_%u:\n",
                    id, id, id, id, id);
        } else if (!strcmp(op, "array_resize_bytes")) {
            unsigned stride, id = (*labels)++;
            if (!parse_u(a, &stride) || !stride) {
                snprintf(error, es, "invalid structure-array element size");
                return;
            }
            emit_array_resize_bytes(out, stride, id, free_name);
        } else if (!strcmp(op, "array_resize")) {
            Ty et = ty(a);
            unsigned id = (*labels)++;
            if (et != T_BYTE && et != T_SBYTE && et != T_UBYTE && et != T_SHORT && et != T_USHORT && et != T_INT && et != T_LONG && et != T_UINT &&
                et != T_ULONG && et != T_REAL && et != T_FLOAT && et != T_PTR) {
                snprintf(error, es, "unsupported dynamic array element type");
                return;
            }
            fputs("  pop %rdi\n  pop %rsi\n  pop %rdx\n  push %rdx\n  push %rsi\n  push %rdi\n  "
                  "test %rdi, %rdi\n",
                  out);
            fprintf(out, "  jle .Lresize_clear_%u\n", id);
            if (et == T_SHORT || et == T_USHORT) {
                fprintf(out, "  mov %%rdi, %%rax\n  shl $1, %%rax\n  shr $1, %%rax\n"
                             "  cmp %%rdi, %%rax\n  jne .Lresize_clear_%u\n  shl $1, %%rdi\n", id);
            } else if (et == T_INT || et == T_UINT || et == T_FLOAT) {
                fprintf(out,
                        "  mov %%rdi, %%rax\n  shl $2, %%rax\n  shr $2, %%rax\n  cmp %%rdi, "
                        "%%rax\n  jne .Lresize_clear_%u\n  shl $2, %%rdi\n",
                        id);
            } else if (et == T_LONG || et == T_ULONG || et == T_REAL || et == T_PTR) {
                fprintf(out,
                        "  mov %%rdi, %%rax\n  shl $3, %%rax\n  shr $3, %%rax\n  cmp %%rdi, "
                        "%%rax\n  jne .Lresize_clear_%u\n  shl $3, %%rdi\n",
                        id);
            }
            fputs("  call __crunch_alloc_bytes\n", out);
            fprintf(out,
                    "  test %%rax, %%rax\n  jz .Lresize_clear_%u\n  push %%rax\n  pop %%rax\n  pop "
                    "%%r10\n  pop %%r8\n  pop %%r9\n  mov %%r8, %%rcx\n  cmp %%r10, %%rcx\n  cmova "
                    "%%r10, %%rcx\n",
                    id);
            if (et == T_SHORT || et == T_USHORT)
                fputs("  shl $1, %rcx\n", out);
            else if (et == T_INT || et == T_UINT || et == T_FLOAT)
                fputs("  shl $2, %rcx\n", out);
            else if (et == T_LONG || et == T_ULONG || et == T_REAL || et == T_PTR)
                fputs("  shl $3, %rcx\n", out);
            fputs("  mov %r9, %rsi\n  mov %rax, %rdi\n  rep movsb\n  push %rax\n  mov %r9, %rdi\n  "
                  "mov %r8, %rsi\n",
                  out);
            if (et == T_SHORT || et == T_USHORT)
                fputs("  shl $1, %rsi\n", out);
            else if (et == T_INT || et == T_UINT || et == T_FLOAT)
                fputs("  shl $2, %rsi\n", out);
            else if (et == T_LONG || et == T_ULONG || et == T_REAL || et == T_PTR)
                fputs("  shl $3, %rsi\n", out);
            fprintf(out, "  call %s\n  pop %%rax\n  push %%r10\n  push %%rax\n", free_name);
            fprintf(out,
                    "  jmp .Lresize_done_%u\n.Lresize_clear_%u:\n  pop %%r10\n  pop %%r8\n  pop "
                    "%%r9\n  test %%r9, %%r9\n  jz .Lresize_zero_%u\n  test %%r8, %%r8\n  jle "
                    ".Lresize_zero_%u\n  mov %%r9, %%rdi\n  mov %%r8, %%rsi\n",
                    id, id, id, id);
            if (et == T_SHORT || et == T_USHORT)
                fputs("  shl $1, %rsi\n", out);
            else if (et == T_INT || et == T_UINT || et == T_FLOAT)
                fputs("  shl $2, %rsi\n", out);
            else if (et == T_LONG || et == T_ULONG || et == T_REAL || et == T_PTR)
                fputs("  shl $3, %rsi\n", out);
            fprintf(out, "  call %s\n", free_name);
            fprintf(out, ".Lresize_zero_%u:\n  push $0\n  push $0\n.Lresize_done_%u:\n", id, id);
        } else if (!strcmp(op, "load") && parse_u(a, &slot)) {
            fprintf(out, "  mov -%u(%%rbp), %%rax\n", (slot + 1) * 8);
            if (slot < CRUNCH_LOCAL_LIMIT && fn->local_types[slot] != T_BAD)
                normalize(out, fn->local_types[slot]);
            fputs("  push %rax\n", out);
        } else if (!strcmp(op, "store") && parse_u(a, &slot)) {
            Ty t = ty(b);
            fputs("  pop %rax\n", out);
            normalize(out, t);
            fprintf(out, "  mov %%rax, -%u(%%rbp)\n", (slot + 1) * 8);
        } else if (!strcmp(op, "lload") || !strcmp(op, "lstore") || !strcmp(op, "laddr")) {
            const char *key = a;
            LocalAlias *la = NULL;
            for (size_t j = 0; j < fn->local_alias_count; j++) {
                LocalAlias *q = &laliases[fn->local_alias_start + j];
                if (!strcmp(q->name, key)) {
                    la = q;
                    break;
                }
            }
            if (!la) {
                snprintf(error, es, "unknown local structure alias '%s'", key);
                return;
            }
            LocalBlock *lb = NULL;
            for (size_t j = 0; j < fn->local_block_count; j++) {
                LocalBlock *q = &lblocks[fn->local_block_start + j];
                if (!strcmp(q->name, la->block)) {
                    lb = q;
                    break;
                }
            }
            if (!lb) {
                snprintf(error, es, "unknown local structure block '%s'", la->block);
                return;
            }
            size_t disp = block_base + lb->frame_offset + lb->size - la->offset;
            Ty t = la->type;
            if (!strcmp(op, "laddr"))
                fprintf(out, "  lea -%zu(%%rbp), %%rax\n  push %%rax\n", disp);
            else if (!strcmp(op, "lload")) {
                if (t == T_BYTE || t == T_UBYTE)
                    fprintf(out, "  movzbq -%zu(%%rbp), %%rax\n", disp);
                else if (t == T_SBYTE)
                    fprintf(out, "  movsbq -%zu(%%rbp), %%rax\n", disp);
                else if (t == T_SHORT)
                    fprintf(out, "  movswq -%zu(%%rbp), %%rax\n", disp);
                else if (t == T_USHORT)
                    fprintf(out, "  movzwq -%zu(%%rbp), %%rax\n", disp);
                else if (t == T_INT)
                    fprintf(out, "  movslq -%zu(%%rbp), %%rax\n", disp);
                else if (t == T_UINT || t == T_FLOAT)
                    fprintf(out, "  movl -%zu(%%rbp), %%eax\n", disp);
                else
                    fprintf(out, "  movq -%zu(%%rbp), %%rax\n", disp);
                fputs("  push %rax\n", out);
            } else {
                t = ty(b);
                fputs("  pop %rax\n", out);
                normalize(out, t);
                if (t == T_BYTE || t == T_SBYTE || t == T_UBYTE)
                    fprintf(out, "  movb %%al, -%zu(%%rbp)\n", disp);
                else if (t == T_SHORT || t == T_USHORT)
                    fprintf(out, "  movw %%ax, -%zu(%%rbp)\n", disp);
                else if (t == T_INT || t == T_UINT || t == T_FLOAT)
                    fprintf(out, "  movl %%eax, -%zu(%%rbp)\n", disp);
                else
                    fprintf(out, "  movq %%rax, -%zu(%%rbp)\n", disp);
            }
        } else if (!strcmp(op, "gload")) {
            Ty t = ty(b);
            int external = external_global(globals, ng, a);
            int tls = thread_local_global(globals, ng, a);
            char address[144];
            if (external || tls) snprintf(address, sizeof address, "(%%rdx)");
            else snprintf(address, sizeof address, "%s(%%rip)", a);
            if (external)
                fprintf(out, "  movq %s@GOTPCREL(%%rip), %%rdx\n", a);
            else if (tls)
                fprintf(out, "  movq %%fs:0, %%rdx\n  leaq %s@TPOFF(%%rdx), %%rdx\n", a);
            if (t == T_BYTE || t == T_UBYTE)
                fprintf(out, "  movzbq %s, %%rax\n", address);
            else if (t == T_SBYTE)
                fprintf(out, "  movsbq %s, %%rax\n", address);
            else if (t == T_SHORT)
                fprintf(out, "  movswq %s, %%rax\n", address);
            else if (t == T_USHORT)
                fprintf(out, "  movzwq %s, %%rax\n", address);
            else if (t == T_INT)
                fprintf(out, "  movslq %s, %%rax\n", address);
            else if (t == T_UINT || t == T_FLOAT)
                fprintf(out, "  movl %s, %%eax\n", address);
            else
                fprintf(out, "  movq %s, %%rax\n", address);
            fputs("  push %rax\n", out);
        } else if (!strcmp(op, "gstore")) {
            Ty t = ty(b);
            int external = external_global(globals, ng, a);
            int tls = thread_local_global(globals, ng, a);
            char address[144];
            if (external || tls) snprintf(address, sizeof address, "(%%rdx)");
            else snprintf(address, sizeof address, "%s(%%rip)", a);
            fputs("  pop %rax\n", out);
            normalize(out, t);
            if (external)
                fprintf(out, "  movq %s@GOTPCREL(%%rip), %%rdx\n", a);
            else if (tls)
                fprintf(out, "  movq %%fs:0, %%rdx\n  leaq %s@TPOFF(%%rdx), %%rdx\n", a);
            if (t == T_BYTE || t == T_SBYTE || t == T_UBYTE)
                fprintf(out, "  movb %%al, %s\n", address);
            else if (t == T_SHORT || t == T_USHORT)
                fprintf(out, "  movw %%ax, %s\n", address);
            else if (t == T_INT || t == T_UINT || t == T_FLOAT)
                fprintf(out, "  movl %%eax, %s\n", address);
            else
                fprintf(out, "  movq %%rax, %s\n", address);
        } else if (!strcmp(op, "gaddr")) {
            int external = external_global(globals, ng, a);
            if (thread_local_global(globals, ng, a))
                fprintf(out, "  movq %%fs:0, %%rax\n  leaq %s@TPOFF(%%rax), %%rax\n  push %%rax\n", a);
            else
                fprintf(out, external ? "  movq %s@GOTPCREL(%%rip), %%rax\n  push %%rax\n"
                                      : "  lea %s(%%rip), %%rax\n  push %%rax\n", a);
        }
        else if (function_pointer_ir_types && !strcmp(op, "fnaddr")) {
            Function *target = findfn(funcs, nf, a);
            if (!target) {
                snprintf(error, es, "unknown function '%s'", a);
                return;
            }
            fprintf(out, target->external
                             ? "  movq %s@GOTPCREL(%%rip), %%rax\n  push %%rax\n"
                             : "  lea %s(%%rip), %%rax\n  push %%rax\n", a);
        }
        else if (large_runtime_call_ir && !strcmp(op, "weak_fnaddr")) {
            fprintf(out, ".weak %s\n"
                         "  movq %s@GOTPCREL(%%rip), %%rax\n  push %%rax\n", a, a);
        }
        else if (!strcmp(op, "straddr"))
            fprintf(out, "  lea %s(%%rip), %%rax\n  push %%rax\n", a);
        else if (!strcmp(op, "addr") && parse_u(a, &slot))
            fprintf(out, "  lea -%u(%%rbp), %%rax\n  push %%rax\n", (slot + 1) * 8);
        else if (!strcmp(op, "loadind")) {
            Ty t = ty(a);
            fputs("  pop %rax\n", out);
            if (t == T_BYTE || t == T_UBYTE)
                fputs("  movzbq (%rax), %rax\n", out);
            else if (t == T_SBYTE)
                fputs("  movsbq (%rax), %rax\n", out);
            else if (t == T_SHORT)
                fputs("  movswq (%rax), %rax\n", out);
            else if (t == T_USHORT)
                fputs("  movzwq (%rax), %rax\n", out);
            else if (t == T_INT)
                fputs("  movslq (%rax), %rax\n", out);
            else if (t == T_UINT || t == T_FLOAT)
                fputs("  movl (%rax), %eax\n", out);
            else
                fputs("  movq (%rax), %rax\n", out);
            fputs("  push %rax\n", out);
        } else if (!strcmp(op, "storeind")) {
            Ty t = ty(a);
            fputs("  pop %rcx\n  pop %rax\n", out);
            if (t == T_BYTE || t == T_UBYTE)
                fputs("  movb %cl, (%rax)\n  movzbq %cl, %rcx\n", out);
            else if (t == T_SBYTE)
                fputs("  movb %cl, (%rax)\n  movsbq %cl, %rcx\n", out);
            else if (t == T_SHORT)
                fputs("  movw %cx, (%rax)\n  movswq %cx, %rcx\n", out);
            else if (t == T_USHORT)
                fputs("  movw %cx, (%rax)\n  movzwq %cx, %rcx\n", out);
            else if (t == T_INT)
                fputs("  movl %ecx, (%rax)\n  movslq %ecx, %rcx\n", out);
            else if (t == T_UINT || t == T_FLOAT)
                fputs("  movl %ecx, (%rax)\n  mov %ecx, %ecx\n", out);
            else
                fputs("  movq %rcx, (%rax)\n", out);
            fputs("  push %rcx\n", out);
        } else if (!strcmp(op, "dup"))
            fputs("  pop %rax\n  push %rax\n  push %rax\n", out);
        else if (!strcmp(op, "drop"))
            fputs("  add $8, %rsp\n", out);
        else if (!strcmp(op, "neg")) {
            Ty t = ty(a);
            fputs("  pop %rax\n", out);
            if (t == T_REAL)
                fputs("  movabs $0x8000000000000000, %rcx\n  xor %rcx, %rax\n", out);
            else if (t == T_FLOAT)
                fputs("  xor $0x80000000, %eax\n", out);
            else if (t == T_INT)
                fputs("  negl %eax\n  movslq %eax, %rax\n", out);
            else
                fputs("  neg %rax\n", out);
            normalize(out, t);
            fputs("  push %rax\n", out);
        } else if (!strcmp(op, "not")) {
            Ty t = ty(a);
            if (t == T_REAL || t == T_FLOAT) {
                fputs("  pop %rax\n", out);
                if (t == T_FLOAT)
                    emit_float_env_begin(out);
                fprintf(out,
                    "  movq %%rax, %%xmm0\n  pxor %%xmm1, %%xmm1\n  %s %%xmm1, %%xmm0\n "
                    " sete %%al\n  setnp %%cl\n  and %%cl, %%al\n  movzbq %%al, %%rax\n",
                    t == T_FLOAT ? "ucomiss" : "ucomisd");
                if (t == T_FLOAT)
                    emit_float_env_end(out);
                fputs("  push %rax\n", out);
            } else if (t == T_SBYTE || t == T_UBYTE || t == T_SHORT || t == T_USHORT || t == T_UINT ||
                       (unsigned_ir_types && (t == T_BYTE || t == T_INT))) {
                fputs("  pop %rax\n", out);
                normalize(out, t);
                fputs("  test %rax, %rax\n  sete %al\n  movzbq %al, %rax\n  push %rax\n", out);
            }
            else
                fputs(
                    "  pop %rax\n  test %rax, %rax\n  sete %al\n  movzbq %al, %rax\n  push %rax\n",
                    out);
        } else if (!strcmp(op, "bitnot")) {
            Ty t = ty(a);
            fputs("  pop %rax\n  not %rax\n", out);
            normalize(out, t);
            fputs("  push %rax\n", out);
        } else if (!strcmp(op, "bool")) {
            Ty t = ty(a);
            if (t == T_REAL || t == T_FLOAT) {
                fputs("  pop %rax\n", out);
                if (t == T_FLOAT)
                    emit_float_env_begin(out);
                fprintf(out,
                    "  movq %%rax, %%xmm0\n  pxor %%xmm1, %%xmm1\n  %s %%xmm1, %%xmm0\n "
                    " setne %%al\n  setp %%cl\n  or %%cl, %%al\n  movzbq %%al, %%rax\n",
                    t == T_FLOAT ? "ucomiss" : "ucomisd");
                if (t == T_FLOAT)
                    emit_float_env_end(out);
                fputs("  push %rax\n", out);
            } else if (t == T_SBYTE || t == T_UBYTE || t == T_SHORT || t == T_USHORT || t == T_UINT ||
                       (unsigned_ir_types && (t == T_BYTE || t == T_INT))) {
                fputs("  pop %rax\n", out);
                normalize(out, t);
                fputs("  test %rax, %rax\n  setne %al\n  movzbq %al, %rax\n  push %rax\n", out);
            }
            else
                fputs(
                    "  pop %rax\n  test %rax, %rax\n  setne %al\n  movzbq %al, %rax\n  push %rax\n",
                    out);
        } else if (!strcmp(op, "cast")) {
            fputs("  pop %rax\n", out);
            int uses_float = ty(a) == T_FLOAT || ty(b) == T_FLOAT;
            if (uses_float)
                emit_float_env_begin(out);
            emit_cast(out, ty(a), ty(b), labels, error, es);
            if (error[0])
                return;
            if (uses_float)
                emit_float_env_end(out);
            fputs("  push %rax\n", out);
        } else if (!strcmp(op, "call") || !strcmp(op, "runtime_call") ||
                   (function_pointer_ir_types && !strcmp(op, "callptr")) ||
                   (extended_aggregate_ir_types && !strcmp(op, "callagg"))) {
            int indirect = !strcmp(op, "callptr");
            Function signature;
            unsigned argc;
            if (indirect) {
                if (!indirect_signature(lines[i], &signature)) {
                    snprintf(error, es, "invalid indirect call signature");
                    return;
                }
                argc = (unsigned)signature.param_count;
            } else if (!parse_u(b, &argc)) {
                snprintf(error, es, "invalid call argument count");
                return;
            }
            if (argc > fn->max_call_args) {
                snprintf(error, es, "invalid call argument count");
                return;
            }
            Function *callee = indirect ? &signature :
                               !strcmp(op, "runtime_call") ? NULL : findfn(funcs, nf, a);
            int aggregate_call = indirect ? address_parameter(callee->ret) : !strcmp(op, "callagg");
            if (!callee) {
                const RuntimeRoutine *routine = find_runtime_routine(a);
                if (!routine) {
                    snprintf(error, es, "unknown function or invalid call '%s'", a);
                    return;
                }
                if (!strcmp(a, "scanf") || !strcmp(a, "fscanf")) {
                    unsigned fixed = !strcmp(a, "fscanf") ? 2 : 1;
                    fprintf(out,
                            "  lea %u(%%rsp), %%rdi\n  mov $%u, %%esi\n"
                            "%s"
                            "  mov %%rsp, %%r11\n  and $-16, %%rsp\n"
                            "  sub $16, %%rsp\n  mov %%r11, (%%rsp)\n"
                            "  call __crunch_%s\n  mov (%%rsp), %%rsp\n"
                            "  add $%u, %%rsp\n  push %%rax\n",
                            (argc - fixed) * 8, (argc - fixed) / 2,
                            fixed == 2 ? "  mov 8(%rdi), %rdx\n" : "", a, argc * 8);
                    continue;
                }
                if (!strcmp(a, "printf") || !strcmp(a, "fprintf") || !strcmp(a, "sprintf") || !strcmp(a, "snprintf")) {
                    unsigned fixed = !strcmp(a, "snprintf") ? 3 :
                                     (!strcmp(a, "fprintf") || !strcmp(a, "sprintf")) ? 2 : 1;
                    fprintf(out,
                            "  lea %u(%%rsp), %%rdi\n  mov $%u, %%esi\n"
                            "%s"
                            "  mov %%rsp, %%r11\n  and $-16, %%rsp\n"
                            "  sub $16, %%rsp\n  mov %%r11, (%%rsp)\n"
                            "  call __crunch_%s%s\n  mov (%%rsp), %%rsp\n"
                            "  add $%u, %%rsp\n  push %%rax\n",
                            (argc - fixed) * 8, (argc - fixed) / 2,
                            fixed == 2 ? "  mov 8(%rdi), %rdx\n" : "", a,
                            extended_fixed_precision_ir ? "_v42" :
                            version_36_ir ? "_v36" : extended_real_ir ? "_v34" :
                            precise_format_ir ? "_v28" : promoted_format_ir ? "_v26" :
                            unsigned_ir_types ? "_v6" : "", argc * 8);
                    continue;
                }
                if (argc != routine->argc) {
                    snprintf(error, es, "invalid argument count for runtime routine '%s'", a);
                    return;
                }
                emit_runtime_call(out, routine);
                continue;
            }
            if (callee->param_count != argc) {
                snprintf(error, es, "unknown function or invalid call '%s'", a);
                return;
            }
            for (unsigned j = argc; j > 0; j--)
                fprintf(out, "  pop %%rax\n  mov %%rax, -%u(%%rbp)\n", (fn->local_count + j) * 8);
            unsigned target_slot = (fn->local_count + fn->max_call_args + 4) * 8;
            if (indirect)
                fprintf(out, "  pop %%rax\n  mov %%rax, -%u(%%rbp)\n", target_slot);
            unsigned result_slot = (fn->local_count + fn->max_call_args + 2) * 8;
            if (aggregate_call)
                fprintf(out, "  pop %%rax\n  mov %%rax, -%u(%%rbp)\n", result_slot);
            unsigned gp = memory_aggregate(callee->ret) ? 1 : 0, fp = 0, stack_count = 0;
            unsigned char on_stack[130] = {0};
            unsigned stack_offsets[130] = {0};
            for (unsigned j = 0; j < argc; j++) {
                Ty t = callee->params[j].type;
                if (!parameter_registers(t, &gp, &fp)) {
                    on_stack[j] = 1;
                    stack_count = parameter_stack_offset(t, stack_count);
                    stack_offsets[j] = stack_count;
                    stack_count += parameter_stack_words(t);
                }
            }
            gp = memory_aggregate(callee->ret) ? 1 : 0;
            fp = 0;
            static const char *gpr[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
            static const char *xpr[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3",
                                        "%xmm4", "%xmm5", "%xmm6", "%xmm7"};
            if (memory_aggregate(callee->ret))
                fprintf(out, "  mov -%u(%%rbp), %%rdi\n", result_slot);
            for (unsigned j = 0; j < argc; j++) {
                Ty t = callee->params[j].type;
                unsigned off = (fn->local_count + j + 1) * 8;
                unsigned next_gp = gp, next_fp = fp;
                int in_registers = parameter_registers(t, &next_gp, &next_fp);
                if (t == T_REAL || t == T_FLOAT) {
                    if (in_registers)
                        fprintf(out, "  mov -%u(%%rbp), %%rax\n  movq %%rax, %s\n", off, xpr[fp++]);
                } else if (aggregate_param(t)) {
                    unsigned words = parameter_words(t);
                    if (in_registers) {
                        fprintf(out, "  mov -%u(%%rbp), %%r11\n", off);
                        for (unsigned k = 0; k < words; k++) {
                            if (parameter_sse_word(t, k))
                                fprintf(out, "  mov %u(%%r11), %%rax\n  movq %%rax, %s\n",
                                        k * 8, xpr[fp++]);
                            else
                                fprintf(out, "  mov %u(%%r11), %s\n", k * 8, gpr[gp++]);
                        }
                    }
                } else if (in_registers)
                    fprintf(out, "  mov -%u(%%rbp), %s\n", off, gpr[gp++]);
            }
            unsigned save = (fn->local_count + fn->max_call_args + 1) * 8;
            fprintf(out, "  mov %%rsp, %%rax\n  mov %%rax, -%u(%%rbp)\n  and $-16, %%rsp\n", save);
            if (stack_count % 2)
                fputs("  sub $8, %rsp\n", out);
            unsigned remaining = stack_count;
            for (unsigned n = argc; n > 0; n--) {
                unsigned j = n - 1;
                if (!on_stack[j])
                    continue;
                unsigned off = (fn->local_count + j + 1) * 8;
                Ty t = callee->params[j].type;
                unsigned words = parameter_stack_words(t);
                while (remaining > stack_offsets[j] + words) {
                    fputs("  sub $8, %rsp\n", out);
                    --remaining;
                }
                remaining -= words;
                if (memory_aggregate(t)) {
                    unsigned size = aggregate_size(t);
                    if (size <= 16) {
                        unsigned tail = size % 8;
                        fprintf(out, "  mov -%u(%%rbp), %%r11\n", off);
                        if (tail) {
                            fputs("  sub $8, %rsp\n", out);
                            for (unsigned k = 0; k < tail; ++k)
                                fprintf(out, "  mov %u(%%r11), %%al\n  mov %%al, %u(%%rsp)\n",
                                        size - tail + k, k);
                        }
                        for (unsigned k = size / 8; k > 0; --k)
                            fprintf(out, "  push %u(%%r11)\n", (k - 1) * 8);
                    } else {
                        unsigned id = (*labels)++;
                        fprintf(out, "  mov -%u(%%rbp), %%r11\n  add $%u, %%r11\n"
                                 "  mov $%u, %%r10\n.Laggregate_stack_%u:\n"
                                 "  sub $8, %%r11\n  push (%%r11)\n"
                                 "  dec %%r10\n  jne .Laggregate_stack_%u\n",
                            off, words * 8, words, id, id);
                    }
                } else if (aggregate_param(t) || t == T_EXT) {
                    fprintf(out, "  mov -%u(%%rbp), %%r11\n", off);
                    for (unsigned k = words; k > 0; k--)
                        fprintf(out, "  push %u(%%r11)\n", (k - 1) * 8);
                } else
                    fprintf(out, "  push -%u(%%rbp)\n", off);
            }
            if (indirect) {
                fprintf(out, "  mov $%u, %%eax\n", fp);
                fprintf(out, "  call *-%u(%%rbp)\n", target_slot);
            } else
                fprintf(out, "  call %s\n", a);
            fprintf(out, "  mov -%u(%%rbp), %%rsp\n", save);
            if (aggregate_call) {
                fprintf(out, "  mov -%u(%%rbp), %%r11\n", result_slot);
                if (callee->ret == T_EXT) {
                    fputs("  fstpt (%r11)\n", out);
                } else if (!memory_aggregate(callee->ret)) {
                    unsigned gp_return = 0, fp_return = 0;
                    static const char *gret[] = {"%rax", "%rdx"};
                    static const char *fret[] = {"%xmm0", "%xmm1"};
                    for (unsigned k = 0; k < parameter_words(callee->ret); k++) {
                        if (parameter_sse_word(callee->ret, k))
                            fprintf(out, "  movq %s, %%r10\n  mov %%r10, %u(%%r11)\n",
                                    fret[fp_return++], k * 8);
                        else
                            fprintf(out, "  mov %s, %u(%%r11)\n", gret[gp_return++], k * 8);
                    }
                }
                fputs("  push %r11\n", out);
                continue;
            } else if (callee->ret == T_REAL || callee->ret == T_FLOAT) {
                fputs("  movq %xmm0, %rax\n", out);
                normalize(out, callee->ret);
            } else if (callee->ret == T_VOID)
                fputs("  xor %eax, %eax\n", out);
            else if (callee->ret == T_UINT || callee->ret == T_SBYTE || callee->ret == T_UBYTE || callee->ret == T_SHORT || callee->ret == T_USHORT)
                normalize(out, callee->ret);
            fputs("  push %rax\n", out);
        } else if (extended_aggregate_ir_types && !strcmp(op, "retagg")) {
            fputs("  pop %r11\n", out);
            if (fn->ret == T_EXT) {
                fputs("  fldt (%r11)\n", out);
            } else if (memory_aggregate(fn->ret)) {
                fprintf(out, "  mov -%u(%%rbp), %%rdi\n  mov %%rdi, %%rax\n"
                             "  mov %%r11, %%rsi\n  mov $%u, %%rcx\n  rep movsb\n",
                        (fn->local_count + fn->max_call_args + 3) * 8,
                        aggregate_size(fn->ret));
            } else {
                unsigned gp_return = 0, fp_return = 0;
                static const char *gret[] = {"%rax", "%rdx"};
                static const char *fret[] = {"%xmm0", "%xmm1"};
                for (unsigned k = 0; k < parameter_words(fn->ret); k++) {
                    if (parameter_sse_word(fn->ret, k))
                        fprintf(out, "  mov %u(%%r11), %%r10\n  movq %%r10, %s\n",
                                k * 8, fret[fp_return++]);
                    else
                        fprintf(out, "  mov %u(%%r11), %s\n", k * 8, gret[gp_return++]);
                }
            }
            fputs("  leave\n  ret\n", out);
        } else if (!strcmp(op, "ret")) {
            Ty t = ty(a);
            fputs("  pop %rax\n", out);
            if (t == T_REAL || t == T_FLOAT)
                fputs("  movq %rax, %xmm0\n", out);
            else if (t == T_UINT || t == T_SBYTE || t == T_UBYTE || t == T_SHORT || t == T_USHORT)
                normalize(out, t);
            fputs("  leave\n  ret\n", out);
        } else if (version_36_ir && !strcmp(op, "ptrdiff")) {
            fputs("  pop %rcx\n  pop %rax\n  sub %rcx, %rax\n  push %rax\n", out);
        } else if (!strcmp(op, "ptradd") || !strcmp(op, "ptrsub")) {
            Ty et = ty(a);
            fputs("  pop %rcx\n  pop %rax\n", out);
            if (et == T_BYTE || et == T_SBYTE || et == T_UBYTE) {
            } else if (et == T_SHORT || et == T_USHORT)
                fputs("  shl $1, %rcx\n", out);
            else if (et == T_INT || et == T_UINT || et == T_FLOAT)
                fputs("  shl $2, %rcx\n", out);
            else
                fputs("  shl $3, %rcx\n", out);
            fprintf(out, "  %s %%rcx, %%rax\n  push %%rax\n",
                    !strcmp(op, "ptrsub") ? "sub" : "add");
        } else if (!strcmp(op, "ptradd_bytes")) {
            unsigned stride;
            if (!parse_storage_size(a, &stride) || !stride) {
                snprintf(error, es, "invalid structure-array element size");
                return;
            }
            fputs("  pop %rcx\n  pop %rax\n", out);
            fprintf(out, "  imul $%u, %%rcx\n  add %%rcx, %%rax\n  push %%rax\n", stride);
        } else if (!strcmp(op, "pow")) {
            fputs("  pop %rax\n  movq %rax, %xmm1\n"
                  "  pop %rax\n  movq %rax, %xmm0\n"
                  "  mov %rsp, %r11\n  and $-16, %rsp\n  sub $16, %rsp\n"
                  "  mov %r11, 8(%rsp)\n  call __crunch_pow\n  mov 8(%rsp), %rsp\n"
                  "  movq %xmm0, %rax\n  push %rax\n", out);
        } else if (!strcmp(op, "add") || !strcmp(op, "sub") || !strcmp(op, "mul") ||
                   !strcmp(op, "div") || !strcmp(op, "rem") || !strcmp(op, "band") ||
                   !strcmp(op, "bor") || !strcmp(op, "bxor") || !strcmp(op, "shl") ||
                   !strcmp(op, "shr") || !strcmp(op, "eq") || !strcmp(op, "ne") ||
                   !strcmp(op, "lt") || !strcmp(op, "le") || !strcmp(op, "gt") ||
                   !strcmp(op, "ge")) {
            emit_binary(out, op, ty(a), labels, error, es);
            if (error[0])
                return;
        } else {
            snprintf(error, es, "unknown Crunch IR instruction '%s'", op);
            return;
        }
    }
}
/**
 * @brief Translates a function and records instruction-level errors.
 * @param out Output FILE stream receiving assembly.
 * @param lines IR instruction lines to validate or translate.
 * @param fn Function metadata being inspected.
 * @param funcs Function metadata being inspected.
 * @param nf Number of functions in the function table.
 * @param globals Global symbol metadata for external references.
 * @param ng Number of global symbols.
 * @param lblocks Local stack-block metadata for the function.
 * @param laliases Local field-alias metadata for the function.
 * @param error Writable character buffer receiving output.
 * @param es Capacity of the diagnostic-message buffer.
 * @param labels Label table used to resolve branch targets.
 * @param free_name Assembly symbol used to release array storage.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_function(FILE *out, char **lines, const Function *fn, Function *funcs, size_t nf,
                          Global *globals, size_t ng, LocalBlock *lblocks, LocalAlias *laliases,
                          char *error, size_t es,
                          unsigned *labels, const char *free_name) {
    size_t error_line = 0;
    emit_function_impl(out, lines, fn, funcs, nf, globals, ng, lblocks, laliases, error, es, labels,
                       &error_line, free_name);
    add_ir_error_context(error, es, error_line, fn->name);
}
/**
 * @brief Emits the Linux process entry point that invokes Crunch main.
 * @param out Output FILE stream receiving assembly.
 * @param main_function Function metadata being inspected.
 * @return No value; the function performs the documented output or state update.
 */
static void emit_entry(FILE *out, const Function *main_function) {
    /* GNU ld needs the section to define its boundaries in PIEs without initializers. */
    fputs(".section __crunch_init_array,\"aw\",@progbits\n"
          ".weak __start___crunch_init_array\n.weak __stop___crunch_init_array\n"
          ".weak __crunch_run_finalizers\n"
          ".section .data\n.balign 8\n.Lcrunch_finalizer_ptr:\n"
          "  .quad __crunch_run_finalizers\n"
          ".section .text\n.global _start\n_start:\n"
          "  mov %rsp, %r12\n  and $-16, %rsp\n"
          "  lea __start___crunch_init_array(%rip), %r13\n"
          "  lea __stop___crunch_init_array(%rip), %r14\n"
          ".Lcrunch_start_init_loop:\n"
          "  cmp %r14, %r13\n  jae .Lcrunch_start_init_done\n"
          "  call *(%r13)\n  add $8, %r13\n  jmp .Lcrunch_start_init_loop\n"
          ".Lcrunch_start_init_done:\n", out);
    if (main_function->param_count) {
        fputs("  mov (%r12), %rdi\n", out);
        if (main_function->param_count > 1)
            fputs("  lea 8(%r12), %rsi\n", out);
        if (main_function->param_count > 2)
            fputs("  lea 8(%rsi,%rdi,8), %rdx\n", out);
    }
    fputs("  call main\n  mov %rax, %r12\n"
          "  movq .Lcrunch_finalizer_ptr(%rip), %rax\n"
          "  test %rax, %rax\n  jz .Lcrunch_start_exit\n"
          "  call *%rax\n.Lcrunch_start_exit:\n  mov %r12, %rdi\n"
          "  mov $60, %rax\n  syscall\n",
          out);
}
/**
 * @brief Parses and validates IR, optionally translating it to x86-64 assembly.
 * @param input Input FILE stream containing textual Crunch IR.
 * @param output Output FILE stream receiving generated assembly.
 * @param error Writable character buffer receiving output.
 * @param es Capacity of the diagnostic-message buffer.
 * @param validate_only Nonzero to validate without producing assembly.
 * @param source_filename Source filename for .file, or NULL to omit it.
 * @param emit_start Nonzero to emit the Crunch process entry point.
 * @return Zero on success; nonzero when output or translation fails.
 */
static int translate_ir(FILE *input, FILE *output, char *error, size_t es, int validate_only,
                        const char *source_filename, int emit_start) {
    FILE *out = tmpfile();
    char **lines = NULL;
    size_t n = 0, lines_capacity = 0;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    Function *funcs = NULL;
    size_t nf = 0, function_capacity = 0;
    Global globals[2048];
    size_t ng = 0;
    GlobalBytes initial_bytes[2048];
    size_t ninitial = 0;
    StringLiteral strings[4096];
    size_t ns = 0;
    GlobalBlock blocks[512];
    size_t nb = 0;
    BlockRelocation relocations[2048];
    size_t nr = 0;
    GlobalAlias aliases[2048];
    size_t na = 0;
    LocalBlock *lblocks = NULL;
    size_t nlb = 0, lblock_capacity = 0;
    LocalAlias *laliases = NULL;
    size_t nla = 0, lalias_capacity = 0;
    Function *cur = NULL;
    unsigned labels = 0;
    int module = 0, has_initializer = 0, supports_initializer = 0, supports_blockdata = 0;
    int abi_variadic_ir = 0, tls_ir = 0;
    int parameter_prologue = 1;
    int supports_blockreloc = 0;
    char initializer[128] = {0};
    if (error && es)
        error[0] = 0;
    unsigned_ir_types = 0;
    float_ir_types = 0;
    short_ir_types = 0;
    signed_byte_ir_types = 0;
    aggregate_ir_types = sse_aggregate_ir_types = extended_aggregate_ir_types =
        function_pointer_ir_types = 0;
    promoted_format_ir = 0;
    precise_format_ir = 0;
    variadic_frame_ir = 0;
    variadic_aggregate_ir = 0;
    extended_real_ir = 0;
    aligned_globals_ir = 0;
    dynamic_stack_ir = 0;
    stackalloc_live_ir = 0;
    aligned_strings_ir = 0;
    version_36_ir = 0;
    pointer_integer_cast_ir = 0;
    flexible_aggregate_ir = 0;
    aligned_sse_aggregate_ir = 0;
    small_memory_aggregate_ir = 0;
    extended_fixed_precision_ir = 0;
    large_runtime_call_ir = 0;
    if (!out) {
        if (error && es)
            snprintf(error, es, "cannot create temporary assembly stream");
        return 1;
    }
    for (;;) {
        errno = 0;
        len = getline(&line, &cap, input);
        if (len < 0)
            break;
        (void)len;
        if (n == 131072) {
            snprintf(error, es, "IR file is too large");
            goto bad;
        }
        if (n == lines_capacity) {
            size_t next = lines_capacity ? lines_capacity * 2 : 256;
            char **grown = realloc(lines, next * sizeof(*lines));
            if (!grown) {
                snprintf(error, es, "out of memory");
                goto bad;
            }
            lines = grown;
            lines_capacity = next;
        }
        lines[n] = strdup(line);
        if (!lines[n]) {
            snprintf(error, es, "out of memory");
            goto bad;
        }
        n++;
    }
    if (ferror(input) || !feof(input)) {
        snprintf(error, es, "%s", errno == ENOMEM ? "out of memory reading IR"
                                                  : "failed to read IR");
        goto bad;
    }
    free(line);
    line = NULL;
    if (n && (!strcmp(lines[0], "CRUNCH-IR 61\n") ||
              !strcmp(lines[0], "CRUNCH-IR 60\n") ||
              !strcmp(lines[0], "CRUNCH-IR 59\n") ||
              !strcmp(lines[0], "CRUNCH-IR 58\n") ||
              !strcmp(lines[0], "CRUNCH-IR 57\n") ||
              !strcmp(lines[0], "CRUNCH-IR 56\n") ||
              !strcmp(lines[0], "CRUNCH-IR 55\n") ||
              !strcmp(lines[0], "CRUNCH-IR 54\n") ||
              !strcmp(lines[0], "CRUNCH-IR 53\n") ||
              !strcmp(lines[0], "CRUNCH-IR 50\n") ||
              !strcmp(lines[0], "CRUNCH-IR 51\n"))) {
        dynamic_stack_ir = aligned_globals_ir = aligned_sse_aggregate_ir = flexible_aggregate_ir = 1;
        aligned_strings_ir = !strcmp(lines[0], "CRUNCH-IR 60\n") ||
                             !strcmp(lines[0], "CRUNCH-IR 61\n");
        stackalloc_live_ir = aligned_strings_ir || !strcmp(lines[0], "CRUNCH-IR 58\n") ||
                             !strcmp(lines[0], "CRUNCH-IR 59\n");
        pointer_integer_cast_ir = stackalloc_live_ir || !strcmp(lines[0], "CRUNCH-IR 56\n") ||
                                  !strcmp(lines[0], "CRUNCH-IR 57\n");
        small_memory_aggregate_ir = pointer_integer_cast_ir ||
                                    !strcmp(lines[0], "CRUNCH-IR 54\n") ||
                                    !strcmp(lines[0], "CRUNCH-IR 55\n");
        tls_ir = !strcmp(lines[0], "CRUNCH-IR 61\n") || !strcmp(lines[0], "CRUNCH-IR 59\n") ||
                 !strcmp(lines[0], "CRUNCH-IR 53\n") || !strcmp(lines[0], "CRUNCH-IR 55\n") ||
                 !strcmp(lines[0], "CRUNCH-IR 57\n");
        module = tls_ir || !strcmp(lines[0], "CRUNCH-IR 51\n");
        abi_variadic_ir = module;
        large_runtime_call_ir = extended_fixed_precision_ir = version_36_ir =
            extended_real_ir = variadic_aggregate_ir = variadic_frame_ir =
            precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 48\n") ||
              !strcmp(lines[0], "CRUNCH-IR 49\n"))) {
        aligned_globals_ir = aligned_sse_aggregate_ir = flexible_aggregate_ir = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 49\n");
        abi_variadic_ir = module;
        large_runtime_call_ir = extended_fixed_precision_ir = version_36_ir =
            extended_real_ir = variadic_aggregate_ir = variadic_frame_ir =
            precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 46\n") ||
              !strcmp(lines[0], "CRUNCH-IR 47\n"))) {
        aligned_sse_aggregate_ir = flexible_aggregate_ir = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 47\n");
        abi_variadic_ir = module;
        large_runtime_call_ir = extended_fixed_precision_ir = version_36_ir =
            extended_real_ir = variadic_aggregate_ir = variadic_frame_ir =
            precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 44\n") ||
              !strcmp(lines[0], "CRUNCH-IR 45\n"))) {
        flexible_aggregate_ir = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 45\n");
        abi_variadic_ir = module;
        large_runtime_call_ir = extended_fixed_precision_ir = version_36_ir =
            extended_real_ir = variadic_aggregate_ir = variadic_frame_ir =
            precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 42\n") ||
              !strcmp(lines[0], "CRUNCH-IR 43\n"))) {
        module = !strcmp(lines[0], "CRUNCH-IR 43\n");
        abi_variadic_ir = module;
        large_runtime_call_ir = extended_fixed_precision_ir = version_36_ir =
            extended_real_ir = variadic_aggregate_ir = variadic_frame_ir =
            precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 39\n") ||
              !strcmp(lines[0], "CRUNCH-IR 41\n"))) {
        abi_variadic_ir = !strcmp(lines[0], "CRUNCH-IR 41\n");
        large_runtime_call_ir = version_36_ir = extended_real_ir = variadic_aggregate_ir =
            variadic_frame_ir = precise_format_ir = promoted_format_ir =
            function_pointer_ir_types = extended_aggregate_ir_types = sse_aggregate_ir_types =
            aggregate_ir_types = signed_byte_ir_types = short_ir_types = float_ir_types =
            unsigned_ir_types = supports_initializer = supports_blockdata = supports_blockreloc = 1;
        module = 1;
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 37\n") ||
              !strcmp(lines[0], "CRUNCH-IR 36\n"))) {
        version_36_ir = extended_real_ir = variadic_aggregate_ir = variadic_frame_ir =
            precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 37\n");
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 35\n") ||
              !strcmp(lines[0], "CRUNCH-IR 34\n"))) {
        extended_real_ir = variadic_aggregate_ir = variadic_frame_ir = precise_format_ir =
            promoted_format_ir = function_pointer_ir_types = extended_aggregate_ir_types =
            sse_aggregate_ir_types = aggregate_ir_types = signed_byte_ir_types =
            short_ir_types = float_ir_types = unsigned_ir_types = supports_initializer =
            supports_blockdata = supports_blockreloc = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 35\n");
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 33\n") ||
              !strcmp(lines[0], "CRUNCH-IR 32\n"))) {
        variadic_aggregate_ir = variadic_frame_ir = precise_format_ir = promoted_format_ir =
            function_pointer_ir_types = extended_aggregate_ir_types = sse_aggregate_ir_types =
            aggregate_ir_types = signed_byte_ir_types = short_ir_types = float_ir_types =
            unsigned_ir_types = supports_initializer = supports_blockdata = supports_blockreloc = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 33\n");
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 31\n") ||
              !strcmp(lines[0], "CRUNCH-IR 30\n"))) {
        variadic_frame_ir = precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 31\n");
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 29\n") ||
              !strcmp(lines[0], "CRUNCH-IR 28\n"))) {
        precise_format_ir = promoted_format_ir = function_pointer_ir_types =
            extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 29\n");
    } else if (n && (!strcmp(lines[0], "CRUNCH-IR 27\n") ||
              !strcmp(lines[0], "CRUNCH-IR 26\n"))) {
        promoted_format_ir = function_pointer_ir_types = extended_aggregate_ir_types =
            sse_aggregate_ir_types = aggregate_ir_types = signed_byte_ir_types =
            short_ir_types = float_ir_types = unsigned_ir_types = supports_initializer =
            supports_blockdata = supports_blockreloc = 1;
        module = !strcmp(lines[0], "CRUNCH-IR 27\n");
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 25\n")) {
        function_pointer_ir_types = extended_aggregate_ir_types = sse_aggregate_ir_types =
            aggregate_ir_types = signed_byte_ir_types = short_ir_types = float_ir_types =
            unsigned_ir_types = module = supports_initializer = supports_blockdata =
            supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 24\n")) {
        function_pointer_ir_types = extended_aggregate_ir_types = sse_aggregate_ir_types =
            aggregate_ir_types = signed_byte_ir_types = short_ir_types = float_ir_types =
            unsigned_ir_types = supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 23\n")) {
        extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types = module =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 22\n")) {
        extended_aggregate_ir_types = sse_aggregate_ir_types = aggregate_ir_types =
            signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 21\n")) {
        sse_aggregate_ir_types = aggregate_ir_types = signed_byte_ir_types = short_ir_types =
            float_ir_types = unsigned_ir_types = module = supports_initializer =
            supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 20\n")) {
        sse_aggregate_ir_types = aggregate_ir_types = signed_byte_ir_types = short_ir_types =
            float_ir_types = unsigned_ir_types = supports_initializer = supports_blockdata =
            supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 19\n")) {
        aggregate_ir_types = signed_byte_ir_types = short_ir_types = float_ir_types =
            unsigned_ir_types = module = supports_initializer = supports_blockdata =
            supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 18\n")) {
        aggregate_ir_types = signed_byte_ir_types = short_ir_types = float_ir_types =
            unsigned_ir_types = supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 17\n")) {
        signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types = module =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 16\n")) {
        signed_byte_ir_types = short_ir_types = float_ir_types = unsigned_ir_types =
            supports_initializer = supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 15\n")) {
        short_ir_types = float_ir_types = unsigned_ir_types = module = supports_initializer =
            supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 14\n")) {
        short_ir_types = float_ir_types = unsigned_ir_types = supports_initializer =
            supports_blockdata = supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 13\n")) {
        float_ir_types = unsigned_ir_types = module = supports_initializer = supports_blockdata =
            supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 12\n")) {
        float_ir_types = unsigned_ir_types = supports_initializer = supports_blockdata =
            supports_blockreloc = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 11\n")) {
        float_ir_types = unsigned_ir_types = module = supports_initializer = supports_blockdata = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 10\n")) {
        float_ir_types = unsigned_ir_types = supports_initializer = supports_blockdata = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 9\n")) {
        float_ir_types = unsigned_ir_types = module = supports_initializer = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 8\n")) {
        float_ir_types = unsigned_ir_types = supports_initializer = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 7\n")) {
        unsigned_ir_types = module = supports_initializer = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 6\n")) {
        unsigned_ir_types = supports_initializer = 1;
    } else if (n && !strcmp(lines[0], "CRUNCH-IR 5\n"))
        supports_initializer = has_initializer = 1;
    else if (n && !strcmp(lines[0], "CRUNCH-IR 4\n"))
        module = supports_initializer = has_initializer = 1;
    else if (n && !strcmp(lines[0], "CRUNCH-IR 3\n"))
        module = 1;
    else if (n < 2 || strcmp(lines[0], "CRUNCH-IR 2\n")) {
        snprintf(error, es, "expected Crunch IR v2 through v37, v39, v41 through v51, or v53");
        goto bad;
    }
    for (size_t i = 1; i < n; ++i) {
        if (!signed_byte_ir_types && (has_type_operand(lines[i], "sbyte") ||
                                      has_type_operand(lines[i], "ubyte"))) {
            backend_error(error, es, i + 1, NULL, "signed byte IR requires version 16 or 17");
            goto bad;
        }
        if (!short_ir_types && (has_type_operand(lines[i], "short") ||
                                has_type_operand(lines[i], "ushort"))) {
            backend_error(error, es, i + 1, NULL, "short IR requires version 14 or 15");
            goto bad;
        }
        if (!unsigned_ir_types && legacy_unsigned_syntax(lines[i])) {
            backend_error(error, es, i + 1, NULL, "unsigned IR requires version 6 or 7");
            goto bad;
        }
        char op[32] = {0}, number[128] = {0}, type[32] = {0}, extra[2] = {0};
        int fields = scan_ir(lines[i], "%31s %127s %31s %1s", op, number, type, extra);
        if (!float_ir_types && (!strcmp(op, "pushf") || has_type_operand(lines[i], "float"))) {
            backend_error(error, es, i + 1, NULL, "float IR requires version 8 or 9");
            goto bad;
        }
        if (float_ir_types && !strcmp(op, "pushf") &&
            (fields != 2 || !valid_float_bits(number))) {
            backend_error(error, es, i + 1, NULL, "invalid binary32 literal");
            goto bad;
        }
        if (float_ir_types && has_type_operand(lines[i], "float")) {
            char a[128], b[128], c[128], d[128], trailing[2];
            int count = scan_ir(lines[i], "%*s %127s %127s %127s %127s %1s",
                               a, b, c, d, trailing);
            int expected = !strcmp(op, "globalalias") || !strcmp(op, "localalias") ? 4 :
                           !strcmp(op, "func") || !strcmp(op, "param") ||
                           !strcmp(op, "global") ? 3 :
                           !strcmp(op, "extern_global") || !strcmp(op, "local") ||
                           !strcmp(op, "store") || !strcmp(op, "gload") ||
                           !strcmp(op, "gstore") || !strcmp(op, "lload") ||
                           !strcmp(op, "lstore") || !strcmp(op, "cast") ? 2 : 1;
            if (count != expected) {
                backend_error(error, es, i + 1, NULL, "invalid float instruction operands");
                goto bad;
            }
        }
        if (float_ir_types && !strcmp(op, "global") && !strcmp(type, "float")) {
            char value[128] = {0}, trailing[2] = {0};
            if (scan_ir(lines[i], "%*s %*s %*s %127s %1s", value, trailing) != 1 ||
                !valid_float_bits(value)) {
                backend_error(error, es, i + 1, NULL, "invalid binary32 global initializer");
                goto bad;
            }
        }
        if (float_ir_types && has_type_operand(lines[i], "float") &&
            (!strcmp(op, "bitnot") || !strcmp(op, "rem") || !strcmp(op, "band") ||
             !strcmp(op, "bor") || !strcmp(op, "bxor") || !strcmp(op, "shl") ||
             !strcmp(op, "shr"))) {
            backend_error(error, es, i + 1, NULL, "integer operation is not valid for float");
            goto bad;
        }
        if (!strcmp(op, "pushu")) {
            uint64_t value;
            Ty t = ty(type);
            if (fields != 3 || (t != T_UINT && t != T_ULONG) ||
                !parse_uint_literal(number, t == T_UINT ? UINT32_MAX : UINT64_MAX, &value)) {
                backend_error(error, es, i + 1, NULL, "invalid unsigned integer literal");
                goto bad;
            }
        }
    }
    {
        int in_function = 0;
        for (size_t i = 1; i < n; i++) {
            char op[32] = {0};
            if (scan_ir(lines[i], "%31s", op) != 1)
                continue;
            if (in_function) {
                if (!strcmp(op, "end"))
                    in_function = 0;
                continue;
            }
            if (!strcmp(op, "func")) {
                in_function = 1;
                continue;
            }
            if (module && !strcmp(op, "extern_func"))
                continue;
            if (abi_variadic_ir && !strcmp(op, "abi_variadic"))
                continue;
            if (large_runtime_call_ir && !strcmp(op, "require_user_runtime")) {
                char name[128] = {0}, extra[2] = {0};
                if (scan_ir(lines[i], "%*s %127s %1s", name, extra) != 1 ||
                    !crunch_is_runtime_name(name)) {
                    backend_error(error, es, i + 1, NULL, "invalid runtime guard directive");
                    goto bad;
                }
                for (size_t j = 1; j < i; ++j) {
                    char prior_op[32] = {0}, prior_name[128] = {0};
                    if (scan_ir(lines[j], "%31s %127s", prior_op, prior_name) == 2 &&
                        !strcmp(prior_op, op) && !strcmp(prior_name, name)) {
                        backend_error(error, es, i + 1, NULL, "duplicate runtime guard directive");
                        goto bad;
                    }
                }
                continue;
            }
            if (supports_blockdata && !strcmp(op, "blockdata"))
                continue;
            if (supports_blockreloc && !strcmp(op, "blockreloc"))
                continue;
            if (supports_blockreloc && !strcmp(op, "globalreloc"))
                continue;
            if (tls_ir && !strcmp(op, "thread_local"))
                continue;
            if (supports_initializer && !strcmp(op, "initializer")) {
                char name[128] = {0}, extra[2] = {0};
                if (initializer[0] || scan_ir(lines[i], "%*s %127s %1s", name, extra) != 1 ||
                    !valid_asm_symbol(name)) {
                    backend_error(error, es, i + 1, NULL, "invalid initializer directive");
                    goto bad;
                }
                memcpy(initializer, name, strlen(name) + 1);
                has_initializer = 1;
                continue;
            }
            if (strcmp(op, "internal") && strcmp(op, "globalbytes") && strcmp(op, "global") && strcmp(op, "extern_global") && strcmp(op, "globalblock") && strcmp(op, "globalalias") &&
                strcmp(op, "stringlit")) {
                backend_error(error, es, i + 1, NULL,
                              "unknown IR directive '%s' outside a function", op);
                goto bad;
            }
        }
    }
    for (size_t i = 1; i < n; i++) {
        char op[32] = {0};
        if (scan_ir(lines[i], "%31s", op) == 1 && !strcmp(op, "stringlit")) {
            char *copy = strdup(lines[i]);
            if (!copy || ns == 4096) {
                free(copy);
                snprintf(error, es, "too many string literals or out of memory");
                goto bad;
            }
            char *save = NULL;
            strtok_r(copy, " \t\r\n", &save);
            char *label = strtok_r(NULL, " \t\r\n", &save);
            char *hex = strtok_r(NULL, " \t\r\n", &save);
            char *alignment = strtok_r(NULL, " \t\r\n", &save);
            char *extra = strtok_r(NULL, " \t\r\n", &save);
            if (!label || !hex || extra ||
                (alignment && (!aligned_strings_ir || strcmp(alignment, "4"))) ||
                !valid_asm_symbol(label) || strlen(hex) % 2) {
                free(copy);
                snprintf(error, es, "invalid string literal directive");
                goto bad;
            }
            size_t string_index = ns++;
            strings[string_index].name = strdup(label);
            strings[string_index].hex = strdup(hex);
            strings[string_index].alignment = alignment ? 4 : 1;
            free(copy);
            if (!strings[string_index].name || !strings[string_index].hex) {
                snprintf(error, es, "out of memory storing string literal");
                goto bad;
            }
            for (const char *q = strings[string_index].hex; *q; q++)
                if (!((*q >= '0' && *q <= '9') || (*q >= 'a' && *q <= 'f') ||
                      (*q >= 'A' && *q <= 'F'))) {
                    snprintf(error, es, "invalid hexadecimal string literal");
                    goto bad;
                }
        }
    }
    for (size_t i = 1; i < n; i++) {
        char op[32] = {0}, name[128] = {0}, a[128] = {0}, b[128] = {0};
        if (scan_ir(lines[i], "%31s %127s %127s %127s", op, name, a, b) < 1)
            continue;
        if (!strcmp(op, "global") && !cur) {
            if (ng == 2048) {
                backend_error(error, es, i + 1, NULL, "too many global variables");
                goto bad;
            }
            Global *g = &globals[ng++];
            memset(g, 0, sizeof(*g));
            g->initial_bytes = SIZE_MAX;
            g->name = strdup(name);
            g->type = ty(a);
            g->value = strdup(b);
            if (!g->name || !g->value) {
                backend_error(error, es, i + 1, NULL, "out of memory storing global '%s'", name);
                goto bad;
            }
            if (g->type == T_BAD || g->type == T_VOID) {
                backend_error(error, es, i + 1, NULL, "invalid global declaration for '%s'", name);
                goto bad;
            }
        } else if (!strcmp(op, "globalblock") && !cur) {
            unsigned z, al;
            if (nb == 512 || !parse_storage_size(a, &z) || !parse_u(b, &al) || !z || !al) {
                backend_error(error, es, i + 1, NULL, "invalid global structure block '%s'", name);
                goto bad;
            }
            blocks[nb].name = strdup(name);
            blocks[nb].size = z;
            blocks[nb].align = al;
            blocks[nb].internal = 0;
            blocks[nb].hex = NULL;
            blocks[nb].hex_bytes = 0;
            if (!blocks[nb].name) {
                backend_error(error, es, i + 1, NULL, "out of memory storing global block '%s'",
                              name);
                goto bad;
            }
            nb++;
        } else if (!strcmp(op, "globalalias") && !cur) {
            char block[128] = {0}, offstr[32] = {0}, typestr[32] = {0};
            if (na == 2048 ||
                scan_ir(lines[i], "%*s %127s %127s %31s %31s", name, block, offstr, typestr) != 4) {
                backend_error(error, es, i + 1, NULL, "invalid global alias");
                goto bad;
            }
            unsigned off;
            if (!parse_u(offstr, &off) || ty(typestr) == T_BAD) {
                backend_error(error, es, i + 1, NULL, "invalid fields for global alias '%s'", name);
                goto bad;
            }
            aliases[na].name = strdup(name);
            aliases[na].block = strdup(block);
            aliases[na].offset = off;
            aliases[na].type = ty(typestr);
            if (!aliases[na].name || !aliases[na].block) {
                free(aliases[na].name);
                free(aliases[na].block);
                backend_error(error, es, i + 1, NULL, "out of memory storing global alias '%s'",
                              name);
                goto bad;
            }
            na++;
        } else if (!strcmp(op, "func")) {
            if (cur) {
                backend_error(error, es, i + 1, cur->name, "nested function declaration");
                goto bad;
            }
            if (findfn(funcs, nf, name)) {
                backend_error(error, es, i + 1, NULL, "duplicate function name '%s'", name);
                goto bad;
            }
            unsigned pc;
            if (!parse_u(a, &pc) || pc > 128) {
                backend_error(error, es, i + 1, NULL, "invalid parameter count for function '%s'",
                              name);
                goto bad;
            }
            if (!reserve_function(&funcs, nf, &function_capacity)) {
                backend_error(error, es, i + 1, NULL, "out of memory storing functions");
                goto bad;
            }
            cur = &funcs[nf++];
            parameter_prologue = 1;
            memset(cur, 0, sizeof(*cur));
            cur->name = strdup(name);
            cur->begin = i + 1;
            cur->param_count = pc;
            cur->ret = return_ty(b);
            cur->local_block_start = nlb;
            cur->local_alias_start = nla;
            if (!cur->name) {
                backend_error(error, es, i + 1, NULL, "out of memory storing function '%s'", name);
                goto bad;
            }
            if (cur->ret == T_BAD) {
                backend_error(error, es, i + 1, NULL, "invalid function header for '%s'", name);
                goto bad;
            }
        } else if (!strcmp(op, "end")) {
            if (!cur) {
                backend_error(error, es, i + 1, NULL, "'end' outside function");
                goto bad;
            }
            cur->end = i;
            cur = NULL;
        } else if (cur) {
            unsigned slot, index;
            if (aggregate_ir_types && !strcmp(op, "param") && !parameter_prologue) {
                backend_error(error, es, i + 1, cur->name,
                              "parameter must precede executable instructions");
                goto bad;
            }
            if (strcmp(op, "param") && strcmp(op, "local") && strcmp(op, "localblock") &&
                !(variadic_frame_ir && !strcmp(op, "variadic")) &&
                strcmp(op, "localalias") && strcmp(op, "stringlit"))
                parameter_prologue = 0;
            if (variadic_frame_ir && !strcmp(op, "variadic")) {
                if (!parameter_prologue || cur->variadic || !cur->param_count || name[0]) {
                    backend_error(error, es, i + 1, cur->name, "invalid variadic directive");
                    goto bad;
                }
                cur->variadic = 1;
            } else if (!strcmp(op, "localblock")) {
                unsigned z, al;
                if (!parse_local_block_size(a, &z) || !parse_u(b, &al) || !z || !al ||
                    (extended_real_ir && (al > 16 || (al & (al - 1))))) {
                    backend_error(error, es, i + 1, cur->name, "invalid local structure block '%s'",
                                  name);
                    goto bad;
                }
                if (nlb == lblock_capacity) {
                    size_t next = lblock_capacity ? lblock_capacity * 2 : 128;
                    if (next < lblock_capacity || next > SIZE_MAX / sizeof(*lblocks)) {
                        backend_error(error, es, i + 1, cur->name, "too many local blocks");
                        goto bad;
                    }
                    LocalBlock *grown = realloc(lblocks, next * sizeof(*lblocks));
                    if (!grown) {
                        backend_error(error, es, i + 1, cur->name, "out of memory storing local blocks");
                        goto bad;
                    }
                    lblocks = grown;
                    lblock_capacity = next;
                }
                size_t end = cur->local_block_bytes + z;
                size_t rem = end % al;
                if (rem)
                    end += al - rem;
                if (end > 16777216UL) {
                    backend_error(error, es, i + 1, cur->name, "local structure blocks are too large");
                    goto bad;
                }
                size_t off = end - z;
                lblocks[nlb] = (LocalBlock){strdup(name), z, al, off, (unsigned)(nf - 1)};
                if (!lblocks[nlb].name) {
                    backend_error(error, es, i + 1, cur->name,
                                  "out of memory storing local block '%s'", name);
                    goto bad;
                }
                nlb++;
                cur->local_block_count++;
                cur->local_block_bytes = end;
            } else if (!strcmp(op, "localalias")) {
                char block[128] = {0}, offstr[32] = {0}, typestr[32] = {0};
                if (scan_ir(lines[i], "%*s %127s %127s %31s %31s", name, block,
                                          offstr, typestr) != 4) {
                    backend_error(error, es, i + 1, cur->name, "invalid local structure alias");
                    goto bad;
                }
                unsigned off;
                if (!parse_u(offstr, &off) || ty(typestr) == T_BAD) {
                    backend_error(error, es, i + 1, cur->name,
                                  "invalid fields for local alias '%s'", name);
                    goto bad;
                }
                if (nla == lalias_capacity) {
                    size_t next = lalias_capacity ? lalias_capacity * 2 : 128;
                    if (next < lalias_capacity || next > SIZE_MAX / sizeof(*laliases)) {
                        backend_error(error, es, i + 1, cur->name, "too many local aliases");
                        goto bad;
                    }
                    LocalAlias *grown = realloc(laliases, next * sizeof(*laliases));
                    if (!grown) {
                        backend_error(error, es, i + 1, cur->name, "out of memory storing local aliases");
                        goto bad;
                    }
                    laliases = grown;
                    lalias_capacity = next;
                }
                laliases[nla] =
                    (LocalAlias){strdup(name), strdup(block), off, ty(typestr), (unsigned)(nf - 1)};
                if (!laliases[nla].name || !laliases[nla].block) {
                    free(laliases[nla].name);
                    free(laliases[nla].block);
                    backend_error(error, es, i + 1, cur->name,
                                  "out of memory storing local alias '%s'", name);
                    goto bad;
                }
                nla++;
                cur->local_alias_count++;
            } else if (!strcmp(op, "local") && parse_u(name, &slot)) {
                for (unsigned j = 0; j < cur->param_count; j++)
                    if (address_parameter(cur->params[j].type) &&
                        slot >= cur->params[j].slot &&
                        slot < cur->params[j].slot + parameter_words(cur->params[j].type)) {
                        backend_error(error, es, i + 1, cur->name, "local overlaps aggregate parameter");
                        goto bad;
                    }
                if (slot >= CRUNCH_LOCAL_LIMIT ||
                    (float_ir_types && (ty(a) == T_BAD || ty(a) == T_VOID ||
                    (cur->local_types[slot] != T_BAD && cur->local_types[slot] != ty(a))))) {
                    backend_error(error, es, i + 1, cur->name, "invalid or conflicting local type");
                    goto bad;
                }
                if (slot + 1 > cur->local_count)
                    cur->local_count = slot + 1;
                if (slot < CRUNCH_LOCAL_LIMIT && a[0])
                    cur->local_types[slot] = ty(a);
            } else if (!strcmp(op, "param")) {
                if (!parse_u(name, &index) || !parse_u(a, &slot) || index >= cur->param_count) {
                    backend_error(error, es, i + 1, cur->name, "invalid param instruction");
                    goto bad;
                }
                Ty pt = parameter_ty(b);
                unsigned words = parameter_words(pt);
                if ((module || float_ir_types) &&
                    (slot > CRUNCH_LOCAL_LIMIT - words || cur->params[index].type != T_BAD ||
                     pt == T_BAD || pt == T_VOID ||
                     (float_ir_types && cur->local_types[slot] != T_BAD &&
                      (address_parameter(pt) || cur->local_types[slot] != pt)) ||
                     (words == 2 && cur->local_types[slot + 1] != T_BAD))) {
                    backend_error(error, es, i + 1, cur->name, "invalid or duplicate parameter");
                    goto bad;
                }
                cur->params[index] = (Param){slot, pt};
                for (unsigned j = 0; j < words && slot + j < CRUNCH_LOCAL_LIMIT; j++)
                    cur->local_types[slot + j] = (memory_aggregate(pt) || pt == T_EXT) ? T_PTR
                                                    : aggregate_param(pt) ? T_LONG : pt;
                if (slot + words > cur->local_count)
                    cur->local_count = slot + words;
            } else if (!strcmp(op, "call") ||
                       (function_pointer_ir_types && !strcmp(op, "callptr")) ||
                       (extended_aggregate_ir_types && !strcmp(op, "callagg")) ||
                       (module && !strcmp(op, "runtime_call"))) {
                unsigned argc;
                if (!parse_u(!strcmp(op, "callptr") ? name : a, &argc)) {
                    backend_error(error, es, i + 1, cur->name, "invalid call instruction");
                    goto bad;
                }
                if (argc > cur->max_call_args)
                    cur->max_call_args = argc;
            }
        }
    }
    if (cur) {
        snprintf(error, es, "unterminated function '%s' (began before IR line %zu)", cur->name,
                 cur->begin);
        goto bad;
    }
    if (supports_blockdata) {
        int in_function = 0;
        for (size_t i = 1; i < n; ++i) {
            char op[32] = {0};
            if (scan_ir(lines[i], "%31s", op) != 1)
                continue;
            if (!strcmp(op, "func")) {
                in_function = 1;
                continue;
            }
            if (!strcmp(op, "end")) {
                in_function = 0;
                continue;
            }
            if (in_function || strcmp(op, "blockdata"))
                continue;
            char *copy = strdup(lines[i]);
            if (!copy) {
                backend_error(error, es, i + 1, NULL, "out of memory storing block data");
                goto bad;
            }
            char *save = NULL;
            strtok_r(copy, " \t\r\n", &save);
            char *name = strtok_r(NULL, " \t\r\n", &save);
            char *hex = strtok_r(NULL, " \t\r\n", &save);
            char *extra = strtok_r(NULL, " \t\r\n", &save);
            GlobalBlock *block = NULL;
            for (size_t j = 0; name && j < nb; ++j)
                if (!strcmp(blocks[j].name, name)) {
                    if (block) {
                        block = NULL;
                        break;
                    }
                    block = &blocks[j];
                }
            size_t length = hex ? strlen(hex) : 0;
            int valid = block && !block->hex && hex && !extra && length &&
                        !(length % 2) && length / 2 <= block->size;
            for (size_t j = 0; valid && j < length; ++j)
                if (!((hex[j] >= '0' && hex[j] <= '9') ||
                      (hex[j] >= 'a' && hex[j] <= 'f') ||
                      (hex[j] >= 'A' && hex[j] <= 'F')))
                    valid = 0;
            if (!valid) {
                free(copy);
                backend_error(error, es, i + 1, NULL, "invalid initialized block data");
                goto bad;
            }
            block->hex = strdup(hex);
            free(copy);
            if (!block->hex) {
                backend_error(error, es, i + 1, NULL, "out of memory storing block data");
                goto bad;
            }
            block->hex_bytes = length / 2;
        }
    }
    {
        int in_function = 0;
        for (size_t i = 1; i < n; i++) {
            char op[32] = {0}, name[128] = {0}, type[32] = {0}, extra[2] = {0};
            int fields = scan_ir(lines[i], "%31s %127s %31s %1s", op, name, type, extra);
            if (fields < 1)
                continue;
            if (!strcmp(op, "func")) {
                in_function = 1;
                continue;
            }
            if (!strcmp(op, "end")) {
                in_function = 0;
                continue;
            }
            if (in_function || strcmp(op, "extern_global"))
                continue;
            Ty t = ty(type);
            if (fields != 3 || !valid_external_global_symbol(name, t) ||
                t == T_BAD || t == T_VOID) {
                backend_error(error, es, i + 1, NULL, "invalid external global declaration");
                goto bad;
            }
            Global *g = NULL;
            for (size_t j = 0; j < ng; j++)
                if (!strcmp(globals[j].name, name)) {
                    if (g || globals[j].type != t) {
                        backend_error(error, es, i + 1, NULL,
                                      "conflicting external global '%s'", name);
                        goto bad;
                    }
                    g = &globals[j];
                }
            if (!g) {
                if (ng == 2048) {
                    backend_error(error, es, i + 1, NULL, "too many global variables");
                    goto bad;
                }
                g = &globals[ng++];
                memset(g, 0, sizeof(*g));
                g->name = strdup(name);
                if (!g->name) {
                    backend_error(error, es, i + 1, NULL, "out of memory storing external global");
                    goto bad;
                }
                g->type = t;
                g->external = 1;
                g->initial_bytes = SIZE_MAX;
            }
            g->declared_extern = 1;
        }
    }
    if (module || float_ir_types) {
        for (size_t i = 0; i < nf; i++) {
            if (!valid_asm_symbol(funcs[i].name)) {
                snprintf(error, es, "invalid function symbol '%s'", funcs[i].name);
                goto bad;
            }
            for (size_t j = 0; j < funcs[i].param_count; j++) {
                if (funcs[i].params[j].type == T_BAD || funcs[i].params[j].type == T_VOID) {
                    snprintf(error, es, "missing or invalid parameter for function '%s'",
                             funcs[i].name);
                    goto bad;
                }
                for (size_t k = 0; k < j; k++)
                    if (funcs[i].params[j].slot <
                            funcs[i].params[k].slot + parameter_words(funcs[i].params[k].type) &&
                        funcs[i].params[k].slot <
                            funcs[i].params[j].slot + parameter_words(funcs[i].params[j].type)) {
                        snprintf(error, es, "duplicate parameter slot in function '%s'",
                                 funcs[i].name);
                        goto bad;
                    }
            }
        }
        int in_function = 0;
        for (size_t i = 1; i < n; i++) {
            char op[32] = {0};
            if (scan_ir(lines[i], "%31s", op) != 1)
                continue;
            if (!strcmp(op, "func")) {
                in_function = 1;
                continue;
            }
            if (!strcmp(op, "end")) {
                in_function = 0;
                continue;
            }
            if (in_function || strcmp(op, "extern_func"))
                continue;
            char *copy = strdup(lines[i]);
            if (!copy) {
                backend_error(error, es, i + 1, NULL, "out of memory reading external function");
                goto bad;
            }
            char *save = NULL;
            strtok_r(copy, " \t\r\n", &save);
            char *name = strtok_r(NULL, " \t\r\n", &save);
            char *count_text = strtok_r(NULL, " \t\r\n", &save);
            char *return_text = strtok_r(NULL, " \t\r\n", &save);
            unsigned count = 0;
            Function signature = {0};
            int valid = name && valid_asm_symbol(name) && count_text && return_text &&
                        parse_u(count_text, &count) && count <= 128 &&
                        return_ty(return_text) != T_BAD;
            if (valid) {
                signature.param_count = count;
                signature.ret = return_ty(return_text);
                for (unsigned j = 0; j < count; j++) {
                    char *type = strtok_r(NULL, " \t\r\n", &save);
                    if (!type || parameter_ty(type) == T_BAD || parameter_ty(type) == T_VOID) {
                        valid = 0;
                        break;
                    }
                    signature.params[j].type = parameter_ty(type);
                }
                if (strtok_r(NULL, " \t\r\n", &save))
                    valid = 0;
            }
            Function *existing = valid ? findfn(funcs, nf, name) : NULL;
            if (existing) {
                if (existing->param_count != count || existing->ret != signature.ret)
                    valid = 0;
                else
                    for (unsigned j = 0; j < count; j++)
                        if (existing->params[j].type != signature.params[j].type)
                            valid = 0;
                if (valid)
                    existing->declared_extern = 1;
            } else if (valid) {
                if (!reserve_function(&funcs, nf, &function_capacity)) {
                    free(copy);
                    backend_error(error, es, i + 1, NULL,
                                  "out of memory storing external functions");
                    goto bad;
                }
                signature.name = strdup(name);
                if (!signature.name) {
                    free(copy);
                    backend_error(error, es, i + 1, NULL,
                                  "out of memory storing external function");
                    goto bad;
                }
                signature.external = 1;
                signature.declared_extern = 1;
                funcs[nf++] = signature;
            }
            if (!valid) {
                free(copy);
                backend_error(error, es, i + 1, NULL,
                              "invalid or conflicting external function declaration");
                goto bad;
            }
            free(copy);
        }
        if (abi_variadic_ir) {
            int in_function = 0;
            for (size_t i = 1; i < n; i++) {
                char op[32] = {0}, name[128] = {0}, extra[2] = {0};
                int fields = scan_ir(lines[i], "%31s %127s %1s", op, name, extra);
                if (!strcmp(op, "func")) {
                    in_function = 1;
                    continue;
                }
                if (!strcmp(op, "end")) {
                    in_function = 0;
                    continue;
                }
                if (in_function || strcmp(op, "abi_variadic"))
                    continue;
                Function *f = fields == 2 ? findfn(funcs, nf, name) : NULL;
                if (!f || !f->param_count || f->abi_variadic) {
                    backend_error(error, es, i + 1, NULL,
                                  "invalid or duplicate ABI variadic annotation");
                    goto bad;
                }
                f->abi_variadic = 1;
            }
            for (size_t i = 0; i < nf; i++) {
                if (funcs[i].variadic && !funcs[i].abi_variadic) {
                    snprintf(error, es, "missing ABI variadic annotation for '%s'", funcs[i].name);
                    goto bad;
                }
            }
        }
        for (size_t i = 0; i < nf; i++) {
            for (size_t j = 0; j < ng; j++)
                if (!strcmp(funcs[i].name, globals[j].name)) {
                    snprintf(error, es, "function and global share symbol '%s'", funcs[i].name);
                    goto bad;
                }
            for (size_t j = 0; j < nb; j++)
                if (!strcmp(funcs[i].name, blocks[j].name)) {
                    snprintf(error, es, "function and global share symbol '%s'", funcs[i].name);
                    goto bad;
                }
            for (size_t j = 0; j < na; j++)
                if (!strcmp(funcs[i].name, aliases[j].name)) {
                    snprintf(error, es, "function and global share symbol '%s'", funcs[i].name);
                    goto bad;
                }
        }
    }
    for (size_t i = 0; i < ng; i++) {
        for (size_t j = 0; j < nb; j++)
            if (!strcmp(globals[i].name, blocks[j].name)) {
                snprintf(error, es, "global and block share symbol '%s'", globals[i].name);
                goto bad;
            }
        for (size_t j = 0; j < na; j++)
            if (!strcmp(globals[i].name, aliases[j].name)) {
                snprintf(error, es, "global and alias share symbol '%s'", globals[i].name);
                goto bad;
            }
        if (!module)
            for (size_t j = 0; j < nf; j++)
                if (!strcmp(globals[i].name, funcs[j].name)) {
                    snprintf(error, es, "function and global share symbol '%s'", globals[i].name);
                    goto bad;
                }
    }
    for (size_t i = 0; i < ng; i++)
        if (is_array_registry_symbol(globals[i].name) || is_file_syscall_symbol(globals[i].name) ||
            !strcmp(globals[i].name, "__crunch_pow") ||
            (module && is_module_reserved_symbol(globals[i].name))) {
            snprintf(error, es, "reserved array registry or module symbol '%s'", globals[i].name);
            goto bad;
        }
    for (size_t i = 0; i < nb; i++)
        if (is_array_registry_symbol(blocks[i].name) || is_file_syscall_symbol(blocks[i].name) ||
            !strcmp(blocks[i].name, "__crunch_pow") ||
            (module && is_module_reserved_symbol(blocks[i].name))) {
            snprintf(error, es, "reserved array registry or module symbol '%s'", blocks[i].name);
            goto bad;
        }
    for (size_t i = 0; i < na; i++)
        if (is_array_registry_symbol(aliases[i].name) || is_file_syscall_symbol(aliases[i].name) ||
            !strcmp(aliases[i].name, "__crunch_pow") ||
            (module && is_module_reserved_symbol(aliases[i].name))) {
            snprintf(error, es, "reserved array registry or module symbol '%s'", aliases[i].name);
            goto bad;
        }
    for (size_t i = 0; i < nf; i++)
        if (is_array_registry_symbol(funcs[i].name) || is_file_syscall_symbol(funcs[i].name) ||
            !strcmp(funcs[i].name, "__crunch_pow") ||
            (module && is_module_reserved_symbol(funcs[i].name))) {
            snprintf(error, es, "reserved array registry or module symbol '%s'", funcs[i].name);
            goto bad;
        }
    for (size_t i = 0; i < ns; i++)
        if (is_array_registry_symbol(strings[i].name) || is_file_syscall_symbol(strings[i].name) ||
            !strcmp(strings[i].name, "__crunch_pow") ||
            (module && is_module_reserved_symbol(strings[i].name) &&
             strncmp(strings[i].name, "__crunch_string_", 16))) {
            snprintf(error, es, "reserved array registry or module symbol '%s'", strings[i].name);
            goto bad;
        }
    {
        int in_function = 0;
        for (size_t i = 1; i < n; i++) {
            char op[32] = {0};
            if (scan_ir(lines[i], "%31s", op) != 1)
                continue;
            if (!strcmp(op, "func")) {
                in_function = 1;
                continue;
            }
            if (!strcmp(op, "end")) {
                in_function = 0;
                continue;
            }
            if (in_function || strcmp(op, "globalbytes"))
                continue;
            char *copy = strdup(lines[i]);
            if (!copy || ninitial == 2048) {
                free(copy);
                backend_error(error, es, i + 1, NULL, "too many global byte arrays or out of memory");
                goto bad;
            }
            char *save = NULL;
            strtok_r(copy, " \t\r\n", &save);
            char *name = strtok_r(NULL, " \t\r\n", &save);
            char *hex = strtok_r(NULL, " \t\r\n", &save);
            char *extra = strtok_r(NULL, " \t\r\n", &save);
            size_t hexlen = hex ? strlen(hex) : 0;
            int valid = name && valid_asm_symbol(name) && hex && !extra &&
                        hexlen >= 2 && !(hexlen % 2) &&
                        hex[hexlen - 2] == '0' && hex[hexlen - 1] == '0' &&
                        hexlen / 2 <= INT32_MAX;
            for (size_t j = 0; valid && j < hexlen; j++)
                if (!((hex[j] >= '0' && hex[j] <= '9') ||
                      (hex[j] >= 'a' && hex[j] <= 'f') ||
                      (hex[j] >= 'A' && hex[j] <= 'F')))
                    valid = 0;
            size_t index = SIZE_MAX, length_index = SIZE_MAX;
            char length_name[136];
            if (valid && snprintf(length_name, sizeof length_name, "%s$length", name) >=
                             (int)sizeof length_name)
                valid = 0;
            for (size_t j = 0; valid && j < ng; j++) {
                if (!strcmp(globals[j].name, name)) {
                    if (index != SIZE_MAX)
                        valid = 0;
                    index = j;
                }
                if (!strcmp(globals[j].name, length_name)) {
                    if (length_index != SIZE_MAX)
                        valid = 0;
                    length_index = j;
                }
            }
            if (valid && (index == SIZE_MAX || length_index == SIZE_MAX ||
                          globals[index].type != T_PTR || globals[length_index].type != T_INT ||
                          globals[index].external || globals[length_index].external ||
                          globals[index].initial_bytes != SIZE_MAX))
                valid = 0;
            if (valid) {
                char *end;
                errno = 0;
                unsigned long long address = strtoull(globals[index].value, &end, 0);
                if (errno || end == globals[index].value || *end || address != 0)
                    valid = 0;
                errno = 0;
                unsigned long long length = strtoull(globals[length_index].value, &end, 0);
                if (errno || end == globals[length_index].value || *end ||
                    length != hexlen / 2)
                    valid = 0;
            }
            if (!valid) {
                free(copy);
                backend_error(error, es, i + 1, NULL, "invalid global byte array directive");
                goto bad;
            }
            initial_bytes[ninitial].hex = strdup(hex);
            if (!initial_bytes[ninitial].hex) {
                free(copy);
                backend_error(error, es, i + 1, NULL, "out of memory storing global byte array");
                goto bad;
            }
            globals[index].initial_bytes = ninitial++;
            free(copy);
        }
    }
    if (supports_blockreloc) {
        int in_function = 0;
        for (size_t i = 1; i < n; i++) {
            char op[32] = {0}, name[128] = {0}, offset_text[32] = {0};
            char target[128] = {0}, addend_text[32] = {0}, extra[2] = {0};
            int fields = scan_ir(lines[i], "%31s %127s %31s %127s %31s %1s",
                                op, name, offset_text, target, addend_text, extra);
            if (fields < 1)
                continue;
            if (!strcmp(op, "func")) {
                in_function = 1;
                continue;
            }
            if (!strcmp(op, "end")) {
                in_function = 0;
                continue;
            }
            int scalar = !strcmp(op, "globalreloc");
            if (in_function || (strcmp(op, "blockreloc") && !scalar))
                continue;
            if (scalar)
                fields = scan_ir(lines[i], "%31s %127s %127s %31s %1s",
                                op, name, target, addend_text, extra);
            unsigned offset = 0;
            char *end = NULL;
            errno = 0;
            long long addend = strtoll(addend_text, &end, 10);
            int addend_valid = !errno && end != addend_text && !*end;
            int valid = fields == (scalar ? 4 : 5) && valid_asm_symbol(name) &&
                        valid_asm_symbol(target) &&
                        (scalar || (nr < 2048 && parse_u(offset_text, &offset))) &&
                        addend_valid;
            size_t block = SIZE_MAX, matches = 0;
            Global *global = NULL;
            for (size_t j = 0; j < nb; j++) {
                if (!scalar && !strcmp(blocks[j].name, name)) {
                    if (block != SIZE_MAX)
                        valid = 0;
                    block = j;
                }
                if (!strcmp(blocks[j].name, target))
                    matches++;
            }
            for (size_t j = 0; j < ng; j++) {
                if (scalar && !strcmp(globals[j].name, name)) {
                    if (global)
                        valid = 0;
                    global = &globals[j];
                }
                if (!strcmp(globals[j].name, target))
                    matches++;
            }
            for (size_t j = 0; j < na; j++)
                if (!strcmp(aliases[j].name, target))
                    matches++;
            for (size_t j = 0; j < ns; j++)
                if (!strcmp(strings[j].name, target))
                    matches++;
            if (function_pointer_ir_types && findfn(funcs, nf, target))
                matches++;
            if (matches != 1)
                valid = 0;
            if (scalar) {
                if (!global || global->external || global->type != T_PTR ||
                    global->has_relocation || global->initial_bytes != SIZE_MAX ||
                    !global->value || strcmp(global->value, "0"))
                    valid = 0;
            } else if (block == SIZE_MAX || offset > blocks[block].size ||
                       blocks[block].size - offset < 8) {
                valid = 0;
            }
            if (valid && !scalar && blocks[block].hex) {
                size_t initialized = blocks[block].hex_bytes;
                for (size_t j = offset; j < (size_t)offset + 8 && j < initialized; j++)
                    if (blocks[block].hex[j * 2] != '0' || blocks[block].hex[j * 2 + 1] != '0')
                        valid = 0;
            }
            if (!valid) {
                backend_error(error, es, i + 1, NULL, "invalid static pointer relocation");
                goto bad;
            }
            if (scalar) {
                global->has_relocation = 1;
                memcpy(global->relocation_target, target, strlen(target) + 1);
                global->relocation_addend = addend;
                continue;
            }
            relocations[nr].block = block;
            relocations[nr].offset = offset;
            memcpy(relocations[nr].target, target, strlen(target) + 1);
            relocations[nr++].addend = addend;
        }
        for (size_t i = 1; i < nr; i++) {
            BlockRelocation item = relocations[i];
            size_t j = i;
            while (j && compare_block_relocations(&item, &relocations[j - 1]) < 0) {
                relocations[j] = relocations[j - 1];
                j--;
            }
            relocations[j] = item;
        }
        for (size_t i = 1; i < nr; i++) {
            if (relocations[i].block == relocations[i - 1].block &&
                relocations[i].offset < relocations[i - 1].offset + 8) {
                snprintf(error, es, "overlapping block relocations");
                goto bad;
            }
        }
    }
    for (size_t i = 1; i < n; i++) {
        char op[32] = {0}, name[128] = {0}, extra[2] = {0};
        if (scan_ir(lines[i], "%31s %127s %1s", op, name, extra) < 1 ||
            strcmp(op, "internal"))
            continue;
        if (!name[0] || extra[0]) {
            backend_error(error, es, i + 1, NULL, "invalid internal directive");
            goto bad;
        }
        int found = 0;
        for (size_t j = 0; j < ng; j++)
            if (!strcmp(globals[j].name, name)) {
                if (globals[j].declared_extern) {
                    backend_error(error, es, i + 1, NULL,
                                  "external global '%s' cannot have internal linkage", name);
                    goto bad;
                }
                if (globals[j].internal) {
                    backend_error(error, es, i + 1, NULL, "duplicate internal directive");
                    goto bad;
                }
                globals[j].internal = 1;
                found++;
            }
        for (size_t j = 0; j < nb; j++)
            if (!strcmp(blocks[j].name, name)) {
                if (blocks[j].internal) {
                    backend_error(error, es, i + 1, NULL, "duplicate internal directive");
                    goto bad;
                }
                blocks[j].internal = 1;
                found++;
            }
        for (size_t j = 0; j < nf; j++)
            if (!strcmp(funcs[j].name, name)) {
                if (funcs[j].declared_extern) {
                    backend_error(error, es, i + 1, NULL,
                                  "external function '%s' cannot have internal linkage", name);
                    goto bad;
                }
                if (funcs[j].internal) {
                    backend_error(error, es, i + 1, NULL, "duplicate internal directive");
                    goto bad;
                }
                funcs[j].internal = 1;
                found++;
            }
        if (found != 1 || !strcmp(name, "main")) {
            backend_error(error, es, i + 1, NULL, "invalid internal symbol '%s'", name);
            goto bad;
        }
    }
    for (size_t i = 1; i < n; i++) {
        char op[32] = {0}, name[128] = {0}, extra[2] = {0};
        if (scan_ir(lines[i], "%31s %127s %1s", op, name, extra) < 1 ||
            strcmp(op, "thread_local"))
            continue;
        Global *g = NULL;
        for (size_t j = 0; j < ng; j++)
            if (!strcmp(globals[j].name, name))
                g = &globals[j];
        if (!tls_ir || !name[0] || extra[0] || !g || !g->internal ||
            g->external || g->declared_extern || g->thread_local) {
            backend_error(error, es, i + 1, NULL, "invalid thread_local directive");
            goto bad;
        }
        g->thread_local = 1;
        if (!validate_only && emit_start) {
            backend_error(error, es, i + 1, NULL,
                          "thread-local storage requires --no-entry and a TLS-capable C runtime");
            goto bad;
        }
    }
    for (size_t i = 0; i < ng; ++i)
        if (globals[i].has_relocation &&
            thread_local_global(globals, ng, globals[i].relocation_target)) {
            snprintf(error, es, "static initializer cannot contain a thread-local address");
            goto bad;
        }
    for (size_t i = 0; i < nr; ++i)
        if (thread_local_global(globals, ng, relocations[i].target)) {
            snprintf(error, es, "static initializer cannot contain a thread-local address");
            goto bad;
        }
    if (has_initializer) {
        Function *init = findfn(funcs, nf, initializer);
        if (!init || init->external || !init->internal || init->param_count || init->ret != T_INT) {
            snprintf(error, es, "initializer must name an internal function with signature int ()");
            goto bad;
        }
    }
    if (!validate_labels(lines, funcs, nf, error, es))
        goto bad;
    if (!validate_call_sites(lines, funcs, nf, error, es, module))
        goto bad;
    if (!validate_ir_stacks(lines, n, funcs, nf, error, es, module))
        goto bad;
    Function *main_function = findfn(funcs, nf, "main");
    if ((!main_function || main_function->external) && !module) {
        snprintf(error, es, "IR has no main function");
        goto bad;
    }
    if (main_function && !main_function->external &&
        ((module && main_function->ret != T_INT) || main_function->param_count > 3 ||
         (main_function->param_count && main_function->params[0].type != T_INT))) {
        snprintf(error, es, "invalid entry-point parameter signature");
        goto bad;
    }
    for (size_t i = 1; main_function && !main_function->external &&
                       i < main_function->param_count; i++)
        if (main_function->params[i].type != T_PTR) {
            snprintf(error, es, "invalid entry-point pointer parameter");
            goto bad;
        }
    if (float_ir_types) {
        for (size_t i = 0; i < na; i++) {
            GlobalBlock *block = NULL;
            for (size_t j = 0; j < nb; j++)
                if (!strcmp(aliases[i].block, blocks[j].name)) {
                    block = &blocks[j];
                    break;
                }
            size_t width = scalar_size(aliases[i].type);
            if (!block || !width || aliases[i].offset > block->size ||
                width > block->size - aliases[i].offset) {
                snprintf(error, es, "invalid global alias '%s'", aliases[i].name);
                goto bad;
            }
        }
        for (size_t i = 0; i < nla; i++) {
            LocalBlock *block = NULL;
            for (size_t j = 0; j < nlb; j++)
                if (lblocks[j].owner == laliases[i].owner &&
                    !strcmp(laliases[i].block, lblocks[j].name)) {
                    block = &lblocks[j];
                    break;
                }
            size_t width = scalar_size(laliases[i].type);
            if (!block || !width || laliases[i].offset > block->size ||
                width > block->size - laliases[i].offset) {
                snprintf(error, es, "invalid local alias '%s'", laliases[i].name);
                goto bad;
            }
        }
        for (size_t i = 0; i < ng; i++) {
            Global *g = &globals[i];
            if (g->external || g->type == T_FLOAT)
                continue;
            if (!valid_character_initializer(g->value, g->type)) {
                snprintf(error, es, "invalid initializer for global '%s'", g->name);
                goto bad;
            }
            char *end;
            uint64_t value;
            errno = 0;
            (void)strtoull(g->value, &end, 0);
            if ((g->type == T_USHORT || g->type == T_UINT || g->type == T_ULONG)
                    ? !parse_uint_literal(g->value, g->type == T_USHORT ? UINT16_MAX :
                                          g->type == T_UINT ? UINT32_MAX : UINT64_MAX,
                                          &value)
                    : errno || end == g->value || *end) {
                snprintf(error, es, "invalid initializer for global '%s'", g->name);
                goto bad;
            }
        }
        if (validate_only)
            goto success;
    }
    if (source_filename) {
        fputs(".file \"", out);
        for (const unsigned char *p = (const unsigned char *)source_filename; *p; ++p)
            emit_quoted_byte(out, *p);
        fputs("\"\n", out);
    }
    if (ng || nb || na) {
        fputs(".section .data\n", out);
        for (size_t i = 0; i < ng; i++) {
            Global *g = &globals[i];
            if (g->external)
                continue;
            fputs(g->thread_local ? ".section .tdata,\"awT\",@progbits\n"
                                 : ".section .data\n", out);
            char *end;
            errno = 0;
            unsigned long long bits = strtoull(g->value, &end, 0);
            uint64_t unsigned_value;
            if ((g->type == T_USHORT || g->type == T_UINT || g->type == T_ULONG)
                    ? !parse_uint_literal(g->value, g->type == T_USHORT ? UINT16_MAX :
                                          g->type == T_UINT ? UINT32_MAX : UINT64_MAX,
                                          &unsigned_value)
                    : errno || end == g->value || *end) {
                snprintf(error, es, "invalid initializer for global '%s'", g->name);
                goto bad;
            }
            if (g->type == T_USHORT || g->type == T_UINT || g->type == T_ULONG)
                bits = (unsigned long long)unsigned_value;
            if (aligned_globals_ir)
                fputs(".balign 16\n", out);
            fprintf(out, ".%s %s\n.type %s, @%s\n",
                    g->internal ? "local" : "globl", g->name, g->name,
                    g->thread_local ? "tls_object" : "object");
            if (g->has_relocation && g->relocation_addend)
                fprintf(out, ".size %s, 8\n%s:\n  .quad %s%+lld\n", g->name, g->name,
                        g->relocation_target, g->relocation_addend);
            else if (g->has_relocation)
                fprintf(out, ".size %s, 8\n%s:\n  .quad %s\n", g->name, g->name,
                        g->relocation_target);
            else if (g->initial_bytes != SIZE_MAX)
                fprintf(out, ".size %s, 8\n%s:\n  .quad .Lcrunch_initial_%zu\n",
                        g->name, g->name, g->initial_bytes);
            else if (g->type == T_SHORT || g->type == T_USHORT)
                fprintf(out, ".balign 2\n.size %s, 2\n%s:\n  .short %llu\n",
                        g->name, g->name, bits & 65535ULL);
            else if (g->type == T_BYTE || g->type == T_SBYTE || g->type == T_UBYTE)
                fprintf(out, ".size %s, 1\n%s:\n  .byte %llu\n", g->name, g->name, bits & 255ULL);
            else if (g->type == T_FLOAT)
                fprintf(out, ".balign 4\n.size %s, 4\n%s:\n  .long %llu\n", g->name, g->name, bits);
            else if (g->type == T_UINT)
                fprintf(out, ".size %s, 4\n%s:\n  .long %llu\n", g->name, g->name, bits);
            else if (g->type == T_INT)
                fprintf(out, ".size %s, 4\n%s:\n  .long %lld\n", g->name, g->name,
                        (long long)(int32_t)bits);
            else
                fprintf(out, ".size %s, 8\n%s:\n  .quad 0x%016llx\n", g->name, g->name, bits);
        }
        fputs(".section .data\n", out);
        if (ninitial) {
            fputs(".Lcrunch_initial_start:\n", out);
            for (size_t i = 0; i < ninitial; i++) {
                fprintf(out, ".Lcrunch_initial_%zu:\n", i);
                for (size_t j = 0; initial_bytes[i].hex[j]; j += 2)
                    fprintf(out, "  .byte 0x%c%c\n",
                            initial_bytes[i].hex[j], initial_bytes[i].hex[j + 1]);
            }
            fputs(".Lcrunch_initial_end:\n", out);
            fputs(".section __crunch_static_arrays,\"aw\",@progbits\n"
                  ".balign 8\n  .quad .Lcrunch_initial_start, .Lcrunch_initial_end\n"
                  ".section .data\n", out);
        }
        size_t reloc_index = 0;
        for (size_t i = 0; i < nb; i++) {
            int has_reloc = reloc_index < nr && relocations[reloc_index].block == i;
            fputs(blocks[i].hex || has_reloc ? ".section .data\n" : ".section .bss\n", out);
            fprintf(out,
                    ".balign %zu\n.%s %s\n.type %s, @object\n.size %s, %zu\n%s:\n",
                    blocks[i].align, blocks[i].internal ? "local" : "globl",
                    blocks[i].name, blocks[i].name, blocks[i].name, blocks[i].size,
                    blocks[i].name);
            size_t position = 0;
            while (reloc_index < nr && relocations[reloc_index].block == i) {
                BlockRelocation *reloc = &relocations[reloc_index++];
                emit_block_bytes(out, blocks[i].hex, blocks[i].hex_bytes, position, reloc->offset);
                if (reloc->addend)
                    fprintf(out, "  .quad %s%+lld\n", reloc->target, reloc->addend);
                else
                    fprintf(out, "  .quad %s\n", reloc->target);
                position = reloc->offset + 8;
            }
            emit_block_bytes(out, blocks[i].hex, blocks[i].hex_bytes, position, blocks[i].size);
        }
        for (size_t i = 0; i < na; i++)
            fprintf(out, ".set %s, %s+%zu\n", aliases[i].name, aliases[i].block, aliases[i].offset);
    }
    int needs_free = 0;
    for (size_t i = 1; i < n; i++) {
        char op[32] = {0};
        if (scan_ir(lines[i], "%31s", op) == 1 &&
            (!strcmp(op, "array_resize") || !strcmp(op, "array_resize_bytes"))) {
            needs_free = 1;
            break;
        }
    }
    if (needs_free)
        fputs(".weak __start___crunch_static_arrays\n"
              ".weak __stop___crunch_static_arrays\n"
              ".section .text\n.Lcrunch_free_array:\n"
              "  lea __start___crunch_static_arrays(%rip), %rax\n"
              "  lea __stop___crunch_static_arrays(%rip), %rcx\n"
              ".Lcrunch_free_scan:\n  cmp %rcx, %rax\n  jae .Lcrunch_free_dynamic\n"
              "  cmp (%rax), %rdi\n  jb .Lcrunch_free_next\n"
              "  cmp 8(%rax), %rdi\n  jb .Lcrunch_free_done\n"
              ".Lcrunch_free_next:\n  add $16, %rax\n  jmp .Lcrunch_free_scan\n"
              ".Lcrunch_free_done:\n  ret\n"
              ".Lcrunch_free_dynamic:\n  jmp __crunch_free_bytes\n", out);
    if (has_initializer) {
        fputs(".section .bss\n.Lcrunch_init_done:\n  .byte 0\n"
              ".section .text\n.Lcrunch_init_wrapper:\n"
              "  cmpb $0, .Lcrunch_init_done(%rip)\n  jne .Lcrunch_init_return\n"
              "  movb $1, .Lcrunch_init_done(%rip)\n"
              "  sub $8, %rsp\n", out);
        fprintf(out, "  call %s\n", initializer);
        fputs("  add $8, %rsp\n.Lcrunch_init_return:\n  ret\n"
              ".section .init_array,\"aw\",@init_array\n"
              ".balign 8\n  .quad .Lcrunch_init_wrapper\n"
              ".section __crunch_init_array,\"aw\",@progbits\n"
              ".balign 8\n  .quad .Lcrunch_init_wrapper\n", out);
    }
    if (large_runtime_call_ir) {
        for (size_t i = 1; i < n; i++) {
            char op[32] = {0}, name[128] = {0};
            if (scan_ir(lines[i], "%31s %127s", op, name) == 2 &&
                !strcmp(op, "require_user_runtime"))
                fprintf(out, ".section .rodata.__crunch_guard_%s,\"aG\",@progbits,"
                             "__crunch_guard_%s,comdat\n"
                             ".globl __crunch_guard_%s\n.hidden __crunch_guard_%s\n"
                             ".globl __crunch_dso_guard_%s\n"
                             "__crunch_guard_%s:\n__crunch_dso_guard_%s:\n  .byte 0\n",
                        name, name, name, name, name, name, name);
        }
    }
    if (module) {
        fputs(".section __crunch_abi,\"aR\",@progbits\n", out);
        for (size_t i = 0; i < nf; i++) {
            const Function *f = &funcs[i];
            if (f->internal)
                continue;
            fprintf(out, "  .ascii \"CABI1 %c %s %s ",
                    f->external ? 'R' : 'D', f->name,
                    f->abi_variadic || (!f->external && f->variadic) ? "variadic" : "unknown");
            emit_abi_type(out, f->ret);
            for (size_t j = 0; j < f->param_count; j++) {
                fputc(' ', out);
                emit_abi_type(out, f->params[j].type);
            }
            fputs("\\n\"\n", out);
        }
        if (large_runtime_call_ir) {
            for (size_t i = 1; i < n; i++) {
                char op[32] = {0}, name[128] = {0};
                if (scan_ir(lines[i], "%31s %127s", op, name) == 2 &&
                    !strcmp(op, "require_user_runtime"))
                    fprintf(out, "  .ascii \"CABI1 G %s\\n\"\n", name);
            }
        }
    }
    if (emit_start && main_function && !main_function->external)
        emit_entry(out, main_function);
    fputs(".section .text\n", out);
    for (size_t i = 0; i < nf; i++) {
        if (funcs[i].external)
            continue;
        emit_function(out, lines, &funcs[i], funcs, nf, globals, ng, lblocks, laliases, error, es, &labels,
                      ".Lcrunch_free_array");
        if (error[0])
            goto bad;
    }
    if (ns) {
        fputs(".section .rodata\n", out);
        for (size_t i = 0; i < ns; i++) {
            fprintf(out, ".balign %u\n%s:\n", strings[i].alignment, strings[i].name);
            emit_hex_string(out, strings[i].hex, 0, strlen(strings[i].hex) / 2);
        }
        fputs(".section .text\n", out);
    }
    fputs(".section .note.GNU-stack,\"\",@progbits\n", out);
success:
    for (size_t i = 0; i < nf; i++) {
        free(funcs[i].name);
        free(funcs[i].stack_depths);
    }
    free(funcs);
    for (size_t i = 0; i < ng; i++) {
        free(globals[i].name);
        free(globals[i].value);
    }
    for (size_t i = 0; i < nb; i++) {
        free(blocks[i].name);
        free(blocks[i].hex);
    }
    for (size_t i = 0; i < na; i++) {
        free(aliases[i].name);
        free(aliases[i].block);
    }
    for (size_t i = 0; i < nlb; i++)
        free(lblocks[i].name);
    for (size_t i = 0; i < nla; i++) {
        free(laliases[i].name);
        free(laliases[i].block);
    }
    free(lblocks);
    free(laliases);
    for (size_t i = 0; i < ns; i++) {
        free(strings[i].name);
        free(strings[i].hex);
    }
    for (size_t i = 0; i < ninitial; i++)
        free(initial_bytes[i].hex);
    for (size_t i = 0; i < n; i++)
        free(lines[i]);
    free(lines);
    if (validate_only) {
        fclose(out);
        return 0;
    }
    {
        int rc = optimize_assembly(out, output, error, es);
        fclose(out);
        return rc;
    }
bad:
    free(line);
    for (size_t i = 0; i < n; i++)
        free(lines[i]);
    free(lines);
    for (size_t i = 0; i < nf; i++) {
        free(funcs[i].name);
        free(funcs[i].stack_depths);
    }
    free(funcs);
    for (size_t i = 0; i < ng; i++) {
        free(globals[i].name);
        free(globals[i].value);
    }
    for (size_t i = 0; i < nb; i++) {
        free(blocks[i].name);
        free(blocks[i].hex);
    }
    for (size_t i = 0; i < na; i++) {
        free(aliases[i].name);
        free(aliases[i].block);
    }
    for (size_t i = 0; i < nlb; i++)
        free(lblocks[i].name);
    for (size_t i = 0; i < nla; i++) {
        free(laliases[i].name);
        free(laliases[i].block);
    }
    free(lblocks);
    free(laliases);
    for (size_t i = 0; i < ns; i++) {
        free(strings[i].name);
        free(strings[i].hex);
    }
    for (size_t i = 0; i < ninitial; i++)
        free(initial_bytes[i].hex);
    fclose(out);
    return 1;
}

/**
 * @brief Validates or translates IR and returns structured diagnostics on failure.
 * @param input Input FILE stream containing textual Crunch IR.
 * @param output Output FILE stream receiving generated assembly.
 * @param diagnostic Diagnostic record to populate with backend or IR context.
 * @param message Writable character buffer receiving output.
 * @param message_size Capacity or size in bytes.
 * @param validate_only Nonzero to validate without producing assembly.
 * @param source_filename Source filename for .file, or NULL to omit it.
 * @param emit_start Nonzero to emit the Crunch process entry point.
 * @return Zero on success; nonzero when output or translation fails.
 */
static int translate_with_diagnostic(FILE *input, FILE *output, Diagnostic *diagnostic,
                                     char *message, size_t message_size, int validate_only,
                                     const char *source_filename, int emit_start) {
    Diagnostic *previous = active_backend_diagnostic;
    int status;

    if (diagnostic) {
        *diagnostic = (Diagnostic){0};
        diagnostic->code = "E_IR";
        diagnostic->severity = DIAGNOSTIC_ERROR;
    }
    if (!input || (!validate_only && !output) || !diagnostic || !message || message_size == 0)
        return 1;
    message[0] = '\0';

    active_backend_diagnostic = diagnostic;
    status = translate_ir(input, output, message, message_size, validate_only, source_filename,
                          emit_start);
    active_backend_diagnostic = previous;

    if (status != 0 && !diagnostic->message) {
        diagnostic->message = message[0] ? message : "translation failed";
    }
    return status;
}

/**
 * @brief Translates IR and returns structured diagnostics on failure.
 * @param input Input FILE stream containing textual Crunch IR.
 * @param output Output FILE stream receiving generated assembly.
 * @param diagnostic Diagnostic record to populate.
 * @param message Writable character buffer receiving output.
 * @param message_size Capacity of the message buffer.
 * @return Zero on success; nonzero on validation or translation failure.
 */
int crunch_x86_64_translate_with_diagnostic(FILE *input, FILE *output, Diagnostic *diagnostic,
                                            char *message, size_t message_size) {
    return translate_with_diagnostic(input, output, diagnostic, message, message_size, 0, NULL, 1);
}

/**
 * @brief Translates IR with an optional source filename and structured diagnostics.
 * @param input Input FILE stream containing textual Crunch IR.
 * @param output Output FILE stream receiving generated assembly.
 * @param source_filename Source filename for .file, or NULL to omit it.
 * @param diagnostic Diagnostic record to populate.
 * @param message Writable character buffer receiving output.
 * @param message_size Capacity of the message buffer.
 * @return Zero on success; nonzero on validation or translation failure.
 */
int crunch_x86_64_translate_source_with_diagnostic(FILE *input, FILE *output,
        const char *source_filename, Diagnostic *diagnostic, char *message, size_t message_size) {
    return translate_with_diagnostic(input, output, diagnostic, message, message_size, 0,
                                     source_filename, 1);
}

/**
 * @brief Translates IR with configurable process entry emission.
 * @param input FILE stream containing textual Crunch IR.
 * @param output Output FILE stream receiving generated assembly.
 * @param source_filename Source filename for .file, or NULL to omit it.
 * @param emit_start Nonzero to emit the Crunch process entry point.
 * @param diagnostic Diagnostic record to populate.
 * @param message Writable character buffer receiving output.
 * @param message_size Capacity of the message buffer.
 * @return Zero on success; nonzero on validation or translation failure.
 */
int crunch_x86_64_translate_source_options_with_diagnostic(FILE *input, FILE *output,
        const char *source_filename, int emit_start, Diagnostic *diagnostic,
        char *message, size_t message_size) {
    return translate_with_diagnostic(input, output, diagnostic, message, message_size, 0,
                                     source_filename, emit_start);
}

/**
 * @brief Validates IR without emitting assembly.
 * @param input Input FILE stream containing textual Crunch IR.
 * @param diagnostic Diagnostic record to populate.
 * @param message Writable character buffer receiving output.
 * @param message_size Capacity of the message buffer.
 * @return Zero on success; nonzero on validation failure.
 */
int crunch_x86_64_validate_with_diagnostic(FILE *input, Diagnostic *diagnostic,
                                           char *message, size_t message_size) {
    return translate_with_diagnostic(input, NULL, diagnostic, message, message_size, 1, NULL, 1);
}
