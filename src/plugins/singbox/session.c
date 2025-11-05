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
 * @brief Session Management for Sing-box Plugin
 *
 * This file implements session creation, deletion, and connection pooling
 * for the sing-box integration plugin. It manages connections to sing-box
 * proxy instances with proper lifecycle management.
 */

#include <vppinfra/error.h>
#include <vppinfra/pool.h>
#include <vppinfra/hash.h>
#include <vnet/session/session.h>
#include <singbox/singbox.h>

/**
 * @brief Create a new session to sing-box proxy
 */
singbox_session_t *
singbox_session_create (singbox_main_t *sm, ip4_address_t *dst_addr,
                       u16 dst_port, u32 sw_if_index)
{
  singbox_session_t *session;
  singbox_interface_config_t *config;
  u32 session_index;

  config = singbox_get_interface_config (sm, sw_if_index);
  if (!config || !config->endpoint.is_enabled)
    return NULL;

  /* Check connection limit */
  if (config->endpoint.max_connections > 0 &&
      config->active_connections >= config->endpoint.max_connections)
    {
      clib_warning ("Maximum connections reached for interface %d",
                   sw_if_index);
      config->connection_failures++;
      return NULL;
    }

  /* Lock for session pool operations */
  clib_spinlock_lock (&sm->sessions_lock);

  /* Allocate new session from pool */
  pool_get_zero (sm->sessions, session);
  session_index = session - sm->sessions;

  /* Initialize session */
  session->session_index = session_index;
  session->sw_if_index = sw_if_index;
  session->state = SINGBOX_STATE_IDLE;
  session->dst_addr = *dst_addr;
  session->dst_port = dst_port;
  session->endpoint = &config->endpoint;
  session->last_activity = vlib_time_now (sm->vlib_main);
  session->proxy_session_handle = SESSION_INVALID_HANDLE;
  session->client_session_handle = SESSION_INVALID_HANDLE;

  /* Initialize buffers */
  vec_reset_length (session->tx_buffer);
  vec_reset_length (session->rx_buffer);

  /* Update statistics */
  config->active_connections++;
  config->total_connections++;

  clib_spinlock_unlock (&sm->sessions_lock);

  if (sm->verbose)
    {
      vlib_cli_output (sm->vlib_main, "Created session %d for %U:%d",
                      session_index, format_ip4_address, dst_addr, dst_port);
    }

  return session;
}

/**
 * @brief Delete a sing-box session
 */
void
singbox_session_delete (singbox_main_t *sm, singbox_session_t *session)
{
  singbox_interface_config_t *config;

  if (!session)
    return;

  config = singbox_get_interface_config (sm, session->sw_if_index);

  if (sm->verbose)
    {
      vlib_cli_output (sm->vlib_main, "Deleting session %d",
                      session->session_index);
    }

  clib_spinlock_lock (&sm->sessions_lock);

  /* Remove from hash tables */
  if (session->client_session_handle != SESSION_INVALID_HANDLE)
    {
      hash_unset (sm->session_by_client_handle,
                 session->client_session_handle);
    }

  if (session->proxy_session_handle != SESSION_INVALID_HANDLE)
    {
      hash_unset (sm->session_by_proxy_handle, session->proxy_session_handle);
    }

  /* Free buffers */
  vec_free (session->tx_buffer);
  vec_free (session->rx_buffer);

  /* Update statistics */
  if (config)
    {
      if (config->active_connections > 0)
        config->active_connections--;
    }

  /* Return to pool */
  pool_put (sm->sessions, session);

  clib_spinlock_unlock (&sm->sessions_lock);
}

/**
 * @brief Get session by client handle
 */
singbox_session_t *
singbox_session_get_by_client (singbox_main_t *sm,
                              session_handle_t client_handle)
{
  uword *p;
  singbox_session_t *session = NULL;

  clib_spinlock_lock (&sm->sessions_lock);

  p = hash_get (sm->session_by_client_handle, client_handle);
  if (p)
    {
      session = pool_elt_at_index (sm->sessions, p[0]);
    }

  clib_spinlock_unlock (&sm->sessions_lock);

  return session;
}

/**
 * @brief Get session by proxy handle
 */
singbox_session_t *
singbox_session_get_by_proxy (singbox_main_t *sm,
                             session_handle_t proxy_handle)
{
  uword *p;
  singbox_session_t *session = NULL;

  clib_spinlock_lock (&sm->sessions_lock);

  p = hash_get (sm->session_by_proxy_handle, proxy_handle);
  if (p)
    {
      session = pool_elt_at_index (sm->sessions, p[0]);
    }

  clib_spinlock_unlock (&sm->sessions_lock);

  return session;
}

