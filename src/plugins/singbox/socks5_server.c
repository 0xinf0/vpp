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
 * @brief SOCKS5 Server Implementation for Sing-box Plugin
 *
 * This file implements the SOCKS5 server side (RFC 1928) allowing VPP
 * to act as a SOCKS5 proxy server that sing-box clients (and other SOCKS5
 * clients) can connect to.
 *
 * Architecture:
 * [sing-box client] --SOCKS5--> [VPP SOCKS5 Server] --> Internet
 */

#include <vppinfra/error.h>
#include <vppinfra/vec.h>
#include <vnet/session/session.h>
#include <vnet/session/application.h>
#include <vnet/session/application_interface.h>
#include <singbox/singbox.h>

/**
 * @brief Start SOCKS5 server
 */
int
singbox_server_start (singbox_main_t *sm, ip4_address_t *listen_addr,
                     u16 listen_port, u8 require_auth, u8 *username,
                     u8 *password)
{
  vnet_app_attach_args_t attach_args = { 0 };
  vnet_app_detach_args_t detach_args;
  u64 options[APP_OPTIONS_N_OPTIONS];
  int rv;

  if (sm->server_mode_enabled)
    {
      clib_warning ("SOCKS5 server already running");
      return -1;
    }

  /* Initialize server structures */
  clib_spinlock_init (&sm->server_lock);
  sm->server_session_by_handle = hash_create (0, sizeof (uword));

  /* Set server configuration */
  if (listen_addr)
    sm->server_listen_addr = *listen_addr;
  else
    sm->server_listen_addr.as_u32 = clib_host_to_net_u32 (0x7f000001); /* 127.0.0.1 */

  sm->server_listen_port = listen_port ? listen_port : 1080;
  sm->server_require_auth = require_auth;

  if (require_auth && username && password)
    {
      sm->server_username_len = strlen ((char *) username);
      sm->server_password_len = strlen ((char *) password);
      clib_memcpy (sm->server_username, username, sm->server_username_len);
      clib_memcpy (sm->server_password, password, sm->server_password_len);
    }

  /* Configure application options */
  clib_memset (options, 0, sizeof (options));
  options[APP_OPTIONS_FLAGS] = APP_OPTIONS_FLAGS_IS_BUILTIN;
  options[APP_OPTIONS_FLAGS] |= APP_OPTIONS_FLAGS_USE_GLOBAL_SCOPE;
  options[APP_OPTIONS_FLAGS] |= APP_OPTIONS_FLAGS_IS_TRANSPORT_APP;

  options[APP_OPTIONS_SEGMENT_SIZE] = 512 << 20; /* 512MB */
  options[APP_OPTIONS_ADD_SEGMENT_SIZE] = 256 << 20; /* 256MB */
  options[APP_OPTIONS_RX_FIFO_SIZE] = 64 << 10; /* 64KB */
  options[APP_OPTIONS_TX_FIFO_SIZE] = 64 << 10; /* 64KB */
  options[APP_OPTIONS_EVT_QUEUE_SIZE] = 128;
  options[APP_OPTIONS_PREALLOC_FIFO_PAIRS] = 16;

  attach_args.api_client_index = ~0;
  attach_args.options = options;
  attach_args.namespace_id = 0;
  attach_args.session_cb_vft = &singbox_server_session_cb_vft; /* Need to define this */

  if ((rv = vnet_application_attach (&attach_args)))
    {
      clib_warning ("Failed to attach SOCKS5 server application: %d", rv);
      return rv;
    }

  sm->server_app_index = attach_args.app_index;

  /* Bind to listening address and port */
  session_endpoint_cfg_t sep = { 0 };
  sep.transport_proto = TRANSPORT_PROTO_TCP;
  sep.is_ip4 = 1;
  clib_memcpy (&sep.ip.ip4, &sm->server_listen_addr, sizeof (ip4_address_t));
  sep.port = sm->server_listen_port;

  /* Start listening */
  if ((rv = vnet_listen (&sep)))
    {
      clib_warning ("Failed to listen on %U:%d: %d",
                   format_ip4_address, &sm->server_listen_addr,
                   sm->server_listen_port, rv);

      detach_args.app_index = sm->server_app_index;
      vnet_application_detach (&detach_args);
      return rv;
    }

  sm->server_mode_enabled = 1;

  clib_warning ("SOCKS5 server started on %U:%d (auth: %s)",
               format_ip4_address, &sm->server_listen_addr,
               sm->server_listen_port,
               require_auth ? "required" : "none");

  return 0;
}

