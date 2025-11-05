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
#include <vpp/app/version.h>
#include <vnet/plugin/plugin.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

outline_server_main_t outline_server_main;

/* Plugin registration */
VLIB_PLUGIN_REGISTER () = {
  .version = VPP_BUILD_VER,
  .description = "Outline Shadowsocks Server Integration",
  .default_disabled = 0,
};

/**
 * @brief Initialize the outline server plugin
 */
static clib_error_t *
outline_server_init (vlib_main_t *vm)
{
  outline_server_main_t *osm = &outline_server_main;
  clib_error_t *error = 0;

  osm->vlib_main = vm;
  osm->vnet_main = vnet_get_main ();

  /* Initialize state */
  osm->state = OUTLINE_SERVER_STATE_STOPPED;
  clib_spinlock_init (&osm->state_lock);

  /* Initialize hash tables */
  osm->port_by_id = hash_create (0, sizeof (uword));
  osm->port_by_number = hash_create (0, sizeof (uword));
  osm->key_by_id = hash_create_string (0, sizeof (uword));

  /* Initialize configuration with defaults */
  osm->config.metrics_port = 9091;
  osm->config.replay_history = 10000;
  osm->config.tcp_timeout = OUTLINE_SERVER_DEFAULT_TIMEOUT;
  osm->config.udp_timeout = 60;

  /* Set binary path - will be in the same directory as vpp plugins */
  osm->config.server_binary_path =
    format (0, "%s/vpp_plugins/outline-ss-server%c", vlib_plugin_get_lib_dir (),
	    0);

  osm->config.log_path = format (0, "/var/log/vpp/outline-server.log%c", 0);

  /* Enable features by default */
  osm->enable_metrics = 1;
  osm->enable_ipinfo = 0;
  osm->enable_replay_defense = 1;

  /* Initialize logging */
  osm->log_class = vlib_log_register_class ("outline_server", 0);

  outline_log_info ("Outline Server plugin initialized (v%d.%d.%d)",
		    OUTLINE_SERVER_PLUGIN_VERSION_MAJOR,
		    OUTLINE_SERVER_PLUGIN_VERSION_MINOR,
		    OUTLINE_SERVER_PLUGIN_VERSION_PATCH);

  return error;
}

VLIB_INIT_FUNCTION (outline_server_init);

/**
 * @brief Generate JSON configuration for outline-ss-server
 */
clib_error_t *
outline_server_generate_config (void)
{
  outline_server_main_t *osm = &outline_server_main;
  u8 *json;
  outline_server_port_t *port;
  outline_server_key_t *key;
  u32 first = 1;

  json = format (0, "{\n");
  json = format (json, "  \"portConfig\": [\n");

  /* Generate port configurations */
  pool_foreach (port, osm->ports)
    {
      if (!first)
	json = format (json, ",\n");
      first = 0;

      json = format (json, "    {\n");
      json = format (json, "      \"port\": %d,\n", port->port);

      /* Add keys for this port */
      json = format (json, "      \"keys\": [\n");

      u32 key_first = 1;
      pool_foreach (key, osm->keys)
	{
	  if (key->port_id == port->port_id && key->is_active)
	    {
	      if (!key_first)
		json = format (json, ",\n");
	      key_first = 0;

	      json = format (json, "        {\n");
	      json = format (json, "          \"id\": \"%s\",\n", key->key_id);
	      json = format (json, "          \"port\": %d,\n", port->port);
	      json = format (json, "          \"cipher\": \"%s\",\n",
			     port->cipher);
	      json = format (json, "          \"secret\": \"%s\"", key->password);

	      if (key->data_limit > 0)
		json = format (json, ",\n          \"dataLimit\": %llu",
			       key->data_limit);

	      json = format (json, "\n        }");
	    }
	}

      json = format (json, "\n      ]\n");
      json = format (json, "    }");
    }

  json = format (json, "\n  ]");

  /* Add metrics configuration */
  if (osm->enable_metrics)
    {
      json = format (json, ",\n  \"metrics\": {\n");
      json = format (json, "    \"address\": \"0.0.0.0:%d\"\n",
		     osm->config.metrics_port);
      json = format (json, "  }");
    }

  /* Add replay defense configuration */
  if (osm->enable_replay_defense)
    {
      json = format (json, ",\n  \"replayHistory\": %d",
		     osm->config.replay_history);
    }

  json = format (json, "\n}\n%c", 0);

  /* Free old config and store new one */
  if (osm->config_json)
    vec_free (osm->config_json);

  osm->config_json = json;

  return 0;
}

