# HYBRID MEMORY ALLOCATOR
## Design Specification and Architecture
### x86-64 Linux / GNU AT&T Assembly

Version: 1.0
Architecture: x86-64
Operating system: Linux
Assembler: GNU as (AT&T syntax)
Calling convention: System V AMD64 ABI
Memory management: mmap(), munmap(), mremap()

======================================================================
1. INTRODUCTION
======================================================================

The objective is to implement a small, efficient memory allocator in
x86-64 Linux assembly language.

The allocator uses two different allocation strategies:

1. SMALL ALLOCATIONS:
   A pool allocator using fixed-size blocks, organized into size
   classes and managed through free lists.

2. LARGE ALLOCATIONS:
   Individual memory mappings obtained through mmap(), with resizing
   supported through mremap().

The allocator exposes the following four C-compatible interfaces:

    void *malloc(size_t size);

    void free(void *ptr);

    void *realloc(void *ptr, size_t size);

    void *mremap(void *old_address,
                 size_t old_size,
                 size_t new_size,
                 int flags, ...);

The first three functions implement the standard C allocation
interfaces.

The fourth function implements the Linux-specific mremap() interface.

All four functions are written in x86-64 GNU AT&T assembly.

======================================================================
2. GENERAL ARCHITECTURE
======================================================================

The allocator uses a hybrid architecture.

Small allocations are handled by the pool allocator.

Large allocations are handled by the mapping allocator.

                        malloc(size)
                             |
                             |
                    Determine allocation
                           size
                             |
                             |
                  +----------+----------+
                  |                     |
                  |                     |
             SMALL SIZE             LARGE SIZE
                <= 256                 > 256
                  |                     |
                  |                     |
                  v                     v
           POOL ALLOCATOR        MAPPING ALLOCATOR
                  |                     |
                  |                     |
             Existing arena          mmap()
                  |                     |
                  |                     |
             Free-list block       Individual mapping
                  |                     |
                  +----------+----------+
                             |
                             |
                        Return ptr
                             |
                             |
                      User application


The public free() and realloc() functions must determine which
allocation mechanism owns the supplied pointer.

The allocation metadata provides this information.

======================================================================
3. ALLOCATION STRATEGIES
======================================================================

3.1 SMALL ALLOCATIONS

Allocations of up to 256 bytes are handled by the pool allocator.

Instead of creating a separate memory mapping for every allocation,
the allocator obtains larger memory regions from the Linux kernel.

These regions are called ARENAS.

Each arena is divided into blocks of a fixed size.

Blocks belonging to the same size class have identical sizes.

For example:

    Requested allocation: 40 bytes

    Selected size class: 64 bytes

    Actual block size: 64 bytes


3.2 LARGE ALLOCATIONS

Allocations exceeding 256 bytes are handled individually.

The allocator obtains memory through:

    mmap()

The resulting mapping is managed independently.

Resizing is performed through:

    mremap()

Deallocation is performed through:

    munmap()


======================================================================
4. SIZE CLASSES
======================================================================

The pool allocator uses fixed-size allocation classes.

The initial proposed size classes are:

    +------------+-------------+----------------------+
    | Class      | Block size  | Requested size       |
    +------------+-------------+----------------------+
    | 0          | 16 bytes    | 1 - 16 bytes         |
    | 1          | 32 bytes    | 17 - 32 bytes        |
    | 2          | 48 bytes    | 33 - 48 bytes        |
    | 3          | 64 bytes    | 49 - 64 bytes        |
    | 4          | 80 bytes    | 65 - 80 bytes        |
    | 5          | 96 bytes    | 81 - 96 bytes        |
    | 6          | 112 bytes   | 97 - 112 bytes       |
    | 7          | 128 bytes   | 113 - 128 bytes     |
    | 8          | 160 bytes   | 129 - 160 bytes     |
    | 9          | 192 bytes   | 161 - 192 bytes     |
    | 10         | 224 bytes   | 193 - 224 bytes     |
    | 11         | 256 bytes   | 225 - 256 bytes     |
    +------------+-------------+----------------------+

All block sizes are multiples of 16 bytes.

This ensures 16-byte alignment for returned user pointers.

The allocator selects the smallest size class that can accommodate
the requested allocation plus any per-block metadata overhead.

The exact block layout must account for that overhead.


======================================================================
5. ARENA ORGANIZATION
======================================================================

The initial proposed arena size is 64 KiB.

An arena contains multiple blocks belonging to one size class.

Each arena maintains information about its blocks and their
availability.

