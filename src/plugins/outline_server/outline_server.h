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

/**
 * @file outline_server.h
 * @brief Outline Server plugin for VPP
 *
 * This plugin integrates the Outline Shadowsocks server with VPP,
 * providing high-performance censorship circumvention capabilities.
 */

#ifndef __OUTLINE_SERVER_H__
#define __OUTLINE_SERVER_H__

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vnet/ip/ip.h>
#include <vppinfra/hash.h>
#include <vppinfra/error.h>
#include <vppinfra/lock.h>

#define OUTLINE_SERVER_PLUGIN_VERSION_MAJOR 1
#define OUTLINE_SERVER_PLUGIN_VERSION_MINOR 0
#define OUTLINE_SERVER_PLUGIN_VERSION_PATCH 0

#define OUTLINE_SERVER_MAX_PORTS 256
#define OUTLINE_SERVER_MAX_KEYS 10000
#define OUTLINE_SERVER_DEFAULT_TIMEOUT 300
#define OUTLINE_SERVER_CONFIG_PATH_MAX 256

/* Process states */
typedef enum
{
  OUTLINE_SERVER_STATE_STOPPED,
  OUTLINE_SERVER_STATE_STARTING,
  OUTLINE_SERVER_STATE_RUNNING,
  OUTLINE_SERVER_STATE_STOPPING,
  OUTLINE_SERVER_STATE_ERROR
} outline_server_state_t;

/* Port configuration */
typedef struct
{
  u32 port_id;
  u16 port;
  u8 *cipher;
  u8 *password;
  u32 timeout;

  /* Statistics */
  u64 connections;
  u64 bytes_transferred;
  f64 last_activity;

  /* Status */
  u8 is_active;
} outline_server_port_t;

/* Access key */
typedef struct
{
  u8 *key_id;
  u32 port_id;
  u8 *password;
  u64 data_limit;
  u64 data_used;

  /* Status */
  u8 is_active;
  f64 created_at;
  f64 last_used;
} outline_server_key_t;

/* Server configuration */
typedef struct
{
  u8 *config_file;
  u16 metrics_port;
  u32 replay_history;
  u32 tcp_timeout;
  u32 udp_timeout;

  /* Binary paths */
  u8 *server_binary_path;
  u8 *log_path;
} outline_server_config_t;

/* Server statistics */
typedef struct
{
  u64 total_connections;
  u32 active_connections;
  u64 bytes_sent;
  u64 bytes_received;
  u64 packets_sent;
  u64 packets_received;

  /* Errors */
  u64 connection_errors;
  u64 auth_failures;
  u64 replay_attacks_blocked;

  f64 uptime_start;
  f64 last_stats_update;
} outline_server_stats_t;

/* Process management */
typedef struct
{
  pid_t pid;
  int stdin_fd;
  int stdout_fd;
  int stderr_fd;
  u8 *stdout_buffer;
  u8 *stderr_buffer;
} outline_server_process_t;

/* Main plugin structure */
typedef struct
{
  /* API message ID base */
  u16 msg_id_base;

  /* Server state */
  outline_server_state_t state;
  clib_spinlock_t state_lock;

  /* Configuration */
  outline_server_config_t config;

  /* Process management */
  outline_server_process_t process;

  /* Ports and keys */
  outline_server_port_t *ports;
  uword *port_by_id;
  uword *port_by_number;

  outline_server_key_t *keys;
  uword *key_by_id;

  /* Statistics */
  outline_server_stats_t stats;

  /* VPP integration */
  vlib_main_t *vlib_main;
  vnet_main_t *vnet_main;

  /* Process node */
  u32 process_node_index;

  /* Logging */
  vlib_log_class_t log_class;

  /* Feature flags */
  u8 enable_metrics;
  u8 enable_ipinfo;
  u8 enable_replay_defense;

  /* Memory pool */
  u8 *config_json;

} outline_server_main_t;

extern outline_server_main_t outline_server_main;
extern vlib_node_registration_t outline_server_process_node;

/* API handlers */
clib_error_t *outline_server_enable_disable (u8 enable, u8 *config_file,
					      u16 metrics_port);

clib_error_t *outline_server_add_port (u16 port, u8 *password, u8 *cipher,
					u32 timeout, u32 *port_id);

clib_error_t *outline_server_delete_port (u32 port_id);

clib_error_t *outline_server_add_key (u8 *key_id, u32 port_id, u8 *password,
				       u64 data_limit);

clib_error_t *outline_server_delete_key (u8 *key_id);

clib_error_t *outline_server_get_stats (outline_server_stats_t *stats,
					 u8 *is_running);

/* Process management */
clib_error_t *outline_server_start (void);
clib_error_t *outline_server_stop (void);
clib_error_t *outline_server_restart (void);
clib_error_t *outline_server_reload_config (void);

/* Configuration management */
clib_error_t *outline_server_generate_config (void);
clib_error_t *outline_server_write_config (u8 *path);
clib_error_t *outline_server_read_config (u8 *path);

/* Utility functions */
u8 *format_outline_server_state (u8 *s, va_list *args);
u8 *format_outline_server_stats (u8 *s, va_list *args);
u8 *format_outline_server_port (u8 *s, va_list *args);
u8 *format_outline_server_key (u8 *s, va_list *args);

/* Logging macros */
#define outline_log_debug(...)                                                \
  vlib_log_debug (outline_server_main.log_class, __VA_ARGS__)
#define outline_log_info(...)                                                 \
  vlib_log_info (outline_server_main.log_class, __VA_ARGS__)
#define outline_log_notice(...)                                               \
  vlib_log_notice (outline_server_main.log_class, __VA_ARGS__)
#define outline_log_warn(...)                                                 \
  vlib_log_warn (outline_server_main.log_class, __VA_ARGS__)
#define outline_log_err(...)                                                  \
  vlib_log_err (outline_server_main.log_class, __VA_ARGS__)

#endif /* __OUTLINE_SERVER_H__ */
