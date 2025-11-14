/*
 * Minimal vlib compatibility shim for standalone AWG VPN
 * Provides only what WireGuard/AWG needs
 */

#ifndef __included_vlib_h__
#define __included_vlib_h__

#include <vppinfra/types.h>
#include <vppinfra/format.h>
#include <vppinfra/time.h>
#include <vppinfra/vec.h>
#include <vppinfra/mem.h>
#include <vppinfra/pool.h>
#include <vppinfra/lock.h>
#include <vppinfra/clib.h>

/* Forward declarations */
typedef struct vlib_main_t vlib_main_t;
typedef struct vlib_node_runtime_t vlib_node_runtime_t;
typedef struct vlib_frame_t vlib_frame_t;
typedef struct vlib_buffer_t vlib_buffer_t;

/* Minimal vlib_main_t - just enough for time functions */
struct vlib_main_t
{
  f64 clib_time_now;
};

/* Stub vlib_main for time queries */
static vlib_main_t _vlib_main_stub = { 0 };

/* Buffer structure - simplified for standalone */
struct vlib_buffer_t
{
  u32 current_data;
  u16 current_length;
  u8 *data;
  u32 flags;
};

/* Buffer allocation - stub implementation */
static_always_inline vlib_buffer_t *
vlib_get_buffer (vlib_main_t *vm, u32 buffer_index)
{
  (void) vm;
  (void) buffer_index;
  /* Not used in crypto code - would need real implementation for dataplane */
  return NULL;
}

/* ASSERT macro for debugging */
#ifdef DEBUG
#define ASSERT(expr) \
  do { \
    if (!(expr)) { \
      fprintf(stderr, "ASSERT failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
      abort(); \
    } \
  } while (0)
#else
#define ASSERT(expr) do {} while (0)
#endif

#endif /* __included_vlib_h__ */
