#ifndef VC_CONTAINERS_TYPES_H
#define VC_CONTAINERS_TYPES_H

/** \file containers_types.h
 * Definition of types used by the containers API
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>

#if defined( __STDC__ ) && defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#include <inttypes.h>

#elif defined( __GNUC__ )
#include <stdint.h>
#include <inttypes.h>

#elif defined(_MSC_VER)
#include <stdint.h>
#if (_MSC_VER < 1700)
typedef          __int8    int8_t;
typedef unsigned __int8    uint8_t;
typedef          __int16   int16_t;
typedef unsigned __int16   uint16_t;
typedef          __int32   int32_t;
typedef unsigned __int32   uint32_t;
typedef          __int64   int64_t;
typedef unsigned __int64   uint64_t;
#endif
#define PRIu16 "u"
#define PRIu32 "u"
#define PRId64 "I64d"
#define PRIi64 "I64i"
#define PRIo64 "I64o"
#define PRIu64 "I64u"
#define PRIx64 "I64x"

#else
typedef signed   char      int8_t;
typedef unsigned char      uint8_t;
typedef signed   short     int16_t;
typedef unsigned short     uint16_t;
typedef signed   long      int32_t;
typedef unsigned long      uint32_t;
typedef signed   long long int64_t;
typedef unsigned long long uint64_t;
#endif

/* C99 64bits integers */
#ifndef INT64_C
# define INT64_C(value) value##LL
# define UINT64_C(value) value##ULL
#endif

/* C99 boolean */
#ifndef __cplusplus
#ifndef bool
# define bool int
#endif
#ifndef true
# define true 1
#endif
#ifndef false
# define false 0
#endif
#endif /* __cplusplus */

/* FIXME: should be different for big endian */
#define VC_FOURCC(a,b,c,d) ((a) | (b << 8) | (c << 16) | (d << 24))

#endif /* VC_CONTAINERS_TYPES_H */
