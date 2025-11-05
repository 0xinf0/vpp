/*
 * Copyright (c) 2024 Internet Mastering & Company
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

#include "outline_server.h"
#include <vlibapi/api.h>
#include <vlibmemory/api.h>
#include <vnet/format_fns.h>

/* API message handler macros */
#define REPLY_MSG_ID_BASE osm->msg_id_base
#include <vlibapi/api_helper_macros.h>

#define vl_msg_name_crc_list
#include <outline_server/outline_server.api.h>
#undef vl_msg_name_crc_list

/**
 * @brief Setup message ID base
 */
static void
setup_message_id_table (outline_server_main_t *osm, api_main_t *am)
{
  VL_MSG_API_CONFIG (&osm->msg_id_base, "outline_server", VL_API_MSG_ID_MAX);
}

/**
 * @brief API handler for enable/disable
 */
static void vl_api_outline_server_enable_disable_t_handler (
  vl_api_outline_server_enable_disable_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_outline_server_enable_disable_reply_t *rmp;
  int rv = 0;
  clib_error_t *error;
  u8 *config_file = 0;

  if (mp->config_file[0])
    config_file = format (0, "%s%c", mp->config_file, 0);

  error =
    outline_server_enable_disable (mp->enable, config_file, mp->metrics_port);

  if (error)
    {
      rv = -1;
      clib_error_free (error);
    }

  if (config_file)
    vec_free (config_file);

  REPLY_MACRO (VL_API_OUTLINE_SERVER_ENABLE_DISABLE_REPLY);
}

/**
 * @brief API handler for adding a port
 */
static void
vl_api_outline_server_add_port_t_handler (vl_api_outline_server_add_port_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_outline_server_add_port_reply_t *rmp;
  int rv = 0;
  u32 port_id = 0;
  clib_error_t *error;
  u8 *password = 0;
  u8 *cipher = 0;

  password = format (0, "%s%c", mp->password, 0);

  if (mp->cipher[0])
    cipher = format (0, "%s%c", mp->cipher, 0);

  error = outline_server_add_port (mp->port, password, cipher, mp->timeout,
				    &port_id);

  if (error)
    {
      rv = -1;
      clib_error_free (error);
    }

  vec_free (password);
  if (cipher)
    vec_free (cipher);

  REPLY_MACRO2 (VL_API_OUTLINE_SERVER_ADD_PORT_REPLY,
		({ rmp->port_id = htonl (port_id); }));
}

/**
 * @brief API handler for deleting a port
 */
static void vl_api_outline_server_delete_port_t_handler (
  vl_api_outline_server_delete_port_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_outline_server_delete_port_reply_t *rmp;
  int rv = 0;
  clib_error_t *error;

  error = outline_server_delete_port (ntohl (mp->port_id));

  if (error)
    {
      rv = -1;
      clib_error_free (error);
    }

  REPLY_MACRO (VL_API_OUTLINE_SERVER_DELETE_PORT_REPLY);
}

/**
 * @brief API handler for adding a key
 */
static void
vl_api_outline_server_add_key_t_handler (vl_api_outline_server_add_key_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_outline_server_add_key_reply_t *rmp;
  int rv = 0;
  clib_error_t *error;
  u8 *key_id = 0;
  u8 *password = 0;

  key_id = format (0, "%s%c", mp->key_id, 0);
  password = format (0, "%s%c", mp->password, 0);

  error = outline_server_add_key (key_id, ntohl (mp->port_id), password,
				   clib_net_to_host_u64 (mp->data_limit));

  if (error)
    {
      rv = -1;
      clib_error_free (error);
    }

  vec_free (key_id);
  vec_free (password);

  REPLY_MACRO (VL_API_OUTLINE_SERVER_ADD_KEY_REPLY);
}

/**
 * @brief API handler for deleting a key
 */
static void vl_api_outline_server_delete_key_t_handler (
  vl_api_outline_server_delete_key_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_outline_server_delete_key_reply_t *rmp;
  int rv = 0;
  clib_error_t *error;
  u8 *key_id = 0;

  key_id = format (0, "%s%c", mp->key_id, 0);

  error = outline_server_delete_key (key_id);

  if (error)
    {
      rv = -1;
      clib_error_free (error);
    }

  vec_free (key_id);

  REPLY_MACRO (VL_API_OUTLINE_SERVER_DELETE_KEY_REPLY);
}

/**
 * @brief API handler for getting statistics
 */