Conceptual arena structure:

    +----------------------------------+
    | ARENA HEADER                     |
    +----------------------------------+
    | Arena identification / magic    |
    +----------------------------------+
    | Size class                       |
    +----------------------------------+
    | Block size                        |
    +----------------------------------+
    | Number of blocks                  |
    +----------------------------------+
    | Number of free blocks             |
    +----------------------------------+
    | Free-list head                    |
    +----------------------------------+
    | Bitmap / allocation information   |
    +----------------------------------+
    | Next arena                        |
    +----------------------------------+
    |                                  |
    | BLOCK 0                          |
    +----------------------------------+
    | BLOCK 1                          |
    +----------------------------------+
    | BLOCK 2                          |
    +----------------------------------+
    | BLOCK 3                          |
    +----------------------------------+
    |                                  |
    | ...                              |
    |                                  |
    +----------------------------------+
    | LAST BLOCK                       |
    +----------------------------------+

Arenas are obtained through mmap().

When an arena runs out of available blocks, the allocator obtains
another arena of the same size class.

When an arena becomes completely empty, it may be returned to the
kernel using munmap().

An arena must not be released while any of its blocks are allocated.


======================================================================
6. FREE-LIST ORGANIZATION
======================================================================

Each size class maintains its own free list.

The free list contains pointers to currently unused blocks.

Example:

    FREE LIST FOR SIZE CLASS 3 (64 BYTES)


       Free-list head
             |
             v
        +---------+
        | BLOCK 1 |
        +---------+
             |
             v
        +---------+
        | BLOCK 2 |
        +---------+
             |
             v
        +---------+
        | BLOCK 3 |
        +---------+
             |
             v
            NULL


When malloc() requests a block:

    1. Determine the appropriate size class.

    2. Check the corresponding free list.

    3. Remove one block from the free list.

    4. Mark the block as allocated.

    5. Return its user-data pointer.


When free() releases a block:

    1. Identify its owning arena.

    2. Identify its size class.

    3. Mark the block as free.

    4. Insert it into the appropriate free list.

The first 8 bytes of a free block can be used to store the pointer
to the next free block.

This avoids additional memory allocations for free-list nodes.


======================================================================
7. ALLOCATION METADATA
======================================================================

The allocator must identify the origin and characteristics of every
allocation.

A common metadata format is proposed for the initial design.

Conceptual 32-byte allocation header:

    Offset       Contents
    ----------------------------------------
    -32          Allocation type
    -24          Requested allocation size
    -16          Arena pointer or mapping length
    -8           Block capacity or other metadata
    ----------------------------------------
    0            User memory begins


The allocation type identifies the allocation mechanism.

For example:

    TYPE_POOL   = 1

    TYPE_MMAP   = 2


For small allocations:

    -32          TYPE_POOL
    -24          Requested allocation size
    -16          Pointer to owning arena
    -8           Block capacity

For large allocations:

    -32          TYPE_MMAP
    -24          Requested allocation size
    -16          Mapping length
    -8           Reserved metadata


The exact header representation must be finalized before assembly
implementation.

The user pointer must remain 16-byte aligned.

The metadata must also allow free() and realloc() to identify the
correct allocation mechanism without searching every arena.


======================================================================
8. malloc() IMPLEMENTATION
======================================================================

C prototype:

    void *malloc(size_t size);

Header:

    #include <stdlib.h>

Algorithm:

    malloc(size):

        IF size is zero:
            Apply the allocator's zero-size policy.

        IF size <= 256:
            Select appropriate size class.

            Find arena with available blocks.

            IF no suitable arena exists:
                Create new arena using mmap().

            Remove block from free list.

            Initialize allocation metadata.

            Return user pointer.

        ELSE:
            Calculate required mapping length.

            Create mapping using mmap().

            Initialize allocation metadata.

            Return user pointer.


======================================================================
9. free() IMPLEMENTATION
======================================================================

C prototype:

    void free(void *ptr);

Header:

    #include <stdlib.h>

Algorithm:

    free(ptr):

        IF ptr is NULL:
            Return.

        Read allocation metadata.

        IF allocation type is TYPE_POOL:

            Identify owning arena.

            Return block to corresponding free list.

            Update arena allocation information.

            IF arena becomes completely empty:
                Optionally release arena using munmap().

        ELSE IF allocation type is TYPE_MMAP:

            Recover original mapping address.

            Read mapping length.

            Release mapping using munmap().

        Return.


======================================================================
10. realloc() IMPLEMENTATION
======================================================================

C prototype:

    void *realloc(void *ptr, size_t size);

Header:

    #include <stdlib.h>

The realloc() function must support both allocation mechanisms.

SMALL ALLOCATIONS:

    IF ptr belongs to a pool:

        IF new size fits existing block:
            Update requested-size metadata.
            Return original pointer.

        ELSE:
            Allocate a new block.

            Copy min(old_size, new_size) bytes.

            Free original block.

            Return new pointer.


