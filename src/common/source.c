#include "common/source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Reads a source file and initializes its owned buffer and metadata.
 * @param path Path of the source file to load.
 * @param out Output record receiving the filename, owned source buffer, and byte length.
 * @return COMPILER_OK on success, otherwise an input, I/O, or memory error status.
 */
CompilerStatus source_load(const char *path, SourceFile *out)
{
    FILE *file = NULL;
    char *data = NULL;
    size_t length = 0, capacity = 0;
    char chunk[4096];
    CompilerStatus status = COMPILER_OK;
    int close_file;
    if (!out || !path) return COMPILER_INVALID_INPUT;
    out->filename = path; out->data = NULL; out->length = 0;
    close_file = strcmp(path, "-") != 0;
    file = close_file ? fopen(path, "rb") : stdin;
    if (!file) return COMPILER_IO_ERROR;
    for (;;) {
        size_t n = fread(chunk, 1, sizeof(chunk), file);
        if (n) {
            size_t required;
            if (n > (size_t)-1 - length - 1) {
                status = COMPILER_OUT_OF_MEMORY;
                goto cleanup;
            }
            required = length + n + 1;
            if (required > capacity) {
                size_t cap = capacity ? capacity : 4096;
                while (cap < required) {
                    if (cap > (size_t)-1 / 2) {
                        cap = required;
                        break;
                    }
                    cap *= 2;
                }
                char *grown = (char *)realloc(data, cap);
                if (!grown) {
                    status = COMPILER_OUT_OF_MEMORY;
                    goto cleanup;
                }
                data = grown; capacity = cap;
            }
            if (!data) {
                status = COMPILER_OUT_OF_MEMORY;
                goto cleanup;
            }
            memcpy(data + length, chunk, n); length += n;
        }
        if (n < sizeof(chunk)) {
            if (ferror(file)) {
                status = COMPILER_IO_ERROR;
                goto cleanup;
            }
            break;
        }
    }
    if (!data) {
        data = (char *)malloc(1);
        if (!data) {
            status = COMPILER_OUT_OF_MEMORY;
            goto cleanup;
        }
    }
    data[length] = '\0';
    out->data = data; out->length = length;
    data = NULL;

cleanup:
    free(data);
    if (close_file) fclose(file);
    return status;
}

/**
 * @brief Releases the source buffer and clears the source-file value.
 * @param source Source-file record being populated or released.
 * @return No value; the function performs the documented output or state update.
 */
void source_destroy(SourceFile *source)
{
    if (!source) return;
    free((void *)source->data);
    source->data = NULL; source->length = 0; source->filename = NULL;
}