/**
 * @brief Write configuration to file
 */
clib_error_t *
outline_server_write_config (u8 *path)
{
  outline_server_main_t *osm = &outline_server_main;
  FILE *fp;
  clib_error_t *error = 0;

  if (!osm->config_json)
    {
      error = outline_server_generate_config ();
      if (error)
	return error;
    }

  fp = fopen ((char *) path, "w");
  if (!fp)
    return clib_error_return_unix (0, "failed to open config file '%s'", path);

  if (fwrite (osm->config_json, 1, vec_len (osm->config_json) - 1, fp) !=
      vec_len (osm->config_json) - 1)
    {
      fclose (fp);
      return clib_error_return_unix (0, "failed to write config file");
    }

  fclose (fp);
  outline_log_info ("Configuration written to %s", path);

  return 0;
}

/**
 * @brief Start the outline-ss-server process
 */
clib_error_t *
outline_server_start (void)
{
  outline_server_main_t *osm = &outline_server_main;
  clib_error_t *error = 0;
  pid_t pid;
  int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
  u8 *config_path;

  clib_spinlock_lock (&osm->state_lock);

  if (osm->state == OUTLINE_SERVER_STATE_RUNNING)
    {
      clib_spinlock_unlock (&osm->state_lock);
      return clib_error_return (0, "server is already running");
    }

  osm->state = OUTLINE_SERVER_STATE_STARTING;
  clib_spinlock_unlock (&osm->state_lock);

  /* Generate and write configuration */
  config_path = format (0, "/tmp/outline-server-%d.json%c", getpid (), 0);

  error = outline_server_generate_config ();
  if (error)
    goto error;

  error = outline_server_write_config (config_path);
  if (error)
    goto error;

  /* Create pipes for IPC */
  if (pipe (stdin_pipe) < 0 || pipe (stdout_pipe) < 0 || pipe (stderr_pipe) < 0)
    {
      error = clib_error_return_unix (0, "failed to create pipes");
      goto error;
    }

  /* Fork process */
  pid = fork ();

  if (pid < 0)
    {
      error = clib_error_return_unix (0, "failed to fork process");
      goto error;
    }

  if (pid == 0)
    {
      /* Child process */
      close (stdin_pipe[1]);
      close (stdout_pipe[0]);
      close (stderr_pipe[0]);

      dup2 (stdin_pipe[0], STDIN_FILENO);
      dup2 (stdout_pipe[1], STDOUT_FILENO);
      dup2 (stderr_pipe[1], STDERR_FILENO);

      close (stdin_pipe[0]);
      close (stdout_pipe[1]);
      close (stderr_pipe[1]);

      /* Execute outline-ss-server */
      char *args[] = { (char *) osm->config.server_binary_path, "-config",
		       (char *) config_path, NULL };

      execv ((char *) osm->config.server_binary_path, args);

      /* If we get here, exec failed */
      fprintf (stderr, "Failed to execute outline-ss-server: %s\n",
	       strerror (errno));
      exit (1);
    }

  /* Parent process */
  close (stdin_pipe[0]);
  close (stdout_pipe[1]);
  close (stderr_pipe[1]);

  /* Set pipes to non-blocking */
  fcntl (stdout_pipe[0], F_SETFL, O_NONBLOCK);
  fcntl (stderr_pipe[0], F_SETFL, O_NONBLOCK);

  /* Store process information */
  osm->process.pid = pid;
  osm->process.stdin_fd = stdin_pipe[1];
  osm->process.stdout_fd = stdout_pipe[0];
  osm->process.stderr_fd = stderr_pipe[0];

  clib_spinlock_lock (&osm->state_lock);
  osm->state = OUTLINE_SERVER_STATE_RUNNING;
  osm->stats.uptime_start = vlib_time_now (osm->vlib_main);
  clib_spinlock_unlock (&osm->state_lock);

  vec_free (config_path);

  outline_log_info ("Outline server started (PID: %d)", pid);

  return 0;

error:
  clib_spinlock_lock (&osm->state_lock);
  osm->state = OUTLINE_SERVER_STATE_ERROR;
  clib_spinlock_unlock (&osm->state_lock);

  if (config_path)
    vec_free (config_path);

  return error;
}

