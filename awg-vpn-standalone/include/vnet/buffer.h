/*
 * Minimal vnet buffer stub
 */

#ifndef __included_vnet_buffer_h__
#define __included_vnet_buffer_h__

#include <vlib/vlib.h>

/* Buffer metadata - minimal stub */
typedef struct
{
  u32 sw_if_index[2]; /* RX and TX */
  u32 next_buffer;
  u32 current_data;
  u16 current_length;
  u16 flags;
} vnet_buffer_opaque_t;

/* Get buffer opaque data */
#define vnet_buffer(b) ((vnet_buffer_opaque_t *)(b))

#endif /* __included_vnet_buffer_h__ */
