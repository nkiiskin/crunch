#ifndef CRUNCH_STDARG_H
#define CRUNCH_STDARG_H

#include <__crunch_va_list.h>

typedef struct __crunch_va_list_tag va_list[1];

#macro va_start(ap, last) __crunch_va_start(ap, last)
#macro va_end(ap) __crunch_va_end(ap)
#macro va_copy(destination, source) __crunch_va_copy(destination, source)
#macro va_arg(ap, type) (0, *__crunch_va_arg((ap), (type *)0))

#endif
