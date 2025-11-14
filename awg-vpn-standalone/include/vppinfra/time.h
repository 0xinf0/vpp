/*
 * Minimal VPP time functions compatibility shim
 */

#ifndef __included_vppinfra_time_h__
#define __included_vppinfra_time_h__

#include <vppinfra/types.h>
#include <time.h>
#include <sys/time.h>

/* Unix timestamp in seconds */
static_always_inline f64
unix_time_now (void)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return (f64) tv.tv_sec + (f64) tv.tv_usec * 1e-6;
}

/* Monotonic time in seconds (for intervals) */
static_always_inline f64
vlib_time_now (void *vm)
{
  (void) vm; /* unused */
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (f64) ts.tv_sec + (f64) ts.tv_nsec * 1e-9;
}

#endif /* __included_vppinfra_time_h__ */
