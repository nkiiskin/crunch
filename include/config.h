#ifndef CRUNCH_CONFIG_H
#define CRUNCH_CONFIG_H

#define CRUNCH_VERSION_MAJOR 0
#define CRUNCH_VERSION_MINOR 9
#define CRUNCH_VERSION_PATCH 0

/* Crunch language type sizes, in bytes. */
#define CRUNCH_INT_SIZE       8
#define CRUNCH_BYTE_SIZE      1
#define CRUNCH_REAL_SIZE      8
#define CRUNCH_POINTER_SIZE  8

/* Target platform: Linux on x86-64 / AMD64. */
#define CRUNCH_TARGET_X86_64  1
#define CRUNCH_TARGET_LINUX   1

/* System V AMD64 ABI stack alignment at call boundaries. */
#define CRUNCH_ABI_STACK_ALIGNMENT 16

#endif
