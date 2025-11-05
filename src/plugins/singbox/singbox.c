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

/**
 * @file
 * @brief Sing-box Integration Plugin - Main implementation
 *
 * This plugin integrates VPP with sing-box proxy platform.
 * It provides APIs and CLI commands to redirect traffic to
 * a sing-box instance for advanced protocol handling.
 */

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <singbox/singbox.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>

#include <singbox/singbox.api_enum.h>
#include <singbox/singbox.api_types.h>

#define REPLY_MSG_ID_BASE sm->msg_id_base
#include <vlibapi/api_helper_macros.h>

/* Plugin registration */
VLIB_PLUGIN_REGISTER () = {
  .version = SINGBOX_PLUGIN_BUILD_VER,
  .description = "Sing-box Integration Plugin - Universal Proxy Platform Support",
};

singbox_main_t singbox_main;

/**
 * @brief Enable/disable sing-box on an interface
 */
int
singbox_enable_disable (singbox_main_t *sm, u32 sw_if_index,
                       int enable_disable, ip4_address_t *proxy_addr,
                       u16 proxy_port)
{
  vnet_sw_interface_t *sw;

  /* Validate interface index */
  if (pool_is_free_index (sm->vnet_main->interface_main.sw_interfaces,
                         sw_if_index))
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  /* Get interface */
  sw = vnet_get_sw_interface (sm->vnet_main, sw_if_index);
  if (sw->type != VNET_SW_INTERFACE_TYPE_HARDWARE)
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  /* Ensure interface config vector is large enough */
  vec_validate_init_empty (sm->interface_configs, sw_if_index,
                           (singbox_interface_config_t){ 0 });

  singbox_interface_config_t *config = &sm->interface_configs[sw_if_index];

  if (enable_disable)
    {
      /* Enable sing-box on this interface */
      config->endpoint.is_enabled = 1;

      if (proxy_addr && proxy_port)
        {
          clib_memcpy (&config->endpoint.proxy_addr, proxy_addr,
                      sizeof (ip4_address_t));
          config->endpoint.proxy_port = proxy_port;
        }
      else
        {
          /* Use default endpoint */
          clib_memcpy (&config->endpoint, &sm->default_endpoint,
                      sizeof (singbox_endpoint_t));
        }

      /* Enable feature on this interface */
      vnet_feature_enable_disable ("ip4-unicast", "singbox",
                                  sw_if_index, enable_disable, 0, 0);
    }
  else
    {
      /* Disable sing-box on this interface */
      config->endpoint.is_enabled = 0;
      vnet_feature_enable_disable ("ip4-unicast", "singbox",
                                  sw_if_index, enable_disable, 0, 0);
    }

  return 0;
}

/**
 * @brief Set global sing-box endpoint
 */
int
singbox_set_endpoint (singbox_main_t *sm, ip4_address_t *proxy_addr,
                     u16 proxy_port, u8 protocol_type)
{
  if (!proxy_addr || proxy_port == 0)
    return VNET_API_ERROR_INVALID_VALUE;

  clib_memcpy (&sm->default_endpoint.proxy_addr, proxy_addr,
              sizeof (ip4_address_t));
  sm->default_endpoint.proxy_port = proxy_port;
  sm->default_endpoint.protocol_type = protocol_type;
  sm->default_endpoint.is_enabled = 1;

  return 0;
}

/**
 * @brief CLI command: singbox enable/disable
 */
static clib_error_t *
singbox_enable_disable_command_fn (vlib_main_t *vm,
                                   unformat_input_t *input,
                                   vlib_cli_command_t *cmd)
{
  singbox_main_t *sm = &singbox_main;
  u32 sw_if_index = ~0;
  int enable_disable = 1;
  ip4_address_t proxy_addr = { 0 };
  u16 proxy_port = 0;
  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "disable"))
        enable_disable = 0;
      else if (unformat (input, "%U", unformat_vnet_sw_interface,
                        sm->vnet_main, &sw_if_index))
        ;
      else if (unformat (input, "proxy %U:%d", unformat_ip4_address,
                        &proxy_addr, &proxy_port))
        ;
      else
        break;
    }

  if (sw_if_index == ~0)
    return clib_error_return (0, "Please specify an interface...");

  rv = singbox_enable_disable (sm, sw_if_index, enable_disable,
                               proxy_port ? &proxy_addr : NULL, proxy_port);

  switch (rv)
    {
    case 0:
      break;

    case VNET_API_ERROR_INVALID_SW_IF_INDEX:
      return clib_error_return (0,
                               "Invalid interface, only works on physical ports");

    case VNET_API_ERROR_INVALID_VALUE:
      return clib_error_return (0, "Invalid proxy address or port");

    default:
      return clib_error_return (0, "singbox_enable_disable returned %d", rv);
    }

  return 0;
}

