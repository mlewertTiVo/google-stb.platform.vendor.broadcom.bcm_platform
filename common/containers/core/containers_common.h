#ifndef VC_CONTAINERS_COMMON_H
#define VC_CONTAINERS_COMMON_H

/** \file containers_common.h
 * Common definitions for containers infrastructure
 */

#ifndef ENABLE_CONTAINERS_STANDALONE
# include "vcos.h"
# define vc_container_assert(a) vcos_assert(a)
#else
# include "assert.h"
# define vc_container_assert(a) assert(a)
#endif /* ENABLE_CONTAINERS_STANDALONE */

#ifndef countof
# define countof(a) (sizeof(a) / sizeof(a[0]))
#endif

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef _MSC_VER
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif

#define STATIC_INLINE static __inline
#define VC_CONTAINER_PARAM_UNUSED(a) (void)(a)

#if defined(__HIGHC__) && !defined(strcasecmp)
# define strcasecmp(a,b) _stricmp(a,b)
#endif

#if defined(__GNUC__) && (__GNUC__ > 2)
# define VC_CONTAINER_CONSTRUCTOR(func) void __attribute__((constructor,used)) func(void)
# define VC_CONTAINER_DESTRUCTOR(func) void __attribute__((destructor,used)) func(void)
#else
# define VC_CONTAINER_CONSTRUCTOR(func) void func(void)
# define VC_CONTAINER_DESTRUCTOR(func) void func(void)
#endif

#endif /* VC_CONTAINERS_COMMON_H */
