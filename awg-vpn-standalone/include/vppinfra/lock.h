/*
 * Minimal VPP locking compatibility shim
 */

#ifndef __included_vppinfra_lock_h__
#define __included_vppinfra_lock_h__

#include <vppinfra/types.h>
#include <pthread.h>

/* Read-write lock wrapper */
typedef struct
{
  pthread_rwlock_t lock;
} clib_rwlock_t;

static_always_inline void
clib_rwlock_init (clib_rwlock_t *p)
{
  pthread_rwlock_init (&p->lock, NULL);
}

static_always_inline void
clib_rwlock_free (clib_rwlock_t *p)
{
  pthread_rwlock_destroy (&p->lock);
}

static_always_inline void
clib_rwlock_reader_lock (clib_rwlock_t *p)
{
  pthread_rwlock_rdlock (&p->lock);
}

static_always_inline void
clib_rwlock_reader_unlock (clib_rwlock_t *p)
{
  pthread_rwlock_unlock (&p->lock);
}

static_always_inline void
clib_rwlock_writer_lock (clib_rwlock_t *p)
{
  pthread_rwlock_wrlock (&p->lock);
}

static_always_inline void
clib_rwlock_writer_unlock (clib_rwlock_t *p)
{
  pthread_rwlock_unlock (&p->lock);
}

/* Spinlock wrapper (use mutex for simplicity) */
typedef struct
{
  pthread_mutex_t lock;
} clib_spinlock_t;

static_always_inline void
clib_spinlock_init (clib_spinlock_t *p)
{
  pthread_mutex_init (&p->lock, NULL);
}

static_always_inline void
clib_spinlock_free (clib_spinlock_t *p)
{
  pthread_mutex_destroy (&p->lock);
}

static_always_inline void
clib_spinlock_lock (clib_spinlock_t *p)
{
  pthread_mutex_lock (&p->lock);
}

static_always_inline void
clib_spinlock_unlock (clib_spinlock_t *p)
{
  pthread_mutex_unlock (&p->lock);
}

#endif /* __included_vppinfra_lock_h__ */
