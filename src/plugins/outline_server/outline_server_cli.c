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
#include <vppinfra/format.h>

/**
 * @brief CLI command to start outline server
 */
static clib_error_t *
outline_server_start_command_fn (vlib_main_t *vm, unformat_input_t *input,
				  vlib_cli_command_t *cmd)
{
  outline_server_main_t *osm = &outline_server_main;
  clib_error_t *error = 0;
  u16 metrics_port = osm->config.metrics_port;
  u8 *config_file = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "config %s", &config_file))
	;
      else if (unformat (input, "metrics-port %d", &metrics_port))
	;
      else
	{
	  error = clib_error_return (0, "unknown input '%U'",
				     format_unformat_error, input);
	  goto done;
	}
    }

  error = outline_server_enable_disable (1, config_file, metrics_port);

done:
  if (config_file)
    vec_free (config_file);

  return error;
}

/**
 * @brief CLI command to stop outline server
 */
static clib_error_t *
outline_server_stop_command_fn (vlib_main_t *vm, unformat_input_t *input,
				 vlib_cli_command_t *cmd)
{
  return outline_server_stop ();
}

/**
 * @brief CLI command to restart outline server
 */
static clib_error_t *
outline_server_restart_command_fn (vlib_main_t *vm, unformat_input_t *input,
				    vlib_cli_command_t *cmd)
{
  return outline_server_restart ();
}

/**
 * @brief CLI command to reload configuration
 */
static clib_error_t *
outline_server_reload_command_fn (vlib_main_t *vm, unformat_input_t *input,
				   vlib_cli_command_t *cmd)
{
  return outline_server_reload_config ();
}

/**
 * @brief CLI command to show server status
 */
static clib_error_t *
outline_server_show_status_command_fn (vlib_main_t *vm,
					unformat_input_t *input,
					vlib_cli_command_t *cmd)
{
  outline_server_main_t *osm = &outline_server_main;
  f64 uptime = 0;

  vlib_cli_output (vm, "Outline Server Status:");
  vlib_cli_output (vm, "  State: %U", format_outline_server_state, osm->state);

  if (osm->state == OUTLINE_SERVER_STATE_RUNNING)
    {
      uptime = vlib_time_now (vm) - osm->stats.uptime_start;
      vlib_cli_output (vm, "  PID: %d", osm->process.pid);
      vlib_cli_output (vm, "  Uptime: %.0f seconds", uptime);
    }

  vlib_cli_output (vm, "\nConfiguration:");
  vlib_cli_output (vm, "  Binary: %s", osm->config.server_binary_path);
  vlib_cli_output (vm, "  Metrics port: %d", osm->config.metrics_port);
  vlib_cli_output (vm, "  TCP timeout: %d seconds", osm->config.tcp_timeout);
  vlib_cli_output (vm, "  UDP timeout: %d seconds", osm->config.udp_timeout);
  vlib_cli_output (vm, "  Replay history: %d", osm->config.replay_history);

  vlib_cli_output (vm, "\nFeatures:");
  vlib_cli_output (vm, "  Metrics: %s",
		   osm->enable_metrics ? "enabled" : "disabled");
  vlib_cli_output (vm, "  IP Info: %s",
		   osm->enable_ipinfo ? "enabled" : "disabled");
  vlib_cli_output (vm, "  Replay Defense: %s",
		   osm->enable_replay_defense ? "enabled" : "disabled");

  vlib_cli_output (vm, "\nStatistics:");
  vlib_cli_output (vm, "  %U", format_outline_server_stats, &osm->stats);

  vlib_cli_output (vm, "\nResources:");
  vlib_cli_output (vm, "  Ports configured: %d", pool_elts (osm->ports));
  vlib_cli_output (vm, "  Keys configured: %d", pool_elts (osm->keys));

  return 0;
}

/**
 * @brief CLI command to add a port
 */
