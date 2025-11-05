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
 * @brief SOCKS5 Protocol Implementation for Sing-box Plugin
 *
 * This file implements the SOCKS5 protocol (RFC 1928) for communication
 * with sing-box proxy. It handles greeting, authentication, and connection
 * establishment phases.
 */

#include <vppinfra/error.h>
#include <vppinfra/vec.h>
#include <vnet/session/session.h>
#include <singbox/singbox.h>

/**
 * @brief Send SOCKS5 greeting message
 *
 * Greeting format:
 * +----+----------+----------+
 * |VER | NMETHODS | METHODS  |
 * +----+----------+----------+
 * | 1  |    1     | 1 to 255 |
 * +----+----------+----------+
 */
int
singbox_socks5_send_greeting (singbox_main_t *sm, singbox_session_t *session)
{
  u8 *data = NULL;
  int rv = 0;

  if (session->greeting_sent)
    return 0;

  /* Build greeting: VER(0x05) + NMETHODS(1 or 2) + METHODS */
  vec_reset_length (data);

  vec_add1 (data, SOCKS5_VERSION);

  if (session->endpoint->auth_method == SOCKS5_AUTH_USERNAME_PASSWORD)
    {
      /* Support both no-auth and username/password */
      vec_add1 (data, 2); /* NMETHODS */
      vec_add1 (data, SOCKS5_AUTH_NONE);
      vec_add1 (data, SOCKS5_AUTH_USERNAME_PASSWORD);
    }
  else
    {
      /* Support only no-auth */
      vec_add1 (data, 1); /* NMETHODS */
      vec_add1 (data, SOCKS5_AUTH_NONE);
    }

  /* Store in tx buffer */
  vec_reset_length (session->tx_buffer);
  vec_append (session->tx_buffer, data);

  session->greeting_sent = 1;
  session->state = SINGBOX_STATE_SOCKS5_GREETING;

  vec_free (data);
  return rv;
}

/**
 * @brief Send SOCKS5 username/password authentication
 *
 * Auth format:
 * +----+------+----------+------+----------+
 * |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
 * +----+------+----------+------+----------+
 * | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
 * +----+------+----------+------+----------+
 */
int
singbox_socks5_send_auth (singbox_main_t *sm, singbox_session_t *session)
{
  u8 *data = NULL;
  int rv = 0;

  if (session->auth_sent || !session->endpoint->username_len)
    return 0;

  vec_reset_length (data);

  /* Version 1 for username/password auth */
  vec_add1 (data, 0x01);

  /* Username */
  vec_add1 (data, session->endpoint->username_len);
  vec_append (data, session->endpoint->username);

  /* Password */
  vec_add1 (data, session->endpoint->password_len);
  vec_append (data, session->endpoint->password);

  /* Store in tx buffer */
  vec_reset_length (session->tx_buffer);
  vec_append (session->tx_buffer, data);

  session->auth_sent = 1;
  session->state = SINGBOX_STATE_SOCKS5_AUTH;

  vec_free (data);
  return rv;
}

/**
 * @brief Send SOCKS5 CONNECT request
 *
 * Request format:
 * +----+-----+-------+------+----------+----------+
 * |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 * +----+-----+-------+------+----------+----------+
 * | 1  |  1  | X'00' |  1   | Variable |    2     |
 * +----+-----+-------+------+----------+----------+
 */
int
singbox_socks5_send_connect (singbox_main_t *sm, singbox_session_t *session)
{
  u8 *data = NULL;
  int rv = 0;

  if (session->request_sent)
    return 0;

  vec_reset_length (data);

  /* VER */
  vec_add1 (data, SOCKS5_VERSION);

  /* CMD - CONNECT */
  vec_add1 (data, SOCKS5_CMD_CONNECT);

  /* RSV - Reserved */
  vec_add1 (data, 0x00);

  /* ATYP - IPv4 */
  vec_add1 (data, SOCKS5_ATYP_IPV4);

  /* DST.ADDR - 4 bytes for IPv4 */
  vec_add (data, session->dst_addr.as_u8, 4);

  /* DST.PORT - 2 bytes in network byte order */
  vec_add1 (data, (session->dst_port >> 8) & 0xff);
  vec_add1 (data, session->dst_port & 0xff);

  /* Store in tx buffer */
  vec_reset_length (session->tx_buffer);
  vec_append (session->tx_buffer, data);

  session->request_sent = 1;
  session->state = SINGBOX_STATE_SOCKS5_REQUEST;

  vec_free (data);
  return rv;
}

/**
 * @brief Process SOCKS5 greeting response
 *
 * Response format:
 * +----+--------+
 * |VER | METHOD |
 * +----+--------+
 * | 1  |   1    |
 * +----+--------+
 */
