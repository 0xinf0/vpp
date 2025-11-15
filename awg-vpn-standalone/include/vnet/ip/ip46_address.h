/*
 * IP address type compatibility shim
 */

#ifndef __included_ip46_address_h__
#define __included_ip46_address_h__

#include <vppinfra/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* IP4 address */
typedef struct
{
  union
  {
    u8 as_u8[4];
    u32 as_u32;
  };
} ip4_address_t;

/* IP6 address */
typedef struct
{
  union
  {
    u8 as_u8[16];
    u16 as_u16[8];
    u32 as_u32[4];
    u64 as_u64[2];
  };
} ip6_address_t;

/* Combined IP4/IP6 address */
typedef struct
{
  union
  {
    ip4_address_t ip4;
    ip6_address_t ip6;
  };
} ip46_address_t;

/* IP address family type */
typedef enum
{
  IP46_TYPE_ANY,
  IP46_TYPE_IP4,
  IP46_TYPE_IP6,
} ip46_type_t;

/* Full IP address with family */
typedef struct
{
  ip46_address_t ip;
  u8 version; /* 4 or 6 */
} ip_address_t;

/* IP address operations */
#define ip46_address_is_ip4(a) ((a)->version == 4)
#define ip46_address_is_ip6(a) ((a)->version == 6)

#endif /* __included_ip46_address_h__ */