/**
 * @brief Stop SOCKS5 server
 */
int
singbox_server_stop (singbox_main_t *sm)
{
  vnet_app_detach_args_t detach_args;
  singbox_session_t *session;

  if (!sm->server_mode_enabled)
    return 0;

  /* Close all active server sessions */
  pool_foreach (session, sm->server_sessions)
    {
      /* Close session - actual implementation would use session layer */
    }

  /* Detach application */
  detach_args.app_index = sm->server_app_index;
  vnet_application_detach (&detach_args);

  /* Cleanup */
  hash_free (sm->server_session_by_handle);
  pool_free (sm->server_sessions);

  sm->server_mode_enabled = 0;

  clib_warning ("SOCKS5 server stopped");

  return 0;
}

/**
 * @brief Process SOCKS5 greeting from client
 *
 * Client sends:
 * +----+----------+----------+
 * |VER | NMETHODS | METHODS  |
 * +----+----------+----------+
 * | 1  |    1     | 1 to 255 |
 * +----+----------+----------+
 *
 * Server responds:
 * +----+--------+
 * |VER | METHOD |
 * +----+--------+
 * | 1  |   1    |
 * +----+--------+
 */
int
singbox_server_process_greeting (singbox_main_t *sm,
                                 singbox_session_t *session, u8 *data,
                                 u32 len)
{
  u8 *response = NULL;
  u8 selected_method;

  if (len < 2)
    {
      clib_warning ("Invalid greeting: too short");
      return -1;
    }

  if (data[0] != SOCKS5_VERSION)
    {
      clib_warning ("Invalid SOCKS5 version: %d", data[0]);
      return -1;
    }

  u8 nmethods = data[1];
  if (len < 2 + nmethods)
    {
      clib_warning ("Invalid greeting: methods length mismatch");
      return -1;
    }

  /* Check supported methods */
  u8 supports_no_auth = 0;
  u8 supports_user_pass = 0;

  for (int i = 0; i < nmethods; i++)
    {
      u8 method = data[2 + i];
      if (method == SOCKS5_AUTH_NONE)
        supports_no_auth = 1;
      else if (method == SOCKS5_AUTH_USERNAME_PASSWORD)
        supports_user_pass = 1;
    }

  /* Select authentication method */
  if (sm->server_require_auth)
    {
      if (supports_user_pass)
        selected_method = SOCKS5_AUTH_USERNAME_PASSWORD;
      else
        selected_method = 0xFF; /* No acceptable methods */
    }
  else
    {
      if (supports_no_auth)
        selected_method = SOCKS5_AUTH_NONE;
      else if (supports_user_pass)
        selected_method = SOCKS5_AUTH_USERNAME_PASSWORD;
      else
        selected_method = 0xFF;
    }

  /* Build response */
  vec_reset_length (response);
  vec_add1 (response, SOCKS5_VERSION);
  vec_add1 (response, selected_method);

  /* Store in tx buffer */
  vec_reset_length (session->tx_buffer);
  vec_append (session->tx_buffer, response);

  if (selected_method == 0xFF)
    {
      sm->server_connections_rejected++;
      session->state = SINGBOX_STATE_ERROR;
      vec_free (response);
      return -1;
    }

  /* Update state */
  if (selected_method == SOCKS5_AUTH_NONE)
    session->state = SINGBOX_STATE_SOCKS5_REQUEST;
  else
    session->state = SINGBOX_STATE_SOCKS5_AUTH;

  sm->server_connections_accepted++;
  vec_free (response);
  return 0;
}

/**
 * @brief Process SOCKS5 authentication from client
 *
 * Client sends:
 * +----+------+----------+------+----------+
 * |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
 * +----+------+----------+------+----------+
 * | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
 * +----+------+----------+------+----------+
 *
 * Server responds:
 * +----+--------+
 * |VER | STATUS |
 * +----+--------+
 * | 1  |   1    |
 * +----+--------+
 */
