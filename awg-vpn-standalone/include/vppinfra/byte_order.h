/*
 * Minimal VPP byte order compatibility shim
 */

#ifndef __included_vppinfra_byte_order_h__
#define __included_vppinfra_byte_order_h__

#include <vppinfra/types.h>
#include <endian.h>

/* Detect architecture endianness */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define CLIB_ARCH_IS_LITTLE_ENDIAN 1
#define CLIB_ARCH_IS_BIG_ENDIAN 0
#else
#define CLIB_ARCH_IS_LITTLE_ENDIAN 0
#define CLIB_ARCH_IS_BIG_ENDIAN 1
#endif

/* Byte swap functions */
static_always_inline u16
clib_byte_swap_u16 (u16 x)
{
  return __builtin_bswap16 (x);
}

static_always_inline u32
clib_byte_swap_u32 (u32 x)
{
  return __builtin_bswap32 (x);
}

static_always_inline u64
clib_byte_swap_u64 (u64 x)
{
  return __builtin_bswap64 (x);
}

/* Host to network / network to host conversions */
#if CLIB_ARCH_IS_LITTLE_ENDIAN
#define clib_host_to_net_u16(x) clib_byte_swap_u16(x)
#define clib_host_to_net_u32(x) clib_byte_swap_u32(x)
#define clib_host_to_net_u64(x) clib_byte_swap_u64(x)
#define clib_net_to_host_u16(x) clib_byte_swap_u16(x)
#define clib_net_to_host_u32(x) clib_byte_swap_u32(x)
#define clib_net_to_host_u64(x) clib_byte_swap_u64(x)
#else
#define clib_host_to_net_u16(x) (x)
#define clib_host_to_net_u32(x) (x)
#define clib_host_to_net_u64(x) (x)
#define clib_net_to_host_u16(x) (x)
#define clib_net_to_host_u32(x) (x)
#define clib_net_to_host_u64(x) (x)
#endif

/* Little endian conversions */
#if CLIB_ARCH_IS_LITTLE_ENDIAN
#define clib_host_to_little_u16(x) (x)
#define clib_host_to_little_u32(x) (x)
#define clib_host_to_little_u64(x) (x)
#define clib_little_to_host_u16(x) (x)
#define clib_little_to_host_u32(x) (x)
#define clib_little_to_host_u64(x) (x)
#else
#define clib_host_to_little_u16(x) clib_byte_swap_u16(x)
#define clib_host_to_little_u32(x) clib_byte_swap_u32(x)
#define clib_host_to_little_u64(x) clib_byte_swap_u64(x)
#define clib_little_to_host_u16(x) clib_byte_swap_u16(x)
#define clib_little_to_host_u32(x) clib_byte_swap_u32(x)
#define clib_little_to_host_u64(x) clib_byte_swap_u64(x)
#endif

/* Big endian conversions (swap on little endian) */
#if CLIB_ARCH_IS_LITTLE_ENDIAN
#define clib_host_to_big_u32(x) clib_byte_swap_u32(x)
#define clib_host_to_big_u64(x) clib_byte_swap_u64(x)
#define clib_big_to_host_u32(x) clib_byte_swap_u32(x)
#define clib_big_to_host_u64(x) clib_byte_swap_u64(x)
#else
#define clib_host_to_big_u32(x) (x)
#define clib_host_to_big_u64(x) (x)
#define clib_big_to_host_u32(x) (x)
#define clib_big_to_host_u64(x) (x)
#endif

#endif /* __included_vppinfra_byte_order_h__ */
