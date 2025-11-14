/*
 * Minimal VPP vector compatibility shim
 * Implements essential vec_* macros for dynamic arrays
 */

#ifndef __included_vppinfra_vec_h__
#define __included_vppinfra_vec_h__

#include <vppinfra/types.h>
#include <vppinfra/mem.h>
#include <string.h>
#include <stdlib.h>

/* Vector header - hidden before user data */
typedef struct
{
  u32 len;      /* Number of elements */
  u32 capacity; /* Allocated capacity */
} vec_header_t;

#define _vec_hdr(v) ((vec_header_t *)((u8 *)(v) - sizeof(vec_header_t)))
#define _vec_len(v) ((v) ? _vec_hdr(v)->len : 0)
#define _vec_capacity(v) ((v) ? _vec_hdr(v)->capacity : 0)

/* Get vector length */
#define vec_len(v) _vec_len(v)

/* Resize vector (internal) */
#define _vec_resize(V, N, ELTSIZE) \
  do { \
    uword _new_cap = (N); \
    if ((V) == NULL) { \
      vec_header_t *_h = malloc(sizeof(vec_header_t) + (_new_cap) * (ELTSIZE)); \
      _h->len = 0; \
      _h->capacity = _new_cap; \
      (V) = (void *)((u8 *)_h + sizeof(vec_header_t)); \
    } else { \
      vec_header_t *_h = _vec_hdr(V); \
      _h = realloc(_h, sizeof(vec_header_t) + (_new_cap) * (ELTSIZE)); \
      _h->capacity = _new_cap; \
      (V) = (void *)((u8 *)_h + sizeof(vec_header_t)); \
    } \
  } while (0)

/* Add 1 element to vector */
#define vec_add1(V, E) \
  do { \
    if ((V) == NULL || _vec_len(V) >= _vec_capacity(V)) { \
      uword _new_cap = (V) ? _vec_capacity(V) * 2 : 4; \
      _vec_resize(V, _new_cap, sizeof((V)[0])); \
    } \
    (V)[_vec_hdr(V)->len++] = (E); \
  } while (0)

/* Add N elements to vector */
#define vec_add2(V, P, N) \
  do { \
    uword _n = (N); \
    uword _old_len = vec_len(V); \
    if ((V) == NULL || (_old_len + _n) > _vec_capacity(V)) { \
      uword _new_cap = (V) ? _vec_capacity(V) * 2 : 4; \
      while (_new_cap < (_old_len + _n)) _new_cap *= 2; \
      _vec_resize(V, _new_cap, sizeof((V)[0])); \
    } \
    _vec_hdr(V)->len += _n; \
    (P) = (V) + _old_len; \
  } while (0)

/* Validate vector index */
#define vec_validate(V, I) \
  do { \
    uword _i = (I); \
    if (_i >= vec_len(V)) { \
      uword _new_len = _i + 1; \
      if ((V) == NULL || _new_len > _vec_capacity(V)) { \
        uword _new_cap = (V) ? _vec_capacity(V) : 4; \
        while (_new_cap < _new_len) _new_cap *= 2; \
        _vec_resize(V, _new_cap, sizeof((V)[0])); \
      } \
      memset((V) + vec_len(V), 0, (_new_len - vec_len(V)) * sizeof((V)[0])); \
      _vec_hdr(V)->len = _new_len; \
    } \
  } while (0)

/* Free vector */
#define vec_free(V) \
  do { \
    if (V) { \
      free(_vec_hdr(V)); \
      (V) = NULL; \
    } \
  } while (0)

/* Reset vector length to zero but keep allocation */
#define vec_reset_length(V) \
  do { \
    if (V) _vec_hdr(V)->len = 0; \
  } while (0)

/* Iterate over vector */
#define vec_foreach(VAR, VEC) \
  for ((VAR) = (VEC); (VAR) < (VEC) + vec_len(VEC); (VAR)++)

/* Get number of elements */
#define vec_elts(V) vec_len(V)

/* Duplicate vector */
#define vec_dup(V) \
  ({ \
    typeof(V) _v = NULL; \
    if (V) { \
      uword _len = vec_len(V); \
      _vec_resize(_v, _len, sizeof((V)[0])); \
      memcpy(_v, V, _len * sizeof((V)[0])); \
      _vec_hdr(_v)->len = _len; \
    } \
    _v; \
  })

#endif /* __included_vppinfra_vec_h__ */
