#ifndef CRUNCH_SYS_MMAN_H
#define CRUNCH_SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#macro PROT_NONE 0
#macro PROT_READ 1
#macro PROT_WRITE 2
#macro PROT_EXEC 4
#macro MAP_SHARED 1
#macro MAP_PRIVATE 2
#macro MAP_FIXED 16
#macro MAP_ANONYMOUS 32
#macro MREMAP_MAYMOVE 1
#macro MREMAP_FIXED 2
#macro MREMAP_DONTUNMAP 4
#macro MS_ASYNC 1
#macro MS_INVALIDATE 2
#macro MS_SYNC 4
#macro MADV_NORMAL 0
#macro MADV_RANDOM 1
#macro MADV_SEQUENTIAL 2
#macro MADV_WILLNEED 3
#macro MADV_DONTNEED 4
#macro MADV_FREE 8
#macro MADV_REMOVE 9
#macro MADV_DONTFORK 10
#macro MADV_DOFORK 11
#macro MADV_MERGEABLE 12
#macro MADV_UNMERGEABLE 13
#macro MADV_HUGEPAGE 14
#macro MADV_NOHUGEPAGE 15
#macro MADV_DONTDUMP 16
#macro MADV_DODUMP 17
#macro MADV_WIPEONFORK 18
#macro MADV_KEEPONFORK 19
#macro MADV_COLD 20
#macro MADV_PAGEOUT 21
#macro MADV_POPULATE_READ 22
#macro MADV_POPULATE_WRITE 23
#macro MADV_DONTNEED_LOCKED 24
#macro MADV_COLLAPSE 25
#macro MADV_HWPOISON 100
#macro MADV_SOFT_OFFLINE 101
#macro MCL_CURRENT 1
#macro MCL_FUTURE 2
#macro MCL_ONFAULT 4

/**
 * @brief Maps memory using the Linux mmap syscall.
 * @param address Requested address, or zero to let the kernel choose.
 * @param byte_count Mapping length in bytes.
 * @param protection Combination of PROT_* flags.
 * @param flags Combination of MAP_* flags.
 * @param descriptor Backing file descriptor, or -1 for an anonymous mapping.
 * @param offset Page-aligned file offset in bytes.
 * @return Mapping address, or MAP_FAILED with errno set; use map_failed to check.
 */
void* mmap(void* address, size_t byte_count, int protection, int flags, int descriptor, off_t offset);

/**
 * @brief Changes protection on a mapped memory range.
 * @param address Page-aligned start address.
 * @param byte_count Length in bytes.
 * @param protection Combination of PROT_* flags.
 * @return Zero, or -1 with errno set.
 */
int mprotect(void* address, size_t byte_count, int protection);
/**
 * @brief Locks pages containing the specified range in memory, subject to resource limits.
 * @param address Address or request-specific pointer.
 * @param byte_count Length in bytes.
 * @return Returns the syscall result, or -1 with errno set.
 */
int mlock(const void* address, size_t byte_count);
/**
 * @brief Unlocks pages containing the specified range.
 * @param address Address or request-specific pointer.
 * @param byte_count Length in bytes.
 * @return Returns the syscall result, or -1 with errno set.
 */
int munlock(const void* address, size_t byte_count);
/**
 * @brief Locks current or future process mappings according to MCL_* flags and resource limits.
 * @param flags Linux operation flags.
 * @return Returns the syscall result, or -1 with errno set.
 */
int mlockall(int flags);
/**
 * @brief Unlocks the calling process's locked pages.
 * @return Returns the syscall result, or -1 with errno set.
 */
int munlockall(void);

/**
 * @brief Synchronizes a mapped range with its backing file.
 * @param address Page-aligned start address.
 * @param byte_count Length in bytes.
 * @param flags Linux MS_* flags.
 * @return Zero, or -1 with errno set.
 */
int msync(void* address, size_t byte_count, int flags);

/**
 * @brief Reports residency of the pages in a mapping.
 * @param address Page-aligned start address.
 * @param byte_count Length in bytes.
 * @param vector Writable byte per page.
 * @return Zero, or -1 with errno set.
 */
int mincore(void* address, size_t byte_count, unsigned char* vector);

/**
 * @brief Advises Linux about the use of a mapped range.
 * @param address Page-aligned start address.
 * @param byte_count Length in bytes.
 * @param advice Linux MADV_* value.
 * @return Zero, or -1 with errno set.
 */
int madvise(void* address, size_t byte_count, int advice);

/**
 * @brief Unmaps a memory region.
 * @param address Page-aligned mapping address.
 * @param byte_count Length in bytes.
 * @return Zero on success, or -1 with errno set.
 */
int munmap(void* address, size_t byte_count);

/**
 * @brief Resizes or moves a Linux memory mapping.
 * @param address Existing page-aligned mapping address.
 * @param old_size Existing mapping size in bytes.
 * @param new_size Requested mapping size in bytes.
 * @param flags Combination of MREMAP_* flags.
 * @param new_address Destination for MREMAP_FIXED; pass zero otherwise.
 * @return New address, or MAP_FAILED with errno set; use map_failed to check.
 */
void* mremap(void* address, size_t old_size, size_t new_size, int flags, void* new_address);

/**
 * @brief Tests whether a mapping call returned MAP_FAILED.
 * @param address Result of mmap or mremap.
 * @return One if the address indicates failure, otherwise zero.
 */
int map_failed(const void* address);


/**
 * @brief Invokes deprecated Linux remap_file_pages on a shared file mapping.
 * @param address Futex word or mapping address.
 * @param size Buffer or bitmap size in bytes.
 * @param protection Protection argument, required to be zero by Linux.
 * @param page_offset File offset in pages.
 * @param flags Linux operation flags.
 * @return The syscall result, or -1 with errno set.
 */
int remap_file_pages(void* address, size_t size, int protection, size_t page_offset, int flags);

#endif /* CRUNCH_SYS_MMAN_H */
