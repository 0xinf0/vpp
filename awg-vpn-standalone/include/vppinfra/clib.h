/*
 * Minimal VPP clib utility functions
 */

#ifndef __included_clib_h__
#define __included_clib_h__

#include <vppinfra/types.h>
#include <vppinfra/byte_order.h>
#include <assert.h>
#include <stddef.h>

/* Cache line size */
#define CLIB_CACHE_LINE_BYTES 64

/* Cache line alignment marker */
#define CLIB_CACHE_LINE_ALIGN_MARK(mark) \
  u8 mark[0] __attribute__((aligned(CLIB_CACHE_LINE_BYTES)))

/* Struct size macros */
#define STRUCT_SIZE_OF(t, f) sizeof(((t *) 0)->f)
#define STRUCT_OFFSET_OF(t, f) offsetof(t, f)

/* Static assert */
#define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)

/* Min/Max macros */
#define clib_min(a, b) ((a) < (b) ? (a) : (b))
#define clib_max(a, b) ((a) > (b) ? (a) : (b))

/* Clamp value */
#define clib_clamp(val, min, max) clib_min(clib_max((val), (min)), (max))

/* Absolute value */
#define clib_abs(x) ((x) < 0 ? -(x) : (x))

/* Count leading zeros */
static_always_inline u32
count_leading_zeros (u64 x)
{
  return (u32) __builtin_clzll (x);
}

/* Count trailing zeros */
static_always_inline u32
count_trailing_zeros (u64 x)
{
  return (u32) __builtin_ctzll (x);
}

/* Power of 2 */
static_always_inline uword
is_pow2 (uword x)
{
  return (x & (x - 1)) == 0;
}

static_always_inline uword
round_pow2 (uword x, uword pow2)
{
  return (x + pow2 - 1) & ~(pow2 - 1);
}

#endif /* __included_clib_h__ */
