#ifndef CRUNCH_STDIO_H
#define CRUNCH_STDIO_H

#include <stddef.h>
#include <sys/types.h>
#include <__crunch_va_list.h>

#ifndef NULL
#macro NULL 0
#endif

#macro EOF -1

#ifndef SEEK_SET
#macro SEEK_SET 0
#macro SEEK_CUR 1
#macro SEEK_END 2
#endif

/* Crunch stream state; fields are private to the runtime. */
struct FILE {
    int _descriptor;
    int _flags;
    int _status;
    int _allocated;
};
typedef struct FILE FILE;

extern struct FILE* _crunch_stdin;
extern struct FILE* _crunch_stdout;
extern struct FILE* _crunch_stderr;

#macro stdin _crunch_stdin
#macro stdout _crunch_stdout
#macro stderr _crunch_stderr

/**
 * @brief Opens a file as a Crunch stream.
 * @param path NUL-terminated path.
 * @param mode Stream mode: r, w, or a, optionally followed by +, b, e, or w's x.
 * @return Stream pointer, or null with errno set on failure.
 */
struct FILE* fopen(const char* path, const char* mode);

/**
 * @brief Reopens an existing stream on a path or changes its mode.
 * @param path Path to open, or NULL to reopen the current descriptor.
 * @param mode Stream mode accepted by fopen.
 * @param stream Existing Crunch stream.
 * @return The same stream on success, or NULL with errno set on failure.
 * @note On failure the original stream is closed and must not be used.
 */
struct FILE* freopen(const char* path, const char* mode, struct FILE* stream);

/**
 * @brief Opens an anonymous, seekable temporary stream.
 * @return Read/write stream, or NULL with errno set on failure.
 * @note Uses Linux memfd_create; data is memory-backed and the descriptor closes on exec.
 */
struct FILE* tmpfile(void);

/**
 * @brief Closes and releases a Crunch stream.
 * @param stream Open Crunch stream; invalid after this call.
 * @return Zero on success, or EOF with errno set on failure.
 */
int fclose(struct FILE* stream);

/**
 * @brief Obtains the descriptor underlying a stream.
 * @param stream Open Crunch stream.
 * @return Descriptor, or -1 with errno set for a null stream.
 */
int fileno(struct FILE* stream);

/**
 * @brief Flushes a stream, or all output streams when stream is null.
 * @param stream Open Crunch stream or NULL.
 * @return Zero; Crunch streams are unbuffered.
 */
int fflush(struct FILE* stream);

/**
 * @brief Sets a stream's file position relative to whence.
 * @param stream Open Crunch stream.
 * @param offset Signed byte offset.
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END.
 * @return Zero on success, or -1 with errno set on failure.
 */
int fseek(struct FILE* stream, long offset, int whence);

/**
 * @brief Gets a stream's current byte position.
 * @param stream Open Crunch stream.
 * @return Position, or -1 with errno set on failure.
 */
long ftell(struct FILE* stream);

/**
 * @brief Seeks to the beginning and clears both stream indicators.
 * @param stream Open Crunch stream.
 * @return No value; a failed seek sets errno and still clears the indicators.
 */
void rewind(struct FILE* stream);

/**
 * @brief Reads up to count items into a writable buffer.
 * @param buffer Destination with room for size times count bytes.
 * @param size Size in bytes of each item.
 * @param count Number of items.
 * @param stream Open Crunch stream.
 * @return Number of complete items read.
 */
size_t fread(void* buffer, size_t size, size_t count, struct FILE* stream);

/**
 * @brief Writes up to count items from a buffer.
 * @param buffer Source containing size times count bytes.
 * @param size Size in bytes of each item.
 * @param count Number of items.
 * @param stream Open Crunch stream.
 * @return Number of complete items written.
 */
size_t fwrite(const void* buffer, size_t size, size_t count, struct FILE* stream);

/**
 * @brief Reads one byte from a stream.
 * @param stream Open Crunch stream.
 * @return Byte value from 0 to 255, or EOF on end-of-file or error.
 */
int fgetc(struct FILE* stream);

#macro getchar() fgetc(stdin)

/**
 * @brief Writes the low byte of a value to a stream.
 * @param character Integer whose low byte is written.
 * @param stream Open Crunch stream.
 * @return Written byte from 0 to 255, or EOF on error.
 */
int fputc(int character, struct FILE* stream);

/**
 * @brief Writes the low byte of a value to a stream.
 * @param character Integer whose low byte is written.
 * @param stream Open Crunch stream.
 * @return Written byte from 0 to 255, or EOF on error.
 */
int putc(int character, struct FILE* stream);

/**
 * @brief Writes the low byte of a value to standard output.
 * @param character Integer whose low byte is written.
 * @return Written byte from 0 to 255, or EOF on error.
 */
int putchar(int character);

/**
 * @brief Reads a line or partial line into a NUL-terminated buffer.
 * @param buffer Writable destination with space for size bytes.
 * @param size Buffer capacity, positive.
 * @param stream Open Crunch stream.
 * @return Buffer on success, or null on EOF before any byte or on error.
 */
char* fgets(char* buffer, int size, struct FILE* stream);

/**
 * @brief Reads a line into a dynamically resized buffer.
 * @param line Pointer to a buffer pointer, initially NULL or allocator-owned.
 * @param capacity Pointer to the buffer capacity in bytes.
 * @param stream Open Crunch stream.
 * @return Byte count including any newline, or -1 on error or EOF before data.
 */
ssize_t getline(char** line, size_t* capacity, struct FILE* stream);

