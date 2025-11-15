/*
 * Minimal VPP string compatibility shim
 */

#ifndef __included_vppinfra_string_h__
#define __included_vppinfra_string_h__

#include <vppinfra/types.h>
#include <string.h>

/* String operations - mostly passthrough to standard C */
#define clib_memcpy_fast(dst, src, n) memcpy((dst), (src), (n))
#define clib_memset_u8(s, c, n) memset((s), (c), (n))
#define clib_memset_u16(s, c, n) \
  do { \
    u16 *_p = (u16 *)(s); \
    for (uword _i = 0; _i < (n); _i++) \
      _p[_i] = (c); \
  } while (0)

#define clib_memcpy(dst, src, n) memcpy((dst), (src), (n))
#define clib_memset(s, c, n) memset((s), (c), (n))
#define clib_strcmp(s1, s2) strcmp((s1), (s2))
#define clib_strncmp(s1, s2, n) strncmp((s1), (s2), (n))
#define clib_strlen(s) strlen((s))

#endif /* __included_vppinfra_string_h__ */
