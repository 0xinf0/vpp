/*
 * Minimal VPP clib utility functions
 */

#ifndef __included_clib_h__
#define __included_clib_h__

#include <vppinfra/types.h>

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
