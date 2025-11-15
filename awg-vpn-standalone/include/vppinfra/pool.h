/*
 * Minimal VPP pool compatibility shim
 * Pools are indexed arrays with free list
 */

#ifndef __included_vppinfra_pool_h__
#define __included_vppinfra_pool_h__

#include <vppinfra/types.h>
#include <vppinfra/vec.h>
#include <vppinfra/mem.h>

/* Pool header - similar to vec, but tracks free indices */
typedef struct
{
  u32 len;
  u32 capacity;
  u32 *free_indices;
} pool_header_t;

#define _pool_hdr(v) ((pool_header_t *)((u8 *)(v) - sizeof(pool_header_t)))
#define _pool_len(v) ((v) ? _pool_hdr(v)->len : 0)

/* Get pool length (number of active elements) */
#define pool_len(p) _pool_len(p)

/* Get element at index */
#define pool_elt_at_index(p, i) ((p) + (i))

/* Check if element is free */
#define pool_is_free_index(p, i) (0)  /* Simplified - no free list for now */

/* Allocate element from pool */
#define pool_get(P, E) \
  do { \
    if ((P) == NULL || vec_len((P)) >= _pool_hdr(P)->capacity) { \
      uword _new_cap = (P) ? _pool_hdr(P)->capacity * 2 : 16; \
      pool_header_t *_h; \
      if ((P) == NULL) { \
        _h = malloc(sizeof(pool_header_t) + _new_cap * sizeof((P)[0])); \
        _h->len = 0; \
        _h->capacity = _new_cap; \
        _h->free_indices = NULL; \
        (P) = (void *)((u8 *)_h + sizeof(pool_header_t)); \
      } else { \
        _h = _pool_hdr(P); \
        _h = realloc(_h, sizeof(pool_header_t) + _new_cap * sizeof((P)[0])); \
        _h->capacity = _new_cap; \
        (P) = (void *)((u8 *)_h + sizeof(pool_header_t)); \
      } \
    } \
    (E) = &(P)[_pool_hdr(P)->len++]; \
    memset((E), 0, sizeof(*(E))); \
  } while (0)

/* Free element back to pool */
#define pool_put(P, E) \
  do { \
    /* Simplified - just zero it out */ \
    memset((E), 0, sizeof(*(E))); \
  } while (0)

/* Free entire pool */
#define pool_free(P) \
  do { \
    if (P) { \
      pool_header_t *_h = _pool_hdr(P); \
      vec_free(_h->free_indices); \
      free(_h); \
      (P) = NULL; \
    } \
  } while (0)

/* Iterate over pool */
#define pool_foreach(VAR, POOL) \
  for ((VAR) = (POOL); (VAR) < (POOL) + pool_len(POOL); (VAR)++)

/* Pool element count */
#define pool_elts(P) pool_len(P)

#endif /* __included_vppinfra_pool_h__ */
