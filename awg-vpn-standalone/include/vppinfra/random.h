/*
 * Minimal VPP random number generation compatibility shim
 */

#ifndef __included_vppinfra_random_h__
#define __included_vppinfra_random_h__

#include <vppinfra/types.h>

/* Simple xorshift64* PRNG - fast and good quality */
static_always_inline u32
random_u32 (u64 *seed)
{
  u64 x = *seed;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  *seed = x;
  return (u32) ((x * 0x2545F4914F6CDD1DULL) >> 32);
}

static_always_inline u64
random_u64 (u64 *seed)
{
  u32 a = random_u32 (seed);
  u32 b = random_u32 (seed);
  return ((u64) a << 32) | (u64) b;
}

/* Initialize seed from system time */
static_always_inline u64
random_default_seed (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (u64) ts.tv_sec ^ ((u64) ts.tv_nsec << 32);
}

#endif /* __included_vppinfra_random_h__ */
