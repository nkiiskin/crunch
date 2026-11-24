#ifndef CRUNCH_SYS_XATTR_H
#define CRUNCH_SYS_XATTR_H
#include <sys/types.h>

#macro XATTR_CREATE 1
#macro XATTR_REPLACE 2

/**
 * @brief Sets an extended attribute on the file named by path, following symbolic links.
 * @param path NUL-terminated path.
 * @param name NUL-terminated extended attribute name.
 * @param value Attribute bytes.
 * @param size Attribute size in bytes.
 * @param flags Linux operation flags.
 * @return The syscall result, or -1 with errno set.
 */
int setxattr(const char* path, const char* name, const void* value, size_t size, int flags);

/**
 * @brief Sets an extended attribute on path without following the final symbolic link.
 * @param path NUL-terminated path.
 * @param name NUL-terminated extended attribute name.
 * @param value Attribute bytes.
 * @param size Attribute size in bytes.
 * @param flags Linux operation flags.
 * @return The syscall result, or -1 with errno set.
 */
int lsetxattr(const char* path, const char* name, const void* value, size_t size, int flags);

/**
 * @brief Sets an extended attribute on an open file.
 * @param descriptor Open file descriptor.
 * @param name NUL-terminated extended attribute name.
 * @param value Attribute bytes.
 * @param size Attribute size in bytes.
 * @param flags Linux operation flags.
 * @return The syscall result, or -1 with errno set.
 */
int fsetxattr(int descriptor, const char* name, const void* value, size_t size, int flags);

/**
 * @brief Reads an extended attribute by path, following symbolic links.
 * @param path NUL-terminated path.
 * @param name NUL-terminated extended attribute name.
 * @param value Writable value buffer, or NULL to query its size.
 * @param size Buffer size in bytes; zero queries the required size.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t getxattr(const char* path, const char* name, void* value, size_t size);

/**
 * @brief Reads an extended attribute without following the final symbolic link.
 * @param path NUL-terminated path.
 * @param name NUL-terminated extended attribute name.
 * @param value Writable value buffer, or NULL to query its size.
 * @param size Buffer size in bytes; zero queries the required size.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t lgetxattr(const char* path, const char* name, void* value, size_t size);

/**
 * @brief Reads an extended attribute from an open file.
 * @param descriptor Open file descriptor.
 * @param name NUL-terminated extended attribute name.
 * @param value Writable value buffer, or NULL to query its size.
 * @param size Buffer size in bytes; zero queries the required size.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t fgetxattr(int descriptor, const char* name, void* value, size_t size);

/**
 * @brief Lists NUL-separated attribute names by path, following symbolic links.
 * @param path NUL-terminated path.
 * @param list Writable attribute-name buffer.
 * @param size Buffer size in bytes; zero queries the required size.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t listxattr(const char* path, char* list, size_t size);

/**
 * @brief Lists NUL-separated attribute names without following the final symbolic link.
 * @param path NUL-terminated path.
 * @param list Writable attribute-name buffer.
 * @param size Buffer size in bytes; zero queries the required size.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t llistxattr(const char* path, char* list, size_t size);

/**
 * @brief Lists NUL-separated attribute names on an open file.
 * @param descriptor Open file descriptor.
 * @param list Writable attribute-name buffer.
 * @param size Buffer size in bytes; zero queries the required size.
 * @return The syscall result, or -1 with errno set.
 */
ssize_t flistxattr(int descriptor, char* list, size_t size);

/**
 * @brief Removes an extended attribute by path, following symbolic links.
 * @param path NUL-terminated path.
 * @param name NUL-terminated extended attribute name.
 * @return The syscall result, or -1 with errno set.
 */
int removexattr(const char* path, const char* name);

/**
 * @brief Removes an extended attribute without following the final symbolic link.
 * @param path NUL-terminated path.
 * @param name NUL-terminated extended attribute name.
 * @return The syscall result, or -1 with errno set.
 */
int lremovexattr(const char* path, const char* name);

/**
 * @brief Removes an extended attribute from an open file.
 * @param descriptor Open file descriptor.
 * @param name NUL-terminated extended attribute name.
 * @return The syscall result, or -1 with errno set.
 */
int fremovexattr(int descriptor, const char* name);

#endif