static clib_error_t *
outline_server_add_port_command_fn (vlib_main_t *vm, unformat_input_t *input,
				     vlib_cli_command_t *cmd)
{
  u16 port = 0;
  u8 *password = 0;
  u8 *cipher = 0;
  u32 timeout = OUTLINE_SERVER_DEFAULT_TIMEOUT;
  u32 port_id;
  clib_error_t *error = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "port %d", &port))
	;
      else if (unformat (input, "password %s", &password))
	;
      else if (unformat (input, "cipher %s", &cipher))
	;
      else if (unformat (input, "timeout %d", &timeout))
	;
      else
	{
	  error = clib_error_return (0, "unknown input '%U'",
				     format_unformat_error, input);
	  goto done;
	}
    }

  if (!port)
    {
      error = clib_error_return (0, "port number required");
      goto done;
    }

  if (!password)
    {
      error = clib_error_return (0, "password required");
      goto done;
    }

  error = outline_server_add_port (port, password, cipher, timeout, &port_id);

  if (!error)
    vlib_cli_output (vm, "Port added successfully (ID: %d)", port_id);

done:
  if (password)
    vec_free (password);
  if (cipher)
    vec_free (cipher);

  return error;
}

/**
 * @brief CLI command to delete a port
 */
static clib_error_t *
outline_server_delete_port_command_fn (vlib_main_t *vm,
					unformat_input_t *input,
					vlib_cli_command_t *cmd)
{
  u32 port_id = ~0;
  clib_error_t *error = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "id %d", &port_id))
	;
      else
	{
	  error = clib_error_return (0, "unknown input '%U'",
				     format_unformat_error, input);
	  goto done;
	}
    }

  if (port_id == ~0)
    {
      error = clib_error_return (0, "port id required");
      goto done;
    }

  error = outline_server_delete_port (port_id);

  if (!error)
    vlib_cli_output (vm, "Port deleted successfully");

done:
  return error;
}

/**
 * @brief CLI command to show all ports
 */
static clib_error_t *
outline_server_show_ports_command_fn (vlib_main_t *vm,
				       unformat_input_t *input,
				       vlib_cli_command_t *cmd)
{
  outline_server_main_t *osm = &outline_server_main;
  outline_server_port_t *port;
  u32 count = 0;

  vlib_cli_output (vm, "Configured Ports:\n");

  if (pool_elts (osm->ports) == 0)
    {
      vlib_cli_output (vm, "  No ports configured\n");
      return 0;
    }

  pool_foreach (port, osm->ports)
    {
      vlib_cli_output (vm, "%U\n", format_outline_server_port, port);
      count++;
    }

  vlib_cli_output (vm, "\nTotal: %d port(s)", count);

  return 0;
}

/**
 * @brief CLI command to add a key
 */
static clib_error_t *
outline_server_add_key_command_fn (vlib_main_t *vm, unformat_input_t *input,
				    vlib_cli_command_t *cmd)
{
  u8 *key_id = 0;
  u32 port_id = ~0;
  u8 *password = 0;
  u64 data_limit = 0;
  clib_error_t *error = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "id %s", &key_id))
	;
      else if (unformat (input, "port-id %d", &port_id))
	;
      else if (unformat (input, "password %s", &password))
	;
      else if (unformat (input, "data-limit %llu", &data_limit))
	;
      else
	{
	  error = clib_error_return (0, "unknown input '%U'",
				     format_unformat_error, input);
	  goto done;
	}
    }

  if (!key_id)
    {
      error = clib_error_return (0, "key id required");
      goto done;
    }

  if (port_id == ~0)
    {
      error = clib_error_return (0, "port id required");
      goto done;
    }

  if (!password)
    {
      error = clib_error_return (0, "password required");
      goto done;
    }

  error = outline_server_add_key (key_id, port_id, password, data_limit);

  if (!error)
    vlib_cli_output (vm, "Key added successfully");

done:
  if (key_id)
    vec_free (key_id);
  if (password)
    vec_free (password);

  return error;
}

/**
 * @brief CLI command to delete a key
 */
static clib_error_t *
outline_server_delete_key_command_fn (vlib_main_t *vm,
				       unformat_input_t *input,
				       vlib_cli_command_t *cmd)
{
  u8 *key_id = 0;
  clib_error_t *error = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "id %s", &key_id))
	;
      else
	{
	  error = clib_error_return (0, "unknown input '%U'",
				     format_unformat_error, input);
	  goto done;
	}
    }

  if (!key_id)
    {
      error = clib_error_return (0, "key id required");
      goto done;
    }

  error = outline_server_delete_key (key_id);

  if (!error)
    vlib_cli_output (vm, "Key deleted successfully");

