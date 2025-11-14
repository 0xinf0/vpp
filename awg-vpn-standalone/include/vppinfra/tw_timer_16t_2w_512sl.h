/*
 * Timing Wheel Timer stub for standalone AWG VPN
 *
 * This is a minimal stub to enable compilation. Real timer functionality
 * needs to be implemented using POSIX timers (timerfd, epoll, or kqueue).
 *
 * TODO: Replace with proper POSIX timer implementation
 */

#ifndef __included_tw_timer_16t_2w_512sl_h__
#define __included_tw_timer_16t_2w_512sl_h__

#include <vppinfra/types.h>

/* Timer wheel name suffix */
#define TW(a) a##_16t_2w_512sl

/* Timer wheel structure - stubbed */
typedef struct
{
  u32 dummy;
} tw_timer_wheel_16t_2w_512sl_t;

/* Timer handle */
typedef u32 tw_timer_handle_16t_2w_512sl_t;

/* Invalid timer handle */
#define TW_TIMER_HANDLE_INVALID ((tw_timer_handle_16t_2w_512sl_t) ~0)

/* Initialize timer wheel (stub) */
static_always_inline void
tw_timer_wheel_init_16t_2w_512sl (tw_timer_wheel_16t_2w_512sl_t *tw,
				  void *expired_timer_callback,
				  f64 timer_interval, u32 max_expirations)
{
  (void) tw;
  (void) expired_timer_callback;
  (void) timer_interval;
  (void) max_expirations;
  /* Stub - no initialization needed */
}

/* Free timer wheel (stub) */
static_always_inline void
tw_timer_wheel_free_16t_2w_512sl (tw_timer_wheel_16t_2w_512sl_t *tw)
{
  (void) tw;
  /* Stub - no cleanup needed */
}

/* Start a timer (stub - returns invalid handle) */
static_always_inline tw_timer_handle_16t_2w_512sl_t
tw_timer_start_16t_2w_512sl (tw_timer_wheel_16t_2w_512sl_t *tw, u32 user_id,
			      u32 timer_id, u64 interval)
{
  (void) tw;
  (void) user_id;
  (void) timer_id;
  (void) interval;
  /* Stub - return invalid handle */
  return TW_TIMER_HANDLE_INVALID;
}

/* Stop a timer (stub) */
static_always_inline void
tw_timer_stop_16t_2w_512sl (tw_timer_wheel_16t_2w_512sl_t *tw,
			     tw_timer_handle_16t_2w_512sl_t handle)
{
  (void) tw;
  (void) handle;
  /* Stub - no-op */
}

/* Update a timer (stub) */
static_always_inline void
tw_timer_update_16t_2w_512sl (tw_timer_wheel_16t_2w_512sl_t *tw,
			       tw_timer_handle_16t_2w_512sl_t handle,
			       u64 interval)
{
  (void) tw;
  (void) handle;
  (void) interval;
  /* Stub - no-op */
}

/* Expire timers (stub - returns 0) */
static_always_inline u32
tw_timer_expire_timers_16t_2w_512sl (tw_timer_wheel_16t_2w_512sl_t *tw,
				      f64 now)
{
  (void) tw;
  (void) now;
  /* Stub - no timers expired */
  return 0;
}

#endif /* __included_tw_timer_16t_2w_512sl_h__ */