LARGE ALLOCATIONS:

    IF ptr belongs to a mapping:

        IF new size fits existing capacity:
            Update requested-size metadata.
            Return original pointer.

        ELSE:
            Attempt resizing using mremap().

            IF successful:
                Update metadata.
                Return new pointer.

            IF unsuccessful:
                Original allocation remains valid.
                Return NULL.


SPECIAL CASES:

    realloc(NULL, size):

        Equivalent to malloc(size).


    realloc(ptr, 0):

        Use the allocator's explicitly chosen zero-size policy.


======================================================================
11. mremap() IMPLEMENTATION
======================================================================

C prototype:

    #define _GNU_SOURCE

    #include <sys/mman.h>

    void *mremap(void *old_address,
                 size_t old_size,
                 size_t new_size,
                 int flags, ...);


mremap() is a Linux-specific interface.

It is used for resizing existing memory mappings.

The principal flag is:

    MREMAP_MAYMOVE = 1

This allows Linux to move a mapping when it cannot be resized
at its existing address.

The allocator uses mremap() for large allocations.

Example:

    realloc(ptr, 10000)

    1. Read original mapping address.

    2. Read original mapping length.

    3. Calculate new mapping length.

    4. Call mremap() with MREMAP_MAYMOVE.

    5. If successful:
       Update allocation metadata.

    6. Return new user pointer.


IMPORTANT:

The mremap() system call operates on the original mapping address,
not the user pointer.

If the allocation header is 32 bytes:

    mapping_address = user_pointer - 32


The original mapping length and new mapping length must be supplied
to mremap().


======================================================================
12. ALLOCATION PATH DIAGRAM
======================================================================

                    malloc(size)
                         |
                         v
                 Determine size class
                         |
                         v
                 Is size <= 256?
                    /       \
                  YES        NO
                   |          |
                   v          v
             POOL ALLOC.   mmap()
                   |          |
                   v          v
             Find arena    Initialize
                   |       metadata
                   v          |
             Free block       |
                   |          |
                   v          |
             Mark allocated   |
                   |          |
                   +-----+----+
                         |
                         v
                    Return ptr


======================================================================
13. DEALLOCATION PATH DIAGRAM
======================================================================

                     free(ptr)
                         |
                         v
                 Read allocation type
                         |
                    +----+----+
                    |         |
                    v         v
                  POOL      MMAP
                    |         |
                    v         v
               Find arena  Read mapping
                    |        length
                    v         |
               Return block  |
               to free list  |
                    |         |
                    v         v
               Update arena munmap()
                    |         |
                    +----+----+
                         |
                         v
                       Return


======================================================================
14. REALLOCATION PATH DIAGRAM
======================================================================

                   realloc(ptr, size)
                          |
                          v
                    Is ptr NULL?
                     /       \
                   YES        NO
                    |          |
                    v          v
                  malloc   Read metadata
                               |
                               v
                       Identify allocator
                               |
                         +-----+-----+
                         |           |
                         v           v
                       POOL        MMAP
                         |           |
                         v           v
                  Fits existing?  Fits existing?
                    /     \         /     \
                  YES      NO     YES      NO
                   |        |      |        |
                   v        v      v        v
                Update   Allocate Update   mremap()
                header   new block header      |
                   |        |      |            v
                   |        v      |         Success?
                   |      Copy old |          /    \
                   |      contents |         YES    NO
                   |        |      |          |      |
                   |        v      |          v      v
                   |      Free old |       Update  Return
                   |        |      |       header  NULL
                   |        |      |          |
                   +--------+------+----------+
                            |
                            v
                       Return ptr


======================================================================
15. INITIAL CONFIGURATION
======================================================================

The proposed initial configuration is:

    ARENA_SIZE              = 65536 bytes (64 KiB)

    SMALL_ALLOCATION_LIMIT  = 256 bytes

    MEMORY_ALIGNMENT       = 16 bytes

    ALLOCATION_HEADER_SIZE = 32 bytes

    NUMBER_OF_SIZE_CLASSES = 12

    LARGE_ALLOCATION      = mmap()

    LARGE_RESIZE          = mremap()

    LARGE_DEALLOCATION    = munmap()

    SMALL_ALLOCATION     = Free-list pool allocator

    SMALL_DEALLOCATION   = Return block to free list


These values are initial design parameters and may be changed
during optimization and benchmarking.


======================================================================
16. INTERNAL ROUTINES
======================================================================

The following routines are internal implementation functions.

