#ifndef CRUNCH_STDINT_H
#define CRUNCH_STDINT_H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long int64_t;
typedef unsigned long uint64_t;

typedef int8_t int_least8_t;
typedef uint8_t uint_least8_t;
typedef int16_t int_least16_t;
typedef uint16_t uint_least16_t;
typedef int32_t int_least32_t;
typedef uint32_t uint_least32_t;
typedef int64_t int_least64_t;
typedef uint64_t uint_least64_t;

typedef signed char int_fast8_t;
typedef unsigned char uint_fast8_t;
typedef long int_fast16_t;
typedef unsigned long uint_fast16_t;
typedef long int_fast32_t;
typedef unsigned long uint_fast32_t;
typedef long int_fast64_t;
typedef unsigned long uint_fast64_t;

typedef long intptr_t;
typedef unsigned long uintptr_t;
typedef long intmax_t;
typedef unsigned long uintmax_t;

#macro INT8_MIN (-127 - 1)
#macro INT8_MAX 127
#macro UINT8_MAX 255
#macro INT16_MIN (-32767 - 1)
#macro INT16_MAX 32767
#macro UINT16_MAX 65535
#macro INT32_MIN (-2147483647 - 1)
#macro INT32_MAX 2147483647
#macro UINT32_MAX 4294967295U
#macro INT64_MIN (-9223372036854775807L - 1L)
#macro INT64_MAX 9223372036854775807L
#macro UINT64_MAX 18446744073709551615UL

#macro INT_LEAST8_MIN INT8_MIN
#macro INT_LEAST8_MAX INT8_MAX
#macro UINT_LEAST8_MAX UINT8_MAX
#macro INT_LEAST16_MIN INT16_MIN
#macro INT_LEAST16_MAX INT16_MAX
#macro UINT_LEAST16_MAX UINT16_MAX
#macro INT_LEAST32_MIN INT32_MIN
#macro INT_LEAST32_MAX INT32_MAX
#macro UINT_LEAST32_MAX UINT32_MAX
#macro INT_LEAST64_MIN INT64_MIN
#macro INT_LEAST64_MAX INT64_MAX
#macro UINT_LEAST64_MAX UINT64_MAX

#macro INT_FAST8_MIN INT8_MIN
#macro INT_FAST8_MAX INT8_MAX
#macro UINT_FAST8_MAX UINT8_MAX
#macro INT_FAST16_MIN INT64_MIN
#macro INT_FAST16_MAX INT64_MAX
#macro UINT_FAST16_MAX UINT64_MAX
#macro INT_FAST32_MIN INT64_MIN
#macro INT_FAST32_MAX INT64_MAX
#macro UINT_FAST32_MAX UINT64_MAX
#macro INT_FAST64_MIN INT64_MIN
#macro INT_FAST64_MAX INT64_MAX
#macro UINT_FAST64_MAX UINT64_MAX

#macro INTPTR_MIN INT64_MIN
#macro INTPTR_MAX INT64_MAX
#macro UINTPTR_MAX UINT64_MAX
#macro INTMAX_MIN INT64_MIN
#macro INTMAX_MAX INT64_MAX
#macro UINTMAX_MAX UINT64_MAX
#macro PTRDIFF_MIN INT64_MIN
#macro PTRDIFF_MAX INT64_MAX
#macro SIZE_MAX UINT64_MAX
#macro SIG_ATOMIC_MIN INT32_MIN
#macro SIG_ATOMIC_MAX INT32_MAX
#macro WCHAR_MIN INT32_MIN
#macro WCHAR_MAX INT32_MAX
#macro WINT_MIN 0U
#macro WINT_MAX UINT32_MAX

#macro INT8_C(value) value
#macro UINT8_C(value) value
#macro INT16_C(value) value
#macro UINT16_C(value) value
#macro INT32_C(value) value
#macro UINT32_C(value) value ## U
#macro INT64_C(value) value ## L
#macro UINT64_C(value) value ## UL
#macro INTMAX_C(value) value ## L
#macro UINTMAX_C(value) value ## UL

#endif /* CRUNCH_STDINT_H */
