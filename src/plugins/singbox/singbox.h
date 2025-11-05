/*
 * Copyright (c) 2025 Internet Mastering & Company
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
#include <vppinfra/hash.h>
#include <vppinfra/error.h>
#include <vppinfra/elog.h>

#define SINGBOX_PLUGIN_BUILD_VER "1.0.0"

/**
 * @file
 * @brief Sing-box Integration Plugin
 *
 * This plugin provides integration with sing-box (https://github.com/SagerNet/sing-box),
 * a universal proxy platform. It allows VPP to redirect traffic to a sing-box instance
 * for protocol handling (SOCKS5, VMess, Trojan, Shadowsocks, etc.).
 */

/**
 * @brief Sing-box proxy endpoint configuration
 */
typedef struct
{
  /** Proxy server IPv4 address */
  ip4_address_t proxy_addr;

  /** Proxy server port (typically 1080 for SOCKS5) */
  u16 proxy_port;

  /** Protocol type (0=SOCKS5, 1=HTTP, future expansion) */
  u8 protocol_type;

  /** Enable/disable flag */
  u8 is_enabled;
} singbox_endpoint_t;

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
} singbox_main_t;

/** Global instance of sing-box plugin */
extern singbox_main_t singbox_main;

/** Graph node registration */
extern vlib_node_registration_t singbox_node;

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

#endif /* __included_singbox_h__ */