/**
 * @brief Stop the outline-ss-server process
 */
clib_error_t *
outline_server_stop (void)
{
  outline_server_main_t *osm = &outline_server_main;
  int status;
  int timeout = 10; /* 10 seconds timeout */

  clib_spinlock_lock (&osm->state_lock);

  if (osm->state != OUTLINE_SERVER_STATE_RUNNING)
    {
      clib_spinlock_unlock (&osm->state_lock);
      return clib_error_return (0, "server is not running");
    }

  osm->state = OUTLINE_SERVER_STATE_STOPPING;
  clib_spinlock_unlock (&osm->state_lock);

  /* Send SIGTERM */
  outline_log_info ("Stopping outline server (PID: %d)", osm->process.pid);
  kill (osm->process.pid, SIGTERM);

  /* Wait for process to exit gracefully */
  while (timeout-- > 0)
    {
      if (waitpid (osm->process.pid, &status, WNOHANG) == osm->process.pid)
	break;

      sleep (1);
    }

  /* Force kill if still running */
  if (timeout <= 0)
    {
      outline_log_warn ("Server did not stop gracefully, forcing shutdown");
      kill (osm->process.pid, SIGKILL);
      waitpid (osm->process.pid, &status, 0);
    }

  /* Close pipes */
  close (osm->process.stdin_fd);
  close (osm->process.stdout_fd);
  close (osm->process.stderr_fd);

  /* Clear buffers */
  if (osm->process.stdout_buffer)
    {
      vec_free (osm->process.stdout_buffer);
      osm->process.stdout_buffer = 0;
    }
  if (osm->process.stderr_buffer)
    {
      vec_free (osm->process.stderr_buffer);
      osm->process.stderr_buffer = 0;
    }

  clib_spinlock_lock (&osm->state_lock);
  osm->state = OUTLINE_SERVER_STATE_STOPPED;
  clib_spinlock_unlock (&osm->state_lock);

  outline_log_info ("Outline server stopped");

  return 0;
}

/**
 * @brief Restart the server
 */
clib_error_t *
outline_server_restart (void)
{
  clib_error_t *error;

  error = outline_server_stop ();
  if (error)
    return error;

  sleep (1);

  return outline_server_start ();
}

/**
 * @brief Reload configuration
 */
clib_error_t *
outline_server_reload_config (void)
{
  outline_server_main_t *osm = &outline_server_main;
  clib_error_t *error;

  if (osm->state != OUTLINE_SERVER_STATE_RUNNING)
    return clib_error_return (0, "server is not running");

  /* Generate new configuration */
  error = outline_server_generate_config ();
  if (error)
    return error;

  /* Write configuration to file */
  u8 *config_path = format (0, "/tmp/outline-server-%d.json%c", getpid (), 0);
  error = outline_server_write_config (config_path);
  vec_free (config_path);

  if (error)
    return error;

  /* Send SIGHUP to reload */
  kill (osm->process.pid, SIGHUP);

  outline_log_info ("Configuration reloaded");

  return 0;
}

/**
 * @brief Enable/Disable outline server
 */
clib_error_t *
outline_server_enable_disable (u8 enable, u8 *config_file, u16 metrics_port)
{
  outline_server_main_t *osm = &outline_server_main;
  clib_error_t *error = 0;

  if (enable)
    {
      if (config_file)
	{
	  if (osm->config.config_file)
	    vec_free (osm->config.config_file);
	  osm->config.config_file = vec_dup (config_file);
	}

      if (metrics_port > 0)
	osm->config.metrics_port = metrics_port;

      error = outline_server_start ();
    }
  else
    {
      error = outline_server_stop ();
    }

  return error;
}

/**
 * @brief Add a new port
 */
