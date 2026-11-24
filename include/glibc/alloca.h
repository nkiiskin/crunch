#ifndef CRUNCH_GLIBC_ALLOCA_H
#define CRUNCH_GLIBC_ALLOCA_H

#include_next <alloca.h>
#undef alloca
#define alloca(size) __builtin_alloca(size)

#endif
