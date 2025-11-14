/*
 * Minimal VPP types compatibility shim for standalone AWG VPN
 * Copyright (c) 2025 Standalone AWG VPN Project
 */

#ifndef __included_vppinfra_types_h__
#define __included_vppinfra_types_h__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* VPP basic types */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef size_t uword;
typedef ptrdiff_t word;

/* Index type - used for pool/vector indices */
typedef u32 index_t;
#define INDEX_INVALID ((index_t)~0)

/* Walk return code - used for iteration callbacks */
typedef enum walk_rc_t_
{
  WALK_STOP,
  WALK_CONTINUE,
} walk_rc_t;

/* always_inline macro */
#ifndef always_inline
#define always_inline inline __attribute__ ((__always_inline__))
#endif

#ifndef static_always_inline
#define static_always_inline static inline __attribute__ ((__always_inline__))
#endif

/* unused attribute */
#ifndef __clib_unused
#define __clib_unused __attribute__ ((unused))
#endif

/* packed attribute */
#ifndef __clib_packed
#define __clib_packed __attribute__ ((packed))
#endif

#endif /* __included_vppinfra_types_h__ */