clib_error_t *
outline_server_add_port (u16 port, u8 *password, u8 *cipher, u32 timeout,
			  u32 *port_id)
{
  outline_server_main_t *osm = &outline_server_main;
  outline_server_port_t *p;
  uword *existing;

  /* Check if port already exists */
  existing = hash_get (osm->port_by_number, port);
  if (existing)
    return clib_error_return (0, "port %d already exists", port);

  /* Allocate new port */
  pool_get_zero (osm->ports, p);
  *port_id = p - osm->ports;

  p->port_id = *port_id;
  p->port = port;
  p->cipher = vec_dup (cipher ? cipher : (u8 *) "chacha20-ietf-poly1305");
  p->password = vec_dup (password);
  p->timeout = timeout ? timeout : OUTLINE_SERVER_DEFAULT_TIMEOUT;
  p->is_active = 1;

  /* Add to hash tables */
  hash_set (osm->port_by_id, *port_id, p - osm->ports);
  hash_set (osm->port_by_number, port, p - osm->ports);

  outline_log_info ("Added port %d (id: %d, cipher: %s)", port, *port_id,
		    p->cipher);

  /* Reload configuration if server is running */
  if (osm->state == OUTLINE_SERVER_STATE_RUNNING)
    outline_server_reload_config ();

  return 0;
}

/**
 * @brief Delete a port
 */
clib_error_t *
outline_server_delete_port (u32 port_id)
{
  outline_server_main_t *osm = &outline_server_main;
  outline_server_port_t *port;
  outline_server_key_t *key;
  uword *p;

  p = hash_get (osm->port_by_id, port_id);
  if (!p)
    return clib_error_return (0, "port id %d not found", port_id);

  port = pool_elt_at_index (osm->ports, p[0]);

  /* Remove all keys associated with this port */
  pool_foreach (key, osm->keys)
    {
      if (key->port_id == port_id)
	{
	  hash_unset_mem (osm->key_by_id, key->key_id);
	  vec_free (key->key_id);
	  vec_free (key->password);
	  pool_put (osm->keys, key);
	}
    }

  /* Remove from hash tables */
  hash_unset (osm->port_by_id, port_id);
  hash_unset (osm->port_by_number, port->port);

  outline_log_info ("Deleted port %d (id: %d)", port->port, port_id);

  /* Free memory */
  vec_free (port->cipher);
  vec_free (port->password);
  pool_put (osm->ports, port);

  /* Reload configuration if server is running */
  if (osm->state == OUTLINE_SERVER_STATE_RUNNING)
    outline_server_reload_config ();

  return 0;
}

/**
 * @brief Add or update an access key
 */
clib_error_t *
outline_server_add_key (u8 *key_id, u32 port_id, u8 *password, u64 data_limit)
{
  outline_server_main_t *osm = &outline_server_main;
  outline_server_key_t *key;
  uword *p;

  /* Verify port exists */
  p = hash_get (osm->port_by_id, port_id);
  if (!p)
    return clib_error_return (0, "port id %d not found", port_id);

  /* Check if key already exists */
  p = hash_get_mem (osm->key_by_id, key_id);
  if (p)
    {
      /* Update existing key */
      key = pool_elt_at_index (osm->keys, p[0]);

      if (key->password)
	vec_free (key->password);

      key->password = vec_dup (password);
      key->data_limit = data_limit;
      key->port_id = port_id;

      outline_log_info ("Updated key %s", key_id);
    }
  else
    {
      /* Create new key */
      pool_get_zero (osm->keys, key);

      key->key_id = vec_dup (key_id);
      key->port_id = port_id;
      key->password = vec_dup (password);
      key->data_limit = data_limit;
      key->is_active = 1;
      key->created_at = vlib_time_now (osm->vlib_main);

      hash_set_mem (osm->key_by_id, key->key_id, key - osm->keys);

      outline_log_info ("Added key %s to port id %d", key_id, port_id);
    }

  /* Reload configuration if server is running */
  if (osm->state == OUTLINE_SERVER_STATE_RUNNING)
    outline_server_reload_config ();

  return 0;
}

/**
 * @brief Delete an access key
 */
