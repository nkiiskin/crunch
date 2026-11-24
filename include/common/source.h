#ifndef CRUNCH_COMMON_SOURCE_H
#define CRUNCH_COMMON_SOURCE_H

#include "common/compiler.h"

/**
 * @brief Loads a source file into an owned, NUL-terminated buffer.
 * @param path Path to read; "-" reads standard input.
 * @param out Output record receiving the filename, buffer, and byte length.
 * @return COMPILER_OK on success, otherwise the applicable input, I/O, or memory error.
 */
CompilerStatus source_load(
    const char *path,
    SourceFile *out
);

/**
 * @brief Releases a loaded source buffer and clears its record.
 * @param source Source-file record returned by source_load.
 * @return No value.
 */
void source_destroy(SourceFile *source);

#endif
