/*
 * Minimal VPP memory management compatibility shim
 */

#ifndef __included_vppinfra_mem_h__
#define __included_vppinfra_mem_h__

#include <vppinfra/types.h>
#include <stdlib.h>
#include <string.h>

/* Memory allocation wrappers */
static_always_inline void *
clib_mem_alloc (uword size)
{
  return malloc (size);
}

static_always_inline void *
clib_mem_alloc_aligned (uword size, uword align)
{
  void *ptr;
  if (posix_memalign (&ptr, align, size) == 0)
    return ptr;
  return NULL;
}

static_always_inline void
clib_mem_free (void *p)
{
  free (p);
}

/* Memory operations */
#define clib_memcpy(dst, src, n) memcpy((dst), (src), (n))
#define clib_memset(s, c, n) memset((s), (c), (n))
#define clib_memcmp(s1, s2, n) memcmp((s1), (s2), (n))

/* Prefetch hints (no-op in standalone) */
#define CLIB_PREFETCH(addr, size, type) __builtin_prefetch(addr)

#endif /* __included_vppinfra_mem_h__ */