done:
  if (key_id)
    vec_free (key_id);

  return error;
}

/**
 * @brief CLI command to show all keys
 */
static clib_error_t *
outline_server_show_keys_command_fn (vlib_main_t *vm, unformat_input_t *input,
				      vlib_cli_command_t *cmd)
{
  outline_server_main_t *osm = &outline_server_main;
  outline_server_key_t *key;
  u32 count = 0;

  vlib_cli_output (vm, "Configured Keys:\n");

  if (pool_elts (osm->keys) == 0)
    {
      vlib_cli_output (vm, "  No keys configured\n");
      return 0;
    }

  pool_foreach (key, osm->keys)
    {
      vlib_cli_output (vm, "%U\n", format_outline_server_key, key);
      count++;
    }

  vlib_cli_output (vm, "\nTotal: %d key(s)", count);

  return 0;
}

/**
 * @brief CLI command to show configuration
 */
static clib_error_t *
outline_server_show_config_command_fn (vlib_main_t *vm,
					unformat_input_t *input,
					vlib_cli_command_t *cmd)
{
  outline_server_main_t *osm = &outline_server_main;
  clib_error_t *error;

  error = outline_server_generate_config ();
  if (error)
    return error;

  vlib_cli_output (vm, "Current Configuration:\n");
  vlib_cli_output (vm, "%s", osm->config_json);

  return 0;
}

/* CLI command definitions */
VLIB_CLI_COMMAND (outline_server_start_command, static) = {
  .path = "outline-server start",
  .short_help = "outline-server start [config <file>] [metrics-port <port>]",
  .function = outline_server_start_command_fn,
};

VLIB_CLI_COMMAND (outline_server_stop_command, static) = {
  .path = "outline-server stop",
  .short_help = "outline-server stop",
  .function = outline_server_stop_command_fn,
};

VLIB_CLI_COMMAND (outline_server_restart_command, static) = {
  .path = "outline-server restart",
  .short_help = "outline-server restart",
  .function = outline_server_restart_command_fn,
};

VLIB_CLI_COMMAND (outline_server_reload_command, static) = {
  .path = "outline-server reload",
  .short_help = "outline-server reload - reload configuration",
  .function = outline_server_reload_command_fn,
};

VLIB_CLI_COMMAND (outline_server_show_status_command, static) = {
  .path = "show outline-server status",
  .short_help = "show outline-server status",
  .function = outline_server_show_status_command_fn,
};

VLIB_CLI_COMMAND (outline_server_add_port_command, static) = {
  .path = "outline-server add port",
  .short_help = "outline-server add port <port> password <password> [cipher "
		"<cipher>] [timeout <seconds>]",
  .function = outline_server_add_port_command_fn,
};

VLIB_CLI_COMMAND (outline_server_delete_port_command, static) = {
  .path = "outline-server delete port",
  .short_help = "outline-server delete port id <port-id>",
  .function = outline_server_delete_port_command_fn,
};

VLIB_CLI_COMMAND (outline_server_show_ports_command, static) = {
  .path = "show outline-server ports",
  .short_help = "show outline-server ports",
  .function = outline_server_show_ports_command_fn,
};

VLIB_CLI_COMMAND (outline_server_add_key_command, static) = {
  .path = "outline-server add key",
  .short_help = "outline-server add key id <key-id> port-id <port-id> "
		"password <password> [data-limit <bytes>]",
  .function = outline_server_add_key_command_fn,
};

VLIB_CLI_COMMAND (outline_server_delete_key_command, static) = {
  .path = "outline-server delete key",
  .short_help = "outline-server delete key id <key-id>",
  .function = outline_server_delete_key_command_fn,
};

VLIB_CLI_COMMAND (outline_server_show_keys_command, static) = {
  .path = "show outline-server keys",
  .short_help = "show outline-server keys",
  .function = outline_server_show_keys_command_fn,
};

VLIB_CLI_COMMAND (outline_server_show_config_command, static) = {
  .path = "show outline-server config",
  .short_help = "show outline-server config",
  .function = outline_server_show_config_command_fn,
};