int
singbox_server_process_auth (singbox_main_t *sm, singbox_session_t *session,
                             u8 *data, u32 len)
{
  u8 *response = NULL;
  u8 status;

  if (len < 2)
    {
      clib_warning ("Invalid auth request: too short");
      return -1;
    }

  if (data[0] != 0x01) /* Auth version */
    {
      clib_warning ("Invalid auth version: %d", data[0]);
      return -1;
    }

  u8 ulen = data[1];
  if (len < 2 + ulen + 1)
    {
      clib_warning ("Invalid auth request: username length");
      return -1;
    }

  u8 *username = &data[2];
  u8 plen = data[2 + ulen];

  if (len < 2 + ulen + 1 + plen)
    {
      clib_warning ("Invalid auth request: password length");
      return -1;
    }

  u8 *password = &data[2 + ulen + 1];

  /* Verify credentials */
  if (ulen == sm->server_username_len &&
      plen == sm->server_password_len &&
      memcmp (username, sm->server_username, ulen) == 0 &&
      memcmp (password, sm->server_password, plen) == 0)
    {
      status = 0x00; /* Success */
      session->state = SINGBOX_STATE_SOCKS5_REQUEST;
    }
  else
    {
      status = 0x01; /* Failure */
      session->state = SINGBOX_STATE_ERROR;
      sm->server_auth_failures++;
    }

  /* Build response */
  vec_reset_length (response);
  vec_add1 (response, 0x01); /* Auth version */
  vec_add1 (response, status);

  /* Store in tx buffer */
  vec_reset_length (session->tx_buffer);
  vec_append (session->tx_buffer, response);

  vec_free (response);
  return status == 0x00 ? 0 : -1;
}

/**
 * @brief Process SOCKS5 connect request from client
 *
 * Client sends:
 * +----+-----+-------+------+----------+----------+
 * |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 * +----+-----+-------+------+----------+----------+
 * | 1  |  1  | X'00' |  1   | Variable |    2     |
 * +----+-----+-------+------+----------+----------+
 *
 * Server responds:
 * +----+-----+-------+------+----------+----------+
 * |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 * +----+-----+-------+------+----------+----------+
 * | 1  |  1  | X'00' |  1   | Variable |    2     |
 * +----+-----+-------+------+----------+----------+
 */
int
singbox_server_process_connect (singbox_main_t *sm,
                                singbox_session_t *session, u8 *data,
                                u32 len)
{
  u8 *response = NULL;
  u8 reply_code = SOCKS5_REP_SUCCESS;

  if (len < 4)
    {
      clib_warning ("Invalid connect request: too short");
      return -1;
    }

  if (data[0] != SOCKS5_VERSION)
    {
      clib_warning ("Invalid SOCKS5 version: %d", data[0]);
      return -1;
    }

  u8 cmd = data[1];
  u8 atyp = data[3];

  if (cmd != SOCKS5_CMD_CONNECT)
    {
      clib_warning ("Unsupported command: %d", cmd);
      reply_code = 0x07; /* Command not supported */
    }
  else if (atyp == SOCKS5_ATYP_IPV4)
    {
      if (len < 10) /* VER + CMD + RSV + ATYP + 4(IP) + 2(PORT) */
        {
          clib_warning ("Invalid IPv4 connect request");
          reply_code = 0x01; /* General failure */
        }
      else
        {
          /* Extract destination */
          clib_memcpy (&session->dst_addr, &data[4], 4);
          session->dst_port = (data[8] << 8) | data[9];

          if (sm->verbose)
            {
              clib_warning ("SOCKS5 server: Connect to %U:%d",
                           format_ip4_address, &session->dst_addr,
                           session->dst_port);
            }

          /* In full implementation: establish connection to destination */
          /* For now, just accept the request */
          reply_code = SOCKS5_REP_SUCCESS;
          session->state = SINGBOX_STATE_ESTABLISHED;
        }
    }
  else if (atyp == SOCKS5_ATYP_DOMAINNAME)
    {
      reply_code = 0x08; /* Address type not supported (for now) */
    }
  else if (atyp == SOCKS5_ATYP_IPV6)
    {
      reply_code = 0x08; /* Address type not supported (for now) */
    }
  else
    {
      reply_code = 0x08; /* Address type not supported */
    }

  /* Build response */
  vec_reset_length (response);
  vec_add1 (response, SOCKS5_VERSION);
  vec_add1 (response, reply_code);
  vec_add1 (response, 0x00); /* Reserved */
  vec_add1 (response, SOCKS5_ATYP_IPV4);

  /* Bind address (use 0.0.0.0) */
  vec_add1 (response, 0);
  vec_add1 (response, 0);
  vec_add1 (response, 0);
  vec_add1 (response, 0);

  /* Bind port (use 0) */
  vec_add1 (response, 0);
  vec_add1 (response, 0);

  /* Store in tx buffer */
  vec_reset_length (session->tx_buffer);
  vec_append (session->tx_buffer, response);

  vec_free (response);
  return reply_code == SOCKS5_REP_SUCCESS ? 0 : -1;
}