/**
 * @brief Register client session handle
 */
int
singbox_session_register_client (singbox_main_t *sm,
                                 singbox_session_t *session,
                                 session_handle_t client_handle)
{
  if (!session || client_handle == SESSION_INVALID_HANDLE)
    return -1;

  clib_spinlock_lock (&sm->sessions_lock);

  session->client_session_handle = client_handle;
  hash_set (sm->session_by_client_handle, client_handle,
           session->session_index);

  clib_spinlock_unlock (&sm->sessions_lock);

  return 0;
}

/**
 * @brief Register proxy session handle
 */
int
singbox_session_register_proxy (singbox_main_t *sm,
                                singbox_session_t *session,
                                session_handle_t proxy_handle)
{
  if (!session || proxy_handle == SESSION_INVALID_HANDLE)
    return -1;

  clib_spinlock_lock (&sm->sessions_lock);

  session->proxy_session_handle = proxy_handle;
  hash_set (sm->session_by_proxy_handle, proxy_handle,
           session->session_index);

  clib_spinlock_unlock (&sm->sessions_lock);

  return 0;
}

/**
 * @brief Cleanup idle sessions (periodic callback)
 */
void
singbox_session_cleanup (singbox_main_t *sm)
{
  singbox_session_t *session;
  f64 now = vlib_time_now (sm->vlib_main);
  u32 timeout = sm->session_timeout;
  u32 *to_delete = NULL;

  if (timeout == 0)
    return;

  /* Identify sessions to delete */
  pool_foreach (session, sm->sessions)
    {
      if (session->state == SINGBOX_STATE_ERROR ||
          session->state == SINGBOX_STATE_CLOSED ||
          (now - session->last_activity) > timeout)
        {
          vec_add1 (to_delete, session->session_index);
        }
    }

  /* Delete identified sessions */
  for (int i = 0; i < vec_len (to_delete); i++)
    {
      session = pool_elt_at_index (sm->sessions, to_delete[i]);
      singbox_session_delete (sm, session);
    }

  vec_free (to_delete);
}

/**
 * @brief Get session statistics
 */
void
singbox_session_get_stats (singbox_main_t *sm, u32 sw_if_index,
                          u32 *active, u64 *total, u64 *failures)
{
  singbox_interface_config_t *config;

  config = singbox_get_interface_config (sm, sw_if_index);
  if (!config)
    {
      *active = 0;
      *total = 0;
      *failures = 0;
      return;
    }

  *active = config->active_connections;
  *total = config->total_connections;
  *failures = config->connection_failures;
}

/**
 * @brief Update session activity timestamp
 */
static inline void
singbox_session_touch (singbox_main_t *sm, singbox_session_t *session)
{
  session->last_activity = vlib_time_now (sm->vlib_main);
}

/**
 * @brief Forward data from client to proxy
 */
int
singbox_session_forward_to_proxy (singbox_main_t *sm,
                                  singbox_session_t *session, u8 *data,
                                  u32 len)
{
  if (!session || !session->is_connected)
    return -1;

  session->bytes_sent += len;
  singbox_session_touch (sm, session);

  /* In a full implementation, use session_send_io_evt to send data */
  /* This is a placeholder - actual implementation would use session layer API */

  return 0;
}

/**
 * @brief Forward data from proxy to client
 */
int
singbox_session_forward_to_client (singbox_main_t *sm,
                                   singbox_session_t *session, u8 *data,
                                   u32 len)
{
  if (!session)
    return -1;

  session->bytes_received += len;
  singbox_session_touch (sm, session);

  /* In a full implementation, use session_send_io_evt to send data */
  /* This is a placeholder - actual implementation would use session layer API */

  return 0;
}

/**
 * @brief Process connection pool
 *
 * Implements connection pooling to reuse established connections
 * to sing-box proxy for improved performance.
 */
singbox_session_t *
singbox_session_get_from_pool (singbox_main_t *sm, ip4_address_t *dst_addr,
                              u16 dst_port, u32 sw_if_index)
{
  singbox_interface_config_t *config;
  singbox_session_t *session;

  if (!sm->enable_pooling)
    return NULL;

  config = singbox_get_interface_config (sm, sw_if_index);
  if (!config)
    return NULL;

  /* Search for an established connection to the same destination */
  pool_foreach (session, sm->sessions)
    {
      if (session->sw_if_index == sw_if_index &&
          session->state == SINGBOX_STATE_ESTABLISHED &&
          session->dst_addr.as_u32 == dst_addr->as_u32 &&
          session->dst_port == dst_port)
        {
          singbox_session_touch (sm, session);
          return session;
        }
    }

  return NULL;
}
