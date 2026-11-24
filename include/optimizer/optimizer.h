#ifndef CRUNCH_OPTIMIZER_H
#define CRUNCH_OPTIMIZER_H

#include "common/compiler.h"
#include <stdio.h>

/**
 * @brief Copies Crunch IR unchanged to the output stream.
 * @param input Stream containing IR bytes.
 * @param output Stream receiving the same bytes.
 * @return COMPILER_OK on success or an I/O or input error.
 */
CompilerStatus crunch_optimize_ir(FILE *input, FILE *output);

#endif
