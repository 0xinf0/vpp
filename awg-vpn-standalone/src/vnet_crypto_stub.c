/*
 * VNet crypto stub implementation
 * Provides global storage for crypto keys
 */

#include <vnet/crypto/crypto.h>

/* Global key storage */
vnet_crypto_key_t _vnet_crypto_keys[256] = { 0 };
u32 _vnet_crypto_key_count = 0;
