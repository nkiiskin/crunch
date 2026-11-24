#include "optimizer/optimizer.h"

/**
 * @brief Copies the IR stream unchanged.
 * @param input Stream containing IR bytes.
 * @param output Stream receiving the same bytes.
 * @return COMPILER_OK on success or an I/O or input error.
 */
CompilerStatus crunch_optimize_ir(FILE *input, FILE *output)
{
    unsigned char buffer[8192];
    size_t count;
    if (!input || !output)
        return COMPILER_INVALID_INPUT;
    while ((count = fread(buffer, 1, sizeof buffer, input)) != 0) {
        if (fwrite(buffer, 1, count, output) != count)
            return COMPILER_IO_ERROR;
    }
    return ferror(input) || ferror(output) ? COMPILER_IO_ERROR : COMPILER_OK;
}
