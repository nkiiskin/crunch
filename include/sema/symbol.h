#ifndef CRUNCH_SEMA_SYMBOL_H
#define CRUNCH_SEMA_SYMBOL_H

#include <stdbool.h>
#include <stddef.h>
#include "common/compiler.h"

typedef enum {
    SEMANTIC_SYMBOL_VARIABLE,
    SEMANTIC_SYMBOL_FUNCTION,
    SEMANTIC_SYMBOL_STRUCTURE,
    SEMANTIC_SYMBOL_TYPE_ALIAS,
    SEMANTIC_SYMBOL_ENUMERATOR
} SemanticSymbolKind;

typedef struct {
    SourceLocation location;
    const char *message;
    bool warning;
} SemanticDiagnostic;

#endif