They do not replace the public C interfaces.

    arena_create()

        Creates a new arena using mmap().

    arena_destroy()

        Releases an empty arena using munmap().

    pool_alloc()

        Allocates a block from a size-class free list.

    pool_free()

        Returns a block to its size-class free list.

    size_class_select()

        Determines the smallest suitable size class.

    arena_find()

        Finds an arena with available blocks.

    arena_initialize()

        Initializes arena metadata and free lists.

    allocation_type()

        Identifies whether a pointer belongs to the pool allocator
        or the individual mapping allocator.


======================================================================
17. IMPLEMENTATION SEQUENCE
======================================================================

The allocator should be implemented incrementally.

PHASE 1: ARENA MANAGEMENT

    Implement arena_create().

    Implement arena_initialize().

    Implement arena_destroy().

    Test mmap() and munmap() handling.


PHASE 2: SIZE CLASSES

    Implement size_class_select().

    Define the fixed block sizes.

    Calculate block counts and arena layouts.


PHASE 3: FREE-LIST MANAGEMENT

    Implement pool_alloc().

    Implement pool_free().

    Test allocation and reuse of individual blocks.


PHASE 4: malloc() INTEGRATION

    Route small allocations through the pool allocator.

    Route large allocations through mmap().

    Test all size classes.


PHASE 5: free() INTEGRATION

    Identify allocation type.

    Return pool allocations to free lists.

    Release large mappings through munmap().


PHASE 6: realloc() INTEGRATION

    Implement resizing within the same block.

    Implement movement between size classes.

    Implement large-allocation resizing using mremap().


PHASE 7: TESTING AND OPTIMIZATION

    Test allocation reuse.

    Test arena exhaustion.

    Test arena expansion.

    Test realloc() data preservation.

    Test large mapping relocation.

    Test release of empty arenas.

    Benchmark against the previous allocator.


======================================================================
18. FUTURE IMPROVEMENTS
======================================================================

Once the basic allocator works correctly, several improvements
can be introduced.

1. MULTITHREADING

   Add synchronization to protect arena metadata and free lists.

   Eventually introduce per-thread arenas to reduce contention.

2. BITMAP ALLOCATION

   Use allocation bitmaps for efficient block allocation and
   ownership tracking.

3. EMPTY ARENA RECLAMATION

   Return completely unused arenas to the kernel.

4. ADVANCED SIZE CLASSES

   Introduce more size classes or tune their sizes based on
   allocation statistics.

5. CALLOC()

   Implement zero-initialized allocations.

6. ALIGNED ALLOCATION

   Support aligned_alloc() and posix_memalign().

7. DEBUGGING

   Add detection of invalid pointers, double frees and memory
   corruption.

8. PERFORMANCE BENCHMARKING

   Compare the allocator against glibc malloc() under different
   allocation patterns.


======================================================================
19. IMPORTANT DESIGN CONSIDERATIONS
======================================================================

The allocator must maintain the following invariants:

    1. Every returned pointer must satisfy the required alignment.

    2. Every allocated block must have valid metadata.

    3. Every pool block must belong to exactly one arena.

    4. Every free pool block must appear exactly once in its
       free list.

    5. No allocated block may be returned to the free list.

    6. munmap() must receive the original mapping address and
       the correct mapping length.

    7. mremap() must operate on the original mapping address,
       not the user pointer.

    8. If realloc() fails, the original allocation must remain
       valid and unchanged.

    9. Pool allocations must never be passed directly to
       munmap() or mremap().

   10. Large allocations must never be returned to a pool free list.

   11. The allocator must not mix its pointers with pointers
       managed by glibc's allocator.

   12. Metadata must be sufficiently protected against accidental
       corruption and invalid pointers.


======================================================================
20. FINAL ARCHITECTURE
======================================================================

The intended final allocator consists of:

    PUBLIC C INTERFACES
    -------------------

        malloc()
        free()
        realloc()
        mremap()


    INTERNAL ALLOCATION ROUTINES
    ----------------------------

        arena_create()
        arena_destroy()
        arena_initialize()
        arena_find()

        pool_alloc()
        pool_free()

        size_class_select()
        allocation_type()


    KERNEL INTERFACES
    -----------------

        mmap()
        munmap()
        mremap()


    SMALL ALLOCATION MANAGEMENT
    ---------------------------

        Arenas
        Size classes
        Fixed-size blocks
        Free lists
        Allocation metadata


    LARGE ALLOCATION MANAGEMENT
    ---------------------------

        Individual memory mappings
        Allocation metadata
        mremap() resizing
        munmap() deallocation


The resulting architecture is a hybrid memory allocator combining
the efficiency of pool allocation for small objects with the
flexibility of Linux memory mappings for large objects.

The design is intended to provide a foundation for a general-purpose
memory allocator written entirely in x86-64 GNU AT&T assembly.

======================================================================
END OF DOCUMENT
======================================================================