static void vl_api_outline_server_get_stats_t_handler (
  vl_api_outline_server_get_stats_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_outline_server_get_stats_reply_t *rmp;
  int rv = 0;
  outline_server_stats_t stats;
  u8 is_running = 0;
  clib_error_t *error;

  error = outline_server_get_stats (&stats, &is_running);

  if (error)
    {
      rv = -1;
      clib_error_free (error);
    }

  REPLY_MACRO2 (VL_API_OUTLINE_SERVER_GET_STATS_REPLY, ({
		  rmp->is_running = is_running;
		  rmp->total_connections =
		    clib_host_to_net_u64 (stats.total_connections);
		  rmp->active_connections = htonl (stats.active_connections);
		  rmp->bytes_transferred =
		    clib_host_to_net_u64 (stats.bytes_sent + stats.bytes_received);
		  rmp->keys_count = htonl (pool_elts (osm->keys));
		}));
}

/**
 * @brief API handler for dumping ports
 */
static void
vl_api_outline_server_ports_dump_t_handler (
  vl_api_outline_server_ports_dump_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_registration_t *reg;
  outline_server_port_t *port;

  reg = vl_api_client_index_to_registration (mp->client_index);
  if (!reg)
    return;

  pool_foreach (port, osm->ports)
    {
      vl_api_outline_server_ports_details_t *rmp;

      rmp = vl_msg_api_alloc (sizeof (*rmp));
      clib_memset (rmp, 0, sizeof (*rmp));

      rmp->_vl_msg_id =
	htons (VL_API_OUTLINE_SERVER_PORTS_DETAILS + osm->msg_id_base);
      rmp->context = mp->context;
      rmp->port_id = htonl (port->port_id);
      rmp->port = htons (port->port);
      strncpy ((char *) rmp->cipher, (char *) port->cipher,
	       sizeof (rmp->cipher) - 1);
      rmp->connections = htonl (port->connections);
      rmp->bytes_transferred = clib_host_to_net_u64 (port->bytes_transferred);

      vl_api_send_msg (reg, (u8 *) rmp);
    }
}

/**
 * @brief API handler for dumping keys
 */
static void
vl_api_outline_server_keys_dump_t_handler (
  vl_api_outline_server_keys_dump_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_registration_t *reg;
  outline_server_key_t *key;

  reg = vl_api_client_index_to_registration (mp->client_index);
  if (!reg)
    return;

  pool_foreach (key, osm->keys)
    {
      vl_api_outline_server_keys_details_t *rmp;

      rmp = vl_msg_api_alloc (sizeof (*rmp));
      clib_memset (rmp, 0, sizeof (*rmp));

      rmp->_vl_msg_id =
	htons (VL_API_OUTLINE_SERVER_KEYS_DETAILS + osm->msg_id_base);
      rmp->context = mp->context;
      strncpy ((char *) rmp->key_id, (char *) key->key_id,
	       sizeof (rmp->key_id) - 1);
      rmp->port_id = htonl (key->port_id);
      rmp->data_limit = clib_host_to_net_u64 (key->data_limit);
      rmp->data_used = clib_host_to_net_u64 (key->data_used);
      rmp->is_active = key->is_active;

      vl_api_send_msg (reg, (u8 *) rmp);
    }
}

/**
 * @brief API handler for setting configuration
 */
static void vl_api_outline_server_set_config_t_handler (
  vl_api_outline_server_set_config_t *mp)
{
  outline_server_main_t *osm = &outline_server_main;
  vl_api_outline_server_set_config_reply_t *rmp;
  int rv = 0;

  osm->config.replay_history = ntohl (mp->replay_history);
  osm->config.tcp_timeout = ntohl (mp->tcp_timeout);
  osm->config.udp_timeout = ntohl (mp->udp_timeout);

  outline_log_info ("Configuration updated: replay_history=%d, tcp_timeout=%d, "
		    "udp_timeout=%d",
		    osm->config.replay_history, osm->config.tcp_timeout,
		    osm->config.udp_timeout);

  /* Reload if server is running */
  if (osm->state == OUTLINE_SERVER_STATE_RUNNING)
    {
      clib_error_t *error = outline_server_reload_config ();
      if (error)
	{
	  rv = -1;
	  clib_error_free (error);
	}
    }

  REPLY_MACRO (VL_API_OUTLINE_SERVER_SET_CONFIG_REPLY);
}

/**
 * @brief API message handler table
 */
#define vl_msg_api_version(n, v) static u32 api_version = (v);
#include <outline_server/outline_server.api.h>
#undef vl_msg_api_version

#include <outline_server/outline_server.api.c>

/**
 * @brief Plugin API initialization
 */
static clib_error_t *
outline_server_api_init (vlib_main_t *vm)
{
  outline_server_main_t *osm = &outline_server_main;

  /* Ask for a correctly-sized block of API message decode slots */
  osm->msg_id_base = setup_message_id_table ();

  return 0;
}

VLIB_INIT_FUNCTION (outline_server_api_init);