/**
 * @brief CLI command: set sing-box endpoint
 */
static clib_error_t *
singbox_set_endpoint_command_fn (vlib_main_t *vm,
                                 unformat_input_t *input,
                                 vlib_cli_command_t *cmd)
{
  singbox_main_t *sm = &singbox_main;
  ip4_address_t proxy_addr = { 0 };
  u16 proxy_port = 0;
  u8 protocol_type = 0; /* Default: SOCKS5 */
  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "%U:%d", unformat_ip4_address, &proxy_addr,
                   &proxy_port))
        ;
      else if (unformat (input, "socks5"))
        protocol_type = 0;
      else if (unformat (input, "http"))
        protocol_type = 1;
      else
        break;
    }

  if (proxy_port == 0)
    return clib_error_return (0, "Please specify proxy address and port");

  rv = singbox_set_endpoint (sm, &proxy_addr, proxy_port, protocol_type);

  if (rv != 0)
    return clib_error_return (0, "Failed to set endpoint: %d", rv);

  vlib_cli_output (vm, "Sing-box endpoint set to %U:%d (protocol: %s)",
                  format_ip4_address, &proxy_addr, proxy_port,
                  protocol_type == 0 ? "SOCKS5" : "HTTP");

  return 0;
}

/**
 * @brief CLI command: show sing-box statistics
 */
static clib_error_t *
singbox_show_stats_command_fn (vlib_main_t *vm,
                               unformat_input_t *input,
                               vlib_cli_command_t *cmd)
{
  singbox_main_t *sm = &singbox_main;
  u32 sw_if_index = ~0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "%U", unformat_vnet_sw_interface,
                   sm->vnet_main, &sw_if_index))
        ;
      else
        break;
    }

  if (sw_if_index != ~0)
    {
      /* Show stats for specific interface */
      singbox_interface_config_t *config =
        singbox_get_interface_config (sm, sw_if_index);

      if (!config || !config->endpoint.is_enabled)
        {
          vlib_cli_output (vm, "Sing-box not enabled on interface %d",
                          sw_if_index);
          return 0;
        }

      vlib_cli_output (vm, "Interface %U:", format_vnet_sw_if_index_name,
                      sm->vnet_main, sw_if_index);
      vlib_cli_output (vm, "  Proxy: %U:%d", format_ip4_address,
                      &config->endpoint.proxy_addr,
                      config->endpoint.proxy_port);
      vlib_cli_output (vm, "  Packets redirected: %llu",
                      config->packets_redirected);
      vlib_cli_output (vm, "  Bytes redirected: %llu",
                      config->bytes_redirected);
      vlib_cli_output (vm, "  Connection failures: %llu",
                      config->connection_failures);
    }
  else
    {
      /* Show global stats */
      vlib_cli_output (vm, "Sing-box Global Configuration:");
      vlib_cli_output (vm, "  Default endpoint: %U:%d",
                      format_ip4_address, &sm->default_endpoint.proxy_addr,
                      sm->default_endpoint.proxy_port);
      vlib_cli_output (vm, "  Protocol: %s",
                      sm->default_endpoint.protocol_type == 0 ? "SOCKS5" : "HTTP");
      vlib_cli_output (vm, "\nPer-Interface Statistics:");

      vec_foreach_index (sw_if_index, sm->interface_configs)
        {
          singbox_interface_config_t *config = &sm->interface_configs[sw_if_index];
          if (config->endpoint.is_enabled)
            {
              vlib_cli_output (vm, "  Interface %U:",
                             format_vnet_sw_if_index_name,
                             sm->vnet_main, sw_if_index);
              vlib_cli_output (vm, "    Packets: %llu, Bytes: %llu, Failures: %llu",
                             config->packets_redirected,
                             config->bytes_redirected,
                             config->connection_failures);
            }
        }
    }

  return 0;
}