clib_error_t *
outline_server_delete_key (u8 *key_id)
{
  outline_server_main_t *osm = &outline_server_main;
  outline_server_key_t *key;
  uword *p;

  p = hash_get_mem (osm->key_by_id, key_id);
  if (!p)
    return clib_error_return (0, "key %s not found", key_id);

  key = pool_elt_at_index (osm->keys, p[0]);

  outline_log_info ("Deleted key %s", key_id);

  hash_unset_mem (osm->key_by_id, key->key_id);

  vec_free (key->key_id);
  vec_free (key->password);
  pool_put (osm->keys, key);

  /* Reload configuration if server is running */
  if (osm->state == OUTLINE_SERVER_STATE_RUNNING)
    outline_server_reload_config ();

  return 0;
}

/**
 * @brief Get server statistics
 */
clib_error_t *
outline_server_get_stats (outline_server_stats_t *stats, u8 *is_running)
{
  outline_server_main_t *osm = &outline_server_main;

  clib_memcpy (stats, &osm->stats, sizeof (outline_server_stats_t));
  *is_running = (osm->state == OUTLINE_SERVER_STATE_RUNNING);

  return 0;
}

/**
 * @brief Format server state
 */
u8 *
format_outline_server_state (u8 *s, va_list *args)
{
  outline_server_state_t state = va_arg (*args, outline_server_state_t);

  switch (state)
    {
    case OUTLINE_SERVER_STATE_STOPPED:
      s = format (s, "stopped");
      break;
    case OUTLINE_SERVER_STATE_STARTING:
      s = format (s, "starting");
      break;
    case OUTLINE_SERVER_STATE_RUNNING:
      s = format (s, "running");
      break;
    case OUTLINE_SERVER_STATE_STOPPING:
      s = format (s, "stopping");
      break;
    case OUTLINE_SERVER_STATE_ERROR:
      s = format (s, "error");
      break;
    default:
      s = format (s, "unknown");
      break;
    }

  return s;
}

/**
 * @brief Format server statistics
 */
u8 *
format_outline_server_stats (u8 *s, va_list *args)
{
  outline_server_stats_t *stats = va_arg (*args, outline_server_stats_t *);

  s = format (s, "Connections: %llu total, %u active\n", stats->total_connections,
	      stats->active_connections);
  s = format (s, "Bytes: %llu sent, %llu received\n", stats->bytes_sent,
	      stats->bytes_received);
  s = format (s, "Packets: %llu sent, %llu received\n", stats->packets_sent,
	      stats->packets_received);
  s = format (s, "Errors: %llu connection, %llu auth failures\n",
	      stats->connection_errors, stats->auth_failures);
  s = format (s, "Replay attacks blocked: %llu", stats->replay_attacks_blocked);

  return s;
}

/**
 * @brief Format port information
 */
u8 *
format_outline_server_port (u8 *s, va_list *args)
{
  outline_server_port_t *port = va_arg (*args, outline_server_port_t *);

  s = format (s, "Port %d (id: %d)\n", port->port, port->port_id);
  s = format (s, "  Cipher: %s\n", port->cipher);
  s = format (s, "  Timeout: %d seconds\n", port->timeout);
  s = format (s, "  Status: %s\n", port->is_active ? "active" : "inactive");
  s = format (s, "  Connections: %llu\n", port->connections);
  s = format (s, "  Bytes transferred: %llu", port->bytes_transferred);

  return s;
}

/**
 * @brief Format key information
 */
u8 *
format_outline_server_key (u8 *s, va_list *args)
{
  outline_server_key_t *key = va_arg (*args, outline_server_key_t *);

  s = format (s, "Key: %s\n", key->key_id);
  s = format (s, "  Port ID: %d\n", key->port_id);
  s = format (s, "  Status: %s\n", key->is_active ? "active" : "inactive");

  if (key->data_limit > 0)
    {
      s = format (s, "  Data limit: %llu bytes\n", key->data_limit);
      s = format (s, "  Data used: %llu bytes (%.1f%%)", key->data_used,
		  (f64) key->data_used / (f64) key->data_limit * 100.0);
    }
  else
    {
      s = format (s, "  Data limit: unlimited");
    }

  return s;
}
