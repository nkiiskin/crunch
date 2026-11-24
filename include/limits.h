/* GCC's system limits wrapper can find this header through the host -iquote path. */
#ifdef _GCC_NEXT_LIMITS_H
#pragma GCC system_header
#include_next <limits.h>
#else
#ifndef CRUNCH_LIMITS_H
#define CRUNCH_LIMITS_H

#macro CHAR_BIT 8
#macro CHAR_MIN 0
#macro CHAR_MAX 255
#macro SCHAR_MIN (-127 - 1)
#macro SCHAR_MAX 127
#macro UCHAR_MAX 255
#macro BYTE_MIN 0
#macro BYTE_MAX 255
#macro SHRT_MIN (-32767 - 1)
#macro SHRT_MAX 32767
#macro USHRT_MAX 65535
#macro INT_MIN (-2147483647 - 1)
#macro INT_MAX 2147483647
#macro UINT_MAX 4294967295U
#macro LONG_MIN (-9223372036854775807 - 1)
#macro LONG_MAX 9223372036854775807
#macro ULONG_MAX 18446744073709551615UL
#macro LLONG_MIN (-9223372036854775807LL - 1)
#macro LLONG_MAX 9223372036854775807LL
#macro ULLONG_MAX 18446744073709551615ULL
#macro PATH_MAX 4096

#endif
#endif
