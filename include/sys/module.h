#ifndef CRUNCH_SYS_MODULE_H
#define CRUNCH_SYS_MODULE_H
#include <stddef.h>
/**
 * @brief Loads a kernel module from an in-memory image; requires CAP_SYS_MODULE and may be disabled.
 * @param image Module image in memory.
 * @param length Length in bytes.
 * @param parameters Module parameter string.
 * @return Returns the syscall result, or -1 with errno set.
 */
int init_module(const void* image, unsigned long length, const char* parameters);
/**
 * @brief Removes a kernel module; requires CAP_SYS_MODULE and may be disabled.
 * @param name Name or module path.
 * @param flags Linux operation flags.
 * @return Returns the syscall result, or -1 with errno set.
 */
int delete_module(const char* name, unsigned int flags);
/**
 * @brief Invokes obsolete create_module syscall 174, unimplemented on Linux x86-64 and returning ENOSYS.
 * @param name Name or module path.
 * @param size Size in bytes.
 * @return Returns the syscall result, or -1 with errno set.
 */
long crunch_create_module(const char* name, size_t size);
/**
 * @brief Invokes obsolete get_kernel_syms syscall 177, unimplemented on Linux x86-64 and returning ENOSYS.
 * @param symbols Legacy symbol buffer.
 * @return Returns the syscall result, or -1 with errno set.
 */
int crunch_get_kernel_syms(void* symbols);
/**
 * @brief Invokes obsolete query_module syscall 178, unimplemented on Linux x86-64 and returning ENOSYS.
 * @param name Name or module path.
 * @param which Process selector or legacy query selector.
 * @param buffer Operation-specific buffer.
 * @param size Size in bytes.
 * @param result Legacy output size.
 * @return Returns the syscall result, or -1 with errno set.
 */
int crunch_query_module(const char* name, int which, void* buffer,
                        size_t size, size_t* result);
#endif
