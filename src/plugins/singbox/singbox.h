/*
 * Copyright (c) 2025 Internet Mastering & Company, Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __included_singbox_h__
#define __included_singbox_h__

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/ip/ip4.h>
#include <vnet/ip/ip6.h>
#include <vnet/tcp/tcp.h>
#include <vnet/session/session.h>
#include <vnet/session/application.h>
#include <vnet/session/application_interface.h>
#include <vppinfra/hash.h>
#include <vppinfra/error.h>
#include <vppinfra/elog.h>
#include <vppinfra/pool.h>
#include <vppinfra/bihash_16_8.h>

#define SINGBOX_PLUGIN_BUILD_VER "2.0.0"

/**
 * @file
 * @brief Sing-box Integration Plugin - Production Grade
 *
 * This plugin provides production-grade integration with sing-box
 * (https://github.com/SagerNet/sing-box), a universal proxy platform.
 * It uses VPP's session layer to manage connections to sing-box and
 * properly handles SOCKS5 protocol encapsulation/decapsulation.
 */

/* SOCKS5 Protocol Definitions */
#define SOCKS5_VERSION 0x05
#define SOCKS5_AUTH_NONE 0x00
#define SOCKS5_AUTH_USERNAME_PASSWORD 0x02
#define SOCKS5_CMD_CONNECT 0x01
#define SOCKS5_CMD_BIND 0x02
#define SOCKS5_CMD_UDP_ASSOCIATE 0x03
#define SOCKS5_ATYP_IPV4 0x01
#define SOCKS5_ATYP_DOMAINNAME 0x03
#define SOCKS5_ATYP_IPV6 0x04
#define SOCKS5_REP_SUCCESS 0x00

/* Connection States */
#define foreach_singbox_session_state     \
  _(IDLE, "idle")                        \
  _(CONNECTING, "connecting")            \
  _(SOCKS5_GREETING, "socks5-greeting")  \
  _(SOCKS5_AUTH, "socks5-auth")          \
  _(SOCKS5_REQUEST, "socks5-request")    \
  _(SOCKS5_RESPONSE, "socks5-response")  \
  _(ESTABLISHED, "established")          \
  _(ERROR, "error")                      \
  _(CLOSED, "closed")

typedef enum
{
#define _(v, s) SINGBOX_STATE_##v,
  foreach_singbox_session_state
#undef _
  SINGBOX_N_STATES,
} singbox_session_state_t;

/**
 * @brief Sing-box proxy endpoint configuration
 */
typedef struct
{
  /** Proxy server IPv4 address */
  ip4_address_t proxy_addr;

  /** Proxy server port (typically 1080 for SOCKS5) */
  u16 proxy_port;

  /** Protocol type (0=SOCKS5, 1=HTTP) */
  u8 protocol_type;

  /** Enable/disable flag */
  u8 is_enabled;

  /** Authentication method */
  u8 auth_method;

  /** Username for authentication */
  u8 username[256];

  /** Password for authentication */
  u8 password[256];

  /** Username length */
  u8 username_len;

  /** Password length */
  u8 password_len;

  /** Connection timeout (seconds) */
  u32 timeout;

  /** Max concurrent connections */
  u32 max_connections;
} singbox_endpoint_t;

/**
 * @brief Connection session to sing-box proxy
 */
typedef struct
{
  CLIB_CACHE_LINE_ALIGN_MARK (cacheline0);

  /** Session handle to proxy */
  session_handle_t proxy_session_handle;

  /** Original client session handle */
  session_handle_t client_session_handle;

  /** Current connection state */
  singbox_session_state_t state;

  /** Original destination IP */
  ip4_address_t dst_addr;

  /** Original destination port */
  u16 dst_port;

  /** Endpoint configuration */
  singbox_endpoint_t *endpoint;

  /** Timestamp of last activity */
  f64 last_activity;

  /** Number of bytes sent */
  u64 bytes_sent;

  /** Number of bytes received */
  u64 bytes_received;

  /** Error count */
  u32 error_count;

  /** Session index */
  u32 session_index;

  /** Interface index */
  u32 sw_if_index;

  /** Flags */
  u8 is_connected;
  u8 greeting_sent;
  u8 auth_sent;
  u8 request_sent;

  /** Buffers for protocol data */
  u8 *tx_buffer;
  u8 *rx_buffer;

} singbox_session_t;

/**
 * @brief Per-interface sing-box configuration
 */
typedef struct
{
  /** Endpoint configuration */
  singbox_endpoint_t endpoint;

  /** Statistics: packets redirected */
  u64 packets_redirected;

  /** Statistics: bytes redirected */
  u64 bytes_redirected;

  /** Statistics: connection failures */
  u64 connection_failures;

  /** Statistics: active connections */
  u32 active_connections;

  /** Statistics: total connections */
  u64 total_connections;

  /** Connection pool */
  u32 *connection_pool;

  /** Free connection indices */
  u32 *free_indices;

} singbox_interface_config_t;

/**
 * @brief Main sing-box plugin runtime structure
 */