static int
singbox_socks5_process_greeting_response (singbox_main_t *sm,
                                          singbox_session_t *session,
                                          u8 *data, u32 len)
{
  if (len < 2)
    return -1; /* Need more data */

  if (data[0] != SOCKS5_VERSION)
    {
      clib_warning ("Invalid SOCKS5 version: %d", data[0]);
      return -1;
    }

  u8 method = data[1];

  if (method == 0xFF)
    {
      clib_warning ("No acceptable authentication methods");
      return -1;
    }

  if (method == SOCKS5_AUTH_NONE)
    {
      /* No authentication required, proceed to connect */
      session->state = SINGBOX_STATE_SOCKS5_REQUEST;
      return singbox_socks5_send_connect (sm, session);
    }
  else if (method == SOCKS5_AUTH_USERNAME_PASSWORD)
    {
      /* Authentication required */
      session->state = SINGBOX_STATE_SOCKS5_AUTH;
      return singbox_socks5_send_auth (sm, session);
    }

  clib_warning ("Unsupported authentication method: %d", method);
  return -1;
}

/**
 * @brief Process SOCKS5 authentication response
 *
 * Response format:
 * +----+--------+
 * |VER | STATUS |
 * +----+--------+
 * | 1  |   1    |
 * +----+--------+
 */
static int
singbox_socks5_process_auth_response (singbox_main_t *sm,
                                      singbox_session_t *session, u8 *data,
                                      u32 len)
{
  if (len < 2)
    return -1; /* Need more data */

  if (data[0] != 0x01)
    {
      clib_warning ("Invalid auth response version: %d", data[0]);
      return -1;
    }

  if (data[1] != 0x00)
    {
      clib_warning ("Authentication failed: status %d", data[1]);
      return -1;
    }

  /* Authentication successful, proceed to connect */
  session->state = SINGBOX_STATE_SOCKS5_REQUEST;
  return singbox_socks5_send_connect (sm, session);
}

/**
 * @brief Process SOCKS5 connect response
 *
 * Response format:
 * +----+-----+-------+------+----------+----------+
 * |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 * +----+-----+-------+------+----------+----------+
 * | 1  |  1  | X'00' |  1   | Variable |    2     |
 * +----+-----+-------+------+----------+----------+
 */
static int
singbox_socks5_process_connect_response (singbox_main_t *sm,
                                         singbox_session_t *session,
                                         u8 *data, u32 len)
{
  if (len < 4)
    return -1; /* Need more data */

  if (data[0] != SOCKS5_VERSION)
    {
      clib_warning ("Invalid SOCKS5 version: %d", data[0]);
      return -1;
    }

  u8 reply = data[1];

  if (reply != SOCKS5_REP_SUCCESS)
    {
      clib_warning ("SOCKS5 connect failed: reply code %d", reply);
      session->state = SINGBOX_STATE_ERROR;
      return -1;
    }

  /* Connection established */
  session->state = SINGBOX_STATE_ESTABLISHED;
  session->is_connected = 1;

  if (sm->verbose)
    {
      clib_warning ("SOCKS5 connection established to %U:%d",
                   format_ip4_address, &session->dst_addr,
                   session->dst_port);
    }

  return 0;
}

/**
 * @brief Process SOCKS5 response based on current state
 */
int
singbox_socks5_process_response (singbox_main_t *sm,
                                 singbox_session_t *session, u8 *data,
                                 u32 len)
{
  int rv = 0;

  switch (session->state)
    {
    case SINGBOX_STATE_SOCKS5_GREETING:
      rv = singbox_socks5_process_greeting_response (sm, session, data, len);
      break;

    case SINGBOX_STATE_SOCKS5_AUTH:
      rv = singbox_socks5_process_auth_response (sm, session, data, len);
      break;

    case SINGBOX_STATE_SOCKS5_REQUEST:
      rv = singbox_socks5_process_connect_response (sm, session, data, len);
      break;

    case SINGBOX_STATE_ESTABLISHED:
      /* Data forwarding - handled by caller */
      break;

    default:
      clib_warning ("Unexpected state %d", session->state);
      rv = -1;
      break;
    }

  return rv;
}

/**
 * @brief Format SOCKS5 state
 */
u8 *
format_singbox_session_state (u8 *s, va_list *args)
{
  singbox_session_state_t state = va_arg (*args, singbox_session_state_t);

  switch (state)
    {
#define _(v, str) case SINGBOX_STATE_##v: return format (s, "%s", str);
      foreach_singbox_session_state
#undef _
      default:
        return format (s, "unknown-%d", state);
    }
}

/**
 * @brief Format sing-box session
 */
u8 *
format_singbox_session (u8 *s, va_list *args)
{
  singbox_session_t *session = va_arg (*args, singbox_session_t *);

  s = format (s, "[%d] dst=%U:%d state=%U",
             session->session_index,
             format_ip4_address, &session->dst_addr,
             session->dst_port,
             format_singbox_session_state, session->state);

  if (session->is_connected)
    s = format (s, " connected");

  s = format (s, " tx=%llu rx=%llu errors=%d",
             session->bytes_sent, session->bytes_received,
             session->error_count);

  return s;
}
