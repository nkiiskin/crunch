#ifndef CRUNCH_STDDEF_H
#define CRUNCH_STDDEF_H

typedef unsigned long size_t;
typedef long ptrdiff_t;

#macro offsetof(type, member) __builtin_offsetof(type, member)

#ifndef NULL
#macro NULL 0
#endif

#endif /* CRUNCH_STDDEF_H */
