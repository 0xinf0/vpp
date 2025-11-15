/*
 * Minimal VPP crypto engine compatibility shim
 * WireGuard uses OpenSSL directly, so this is mostly stubs
 */

#ifndef __included_vnet_crypto_h__
#define __included_vnet_crypto_h__

#include <vppinfra/types.h>
#include <vlib/vlib.h>

/* Crypto operation types */
typedef enum
{
  VNET_CRYPTO_OP_NONE = 0,
  VNET_CRYPTO_OP_CHACHA20_POLY1305_ENC,
  VNET_CRYPTO_OP_CHACHA20_POLY1305_DEC,
} vnet_crypto_op_id_t;

/* Crypto algorithm types */
typedef enum
{
  VNET_CRYPTO_ALG_NONE = 0,
  VNET_CRYPTO_ALG_CHACHA20_POLY1305,
} vnet_crypto_alg_t;

/* Crypto engine stubs */
typedef u32 vnet_crypto_key_index_t;

/* Crypto key structure (minimal stub) */
typedef struct
{
  u8 *data;
  u32 len;
} vnet_crypto_key_t;

/* Global key storage (simple array for now) */
extern vnet_crypto_key_t _vnet_crypto_keys[256];
extern u32 _vnet_crypto_key_count;

/* Stub functions for crypto key management */
static_always_inline vnet_crypto_key_index_t
vnet_crypto_key_add (vlib_main_t *vm, vnet_crypto_alg_t alg, u8 *data,
		      u16 length)
{
  (void) vm;
  (void) alg;

  if (_vnet_crypto_key_count >= 256)
    return ~0;

  u32 idx = _vnet_crypto_key_count++;
  _vnet_crypto_keys[idx].data = data;
  _vnet_crypto_keys[idx].len = length;
  return idx;
}

static_always_inline vnet_crypto_key_t *
vnet_crypto_get_key (vnet_crypto_key_index_t index)
{
  if (index >= 256)
    return NULL;
  return &_vnet_crypto_keys[index];
}

static_always_inline void
vnet_crypto_key_update (vlib_main_t *vm, vnet_crypto_key_index_t index)
{
  (void) vm;
  (void) index;
  /* Stub - key already updated in place */
}

static_always_inline void
vnet_crypto_key_del (vlib_main_t *vm, vnet_crypto_key_index_t index)
{
  (void) vm;
  if (index < 256)
    {
      _vnet_crypto_keys[index].data = NULL;
      _vnet_crypto_keys[index].len = 0;
    }
}

#endif /* __included_vnet_crypto_h__ */