/* CLI command definitions */
VLIB_CLI_COMMAND (singbox_enable_disable_command, static) = {
  .path = "singbox enable",
  .short_help = "singbox enable <interface> [proxy <ip>:<port>] [disable]",
  .function = singbox_enable_disable_command_fn,
};

VLIB_CLI_COMMAND (singbox_set_endpoint_command, static) = {
  .path = "singbox set endpoint",
  .short_help = "singbox set endpoint <ip>:<port> [socks5|http]",
  .function = singbox_set_endpoint_command_fn,
};

VLIB_CLI_COMMAND (singbox_show_stats_command, static) = {
  .path = "show singbox",
  .short_help = "show singbox [<interface>]",
  .function = singbox_show_stats_command_fn,
};

/* API message handlers */

static void
vl_api_singbox_enable_disable_t_handler (vl_api_singbox_enable_disable_t *mp)
{
  vl_api_singbox_enable_disable_reply_t *rmp;
  singbox_main_t *sm = &singbox_main;
  int rv;
  ip4_address_t proxy_addr;

  clib_memcpy (&proxy_addr, &mp->proxy_addr, sizeof (ip4_address_t));

  rv = singbox_enable_disable (sm, ntohl (mp->sw_if_index),
                              (int) (mp->enable_disable), &proxy_addr,
                              ntohs (mp->proxy_port));

  REPLY_MACRO (VL_API_SINGBOX_ENABLE_DISABLE_REPLY);
}

static void
vl_api_singbox_set_endpoint_t_handler (vl_api_singbox_set_endpoint_t *mp)
{
  vl_api_singbox_set_endpoint_reply_t *rmp;
  singbox_main_t *sm = &singbox_main;
  int rv;
  ip4_address_t proxy_addr;

  clib_memcpy (&proxy_addr, &mp->proxy_addr, sizeof (ip4_address_t));

  rv = singbox_set_endpoint (sm, &proxy_addr, ntohs (mp->proxy_port),
                            mp->protocol_type);

  REPLY_MACRO (VL_API_SINGBOX_SET_ENDPOINT_REPLY);
}

static void
vl_api_singbox_get_stats_t_handler (vl_api_singbox_get_stats_t *mp)
{
  singbox_main_t *sm = &singbox_main;
  vl_api_singbox_get_stats_reply_t *rmp;
  int rv = 0;
  u32 sw_if_index = ntohl (mp->sw_if_index);
  singbox_interface_config_t *config;

  config = singbox_get_interface_config (sm, sw_if_index);

  REPLY_MACRO2 (VL_API_SINGBOX_GET_STATS_REPLY,
  ({
    if (config && config->endpoint.is_enabled)
      {
        rmp->packets_redirected = clib_host_to_net_u64 (config->packets_redirected);
        rmp->bytes_redirected = clib_host_to_net_u64 (config->bytes_redirected);
        rmp->connection_failures = clib_host_to_net_u64 (config->connection_failures);
      }
    else
      {
        rv = VNET_API_ERROR_INVALID_SW_IF_INDEX;
      }
  }));
}

/* API definitions */
#include <singbox/singbox.api.c>

/**
 * @brief Plugin initialization function
 */
static clib_error_t *
singbox_init (vlib_main_t *vm)
{
  singbox_main_t *sm = &singbox_main;

  sm->vnet_main = vnet_get_main ();
  sm->vlib_main = vm;

  /* Initialize default endpoint to zeros */
  clib_memset (&sm->default_endpoint, 0, sizeof (singbox_endpoint_t));

  /* Add our API messages to the global name_crc hash table */
  sm->msg_id_base = setup_message_id_table ();

  return 0;
}

VLIB_INIT_FUNCTION (singbox_init);

/**
 * @brief Hook the sing-box plugin into the VPP graph hierarchy
 */
VNET_FEATURE_INIT (singbox, static) = {
  .arc_name = "ip4-unicast",
  .node_name = "singbox",
  .runs_before = VNET_FEATURES ("ip4-lookup"),
};
