/*
 * Minimal VPP crypto engine compatibility shim
 * WireGuard uses OpenSSL directly, so this is mostly stubs
 */

#ifndef __included_vnet_crypto_h__
#define __included_vnet_crypto_h__

#include <vppinfra/types.h>
#include <vlib/vlib.h>

/* Crypto operation types (not used by WireGuard, which uses OpenSSL directly) */
typedef enum
{
  VNET_CRYPTO_OP_NONE = 0,
} vnet_crypto_op_id_t;

/* Crypto engine stubs */
typedef u32 vnet_crypto_key_index_t;

/* Stub functions for crypto key management */
static_always_inline vnet_crypto_key_index_t
vnet_crypto_key_add (vlib_main_t *vm, void *alg, u8 *data, u16 length)
{
  (void) vm;
  (void) alg;
  (void) data;
  (void) length;
  return 0; /* Stub - not used in standalone */
}

static_always_inline void
vnet_crypto_key_del (vlib_main_t *vm, vnet_crypto_key_index_t index)
{
  (void) vm;
  (void) index;
  /* Stub - not used in standalone */
}

#endif /* __included_vnet_crypto_h__ */