typedef struct
{
  /** API message ID base */
  u16 msg_id_base;

  /** Per-interface configuration, indexed by sw_if_index */
  singbox_interface_config_t *interface_configs;

  /** Global enable flag */
  u8 global_enable;

  /** Default proxy endpoint when per-interface not configured */
  singbox_endpoint_t default_endpoint;

  /** Convenience - cached vnet main pointer */
  vnet_main_t *vnet_main;

  /** Convenience - cached vlib main pointer */
  vlib_main_t *vlib_main;

  /** Session pool */
  singbox_session_t *sessions;

  /** Session hash table: client_session_handle -> session_index */
  uword *session_by_client_handle;

  /** Session hash table: proxy_session_handle -> session_index */
  uword *session_by_proxy_handle;

  /** Application index for session layer */
  u32 app_index;

  /** Worker thread index */
  u32 *wrk_index;

  /** Per-worker session pools */
  singbox_session_t **wrk_sessions;

  /** Lock for session management */
  clib_spinlock_t sessions_lock;

  /** Session timeout (seconds) */
  u32 session_timeout;

  /** Connection retry count */
  u32 max_retries;

  /** Enable connection pooling */
  u8 enable_pooling;

  /** Enable verbose logging */
  u8 verbose;

} singbox_main_t;

/** Global instance of sing-box plugin */
extern singbox_main_t singbox_main;

/** Graph node registration */
extern vlib_node_registration_t singbox_node;
extern vlib_node_registration_t singbox_punt_node;
extern vlib_node_registration_t singbox_inject_node;

/**
 * @brief Enable/disable sing-box on an interface
 *
 * @param sm - sing-box main structure
 * @param sw_if_index - software interface index
 * @param enable_disable - 1 to enable, 0 to disable
 * @param proxy_addr - IPv4 address of sing-box proxy
 * @param proxy_port - Port of sing-box proxy
 * @return 0 on success, error code otherwise
 */
int singbox_enable_disable (singbox_main_t *sm, u32 sw_if_index,
                            int enable_disable, ip4_address_t *proxy_addr,
                            u16 proxy_port);

/**
 * @brief Set global sing-box endpoint
 *
 * @param sm - sing-box main structure
 * @param proxy_addr - IPv4 address of sing-box proxy
 * @param proxy_port - Port of sing-box proxy
 * @param protocol_type - Protocol type (0=SOCKS5)
 * @return 0 on success, error code otherwise
 */
int singbox_set_endpoint (singbox_main_t *sm, ip4_address_t *proxy_addr,
                          u16 proxy_port, u8 protocol_type);

/**
 * @brief Create a new session to sing-box proxy
 *
 * @param sm - sing-box main structure
 * @param dst_addr - Destination address
 * @param dst_port - Destination port
 * @param sw_if_index - Interface index
 * @return session pointer or NULL on failure
 */
singbox_session_t *singbox_session_create (singbox_main_t *sm,
                                           ip4_address_t *dst_addr,
                                           u16 dst_port, u32 sw_if_index);

/**
 * @brief Delete a sing-box session
 *
 * @param sm - sing-box main structure
 * @param session - Session to delete
 */
void singbox_session_delete (singbox_main_t *sm, singbox_session_t *session);

/**
 * @brief Get session by client handle
 *
 * @param sm - sing-box main structure
 * @param client_handle - Client session handle
 * @return session pointer or NULL
 */
singbox_session_t *singbox_session_get_by_client (singbox_main_t *sm,
                                                  session_handle_t client_handle);

/**
 * @brief Get session by proxy handle
 *
 * @param sm - sing-box main structure
 * @param proxy_handle - Proxy session handle
 * @return session pointer or NULL
 */
singbox_session_t *singbox_session_get_by_proxy (singbox_main_t *sm,
                                                 session_handle_t proxy_handle);

/**
 * @brief Send SOCKS5 greeting
 *
 * @param sm - sing-box main structure
 * @param session - Session
 * @return 0 on success, error code otherwise
 */
int singbox_socks5_send_greeting (singbox_main_t *sm,
                                  singbox_session_t *session);

/**
 * @brief Send SOCKS5 auth request
 *
 * @param sm - sing-box main structure
 * @param session - Session
 * @return 0 on success, error code otherwise
 */
int singbox_socks5_send_auth (singbox_main_t *sm, singbox_session_t *session);

/**
 * @brief Send SOCKS5 connect request
 *
 * @param sm - sing-box main structure
 * @param session - Session
 * @return 0 on success, error code otherwise
 */
int singbox_socks5_send_connect (singbox_main_t *sm,
                                 singbox_session_t *session);

/**
 * @brief Process SOCKS5 response
 *
 * @param sm - sing-box main structure
 * @param session - Session
 * @param data - Response data
 * @param len - Data length
 * @return 0 on success, error code otherwise
 */
int singbox_socks5_process_response (singbox_main_t *sm,
                                     singbox_session_t *session, u8 *data,
                                     u32 len);

/**
 * @brief Get interface configuration
 *
 * @param sm - sing-box main structure
 * @param sw_if_index - software interface index
 * @return pointer to interface config or NULL
 */
static inline singbox_interface_config_t *
singbox_get_interface_config (singbox_main_t *sm, u32 sw_if_index)
{
  if (sw_if_index >= vec_len (sm->interface_configs))
    return NULL;
  return &sm->interface_configs[sw_if_index];
}

/**
 * @brief Format session state
 */
format_function_t format_singbox_session_state;

/**
 * @brief Format session
 */
format_function_t format_singbox_session;

#endif /* __included_singbox_h__ */