/**
 * @brief Writes a NUL-terminated string without adding a newline.
 * @param text NUL-terminated source string.
 * @param stream Open Crunch stream.
 * @return Nonnegative on success, or EOF on error.
 */
int fputs(const char* text, struct FILE* stream);

/**
 * @brief Writes a string followed by a newline to standard output.
 * @param text NUL-terminated source string.
 * @return Nonnegative on success, or EOF on error.
 */
int puts(const char* text);

/**
 * @brief Tests whether a stream has reached end-of-file.
 * @param stream Open Crunch stream.
 * @return Nonzero if the indicator is set, zero if clear, or -1 with errno set for null.
 */
int feof(struct FILE* stream);

/**
 * @brief Tests whether a stream has encountered an I/O error.
 * @param stream Open Crunch stream.
 * @return Nonzero if the indicator is set, zero if clear, or -1 with errno set for null.
 */
int ferror(struct FILE* stream);

/**
 * @brief Clears a stream's end-of-file and error indicators.
 * @param stream Open Crunch stream.
 * @return No value; a null stream sets errno to EINVAL.
 */
void clearerr(struct FILE* stream);

/**
 * @brief Reads formatted input from standard input.
 * @param format Input conversion specification.
 * @return Number of assignments, or EOF on input failure before any conversion.
 * @note Additional destination pointers are accepted by the compiler.
 */
int scanf(const char* format);

/**
 * @brief Reads formatted input from a stream.
 * @param stream Open input stream.
 * @param format Input conversion specification.
 * @return Number of assignments, or EOF on input failure before any conversion.
 * @note Additional destination pointers are accepted by the compiler.
 */
int fscanf(struct FILE* stream, const char* format);

/**
 * @brief Writes formatted output to standard output.
 * @param format Format string supporting integer, real, string, and character conversions.
 * @return Number of characters written, or -1 for an invalid conversion or write error.
 * @note Additional numeric and byte-pointer arguments are accepted by the compiler.
 */
int printf(const char* format);

/**
 * @brief Writes formatted output to a Crunch stream.
 * @param stream Open Crunch stream.
 * @param format Format string supporting integer, real, string, and character conversions.
 * @return Number of bytes written, or -1 for an invalid conversion or write error.
 * @note Additional numeric and byte-pointer arguments are accepted by the compiler.
 */
int fprintf(struct FILE* stream, const char* format);

/**
 * @brief Writes a System V argument list to a Crunch stream.
 * @param stream Open Crunch stream.
 * @param format Format string supported by the Crunch formatter.
 * @param arguments Pointer to the argument-list state, consumed by the call.
 * @return Number of bytes written, or -1 on error.
 */
int vfprintf(struct FILE* stream, const char* format,
             struct __crunch_va_list_tag* arguments);

/**
 * @brief Formats output into a bounded byte buffer.
 * @param buffer Writable destination, or null when capacity is zero.
 * @param capacity Buffer size in bytes, including the terminating NUL.
 * @param format Format string supporting integer, real, string, and character conversions.
 * @return Number of bytes that would have been written, or -1 on error.
 * @note Additional numeric and byte-pointer arguments are accepted by the compiler.
 */
int snprintf(char* buffer, size_t capacity, const char* format);

/**
 * @brief Formats output into an unbounded byte buffer.
 * @param buffer Writable destination large enough for the result and NUL.
 * @param format Format string supported by the Crunch formatter.
 * @return Number of bytes written, excluding the NUL, or -1 on error.
 * @note Additional numeric and byte-pointer arguments are accepted by the compiler.
 */
int sprintf(char* buffer, const char* format);

/**
 * @brief Writes the message for the current errno to standard error.
 * @param prefix Optional prefix, followed by a colon and space when nonempty.
 * @return No value.
 */
void perror(const char* prefix);

/**
 * @brief Formats a System V argument list into a bounded byte buffer.
 * @param buffer Writable destination, or null when capacity is zero.
 * @param capacity Buffer size in bytes, including the terminating NUL.
 * @param format Format string supported by the Crunch formatter.
 * @param arguments Pointer to the System V argument-list state, consumed by the call.
 * @return Number of bytes that would have been written, or -1 on error.
 */
int vsnprintf(char* buffer, size_t capacity, const char* format,
              struct __crunch_va_list_tag* arguments);

/**
 * @brief Reads one line from standard input without storing its newline.
 * @param buf Writable buffer with space for max_length characters plus a terminating NUL.
 * @param max_length Maximum number of characters to store, excluding the terminating NUL.
 * @return Stored byte count, zero for an empty line/EOF, or -1 with errno set on error.
 * @note On success, excess input is discarded through newline/EOF. Interrupted reads are retried.
 * A zero maximum consumes the line and stores only NUL. NULL buffers or negative
 * maxima fail with EINVAL without consuming input. On a read error, any stored
 * prefix is NUL-terminated; unread input remains for the next call. Success leaves errno unchanged.
 */
int input(char buf[], int max_length);

/**
 * @brief Renames or moves a filesystem entry.
 * @param old_path NUL-terminated existing path.
 * @param new_path NUL-terminated destination path.
 * @return Zero on success, or -1 with errno set.
 */
int rename(const char* old_path, const char* new_path);

/**
 * @brief Renames an entry between directory-relative paths.
 * @param old_directory Source directory descriptor or AT_FDCWD.
 * @param old_path NUL-terminated source path.
 * @param new_directory Destination directory descriptor or AT_FDCWD.
 * @param new_path NUL-terminated destination path.
 * @return Zero on success, or -1 with errno set.
 */
int renameat(int old_directory, const char* old_path, int new_directory, const char* new_path);

#endif /* CRUNCH_STDIO_H */
