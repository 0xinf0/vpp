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
 * @brief Sing-box Plugin - Graph Node Implementation
 *
 * This file implements the packet processing graph node for the sing-box plugin.
 * It processes IPv4 packets and prepares them for redirection to a sing-box proxy.
 */

#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/pg/pg.h>
#include <vnet/ip/ip.h>
#include <vnet/ethernet/ethernet.h>
#include <vppinfra/error.h>
#include <singbox/singbox.h>

typedef struct
{
  u32 next_index;
  u32 sw_if_index;
  ip4_address_t proxy_addr;
  u16 proxy_port;
  u32 packet_length;
} singbox_trace_t;

/* Packet trace format function */
static u8 *
format_singbox_trace (u8 *s, va_list *args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  singbox_trace_t *t = va_arg (*args, singbox_trace_t *);

  s = format (s, "SINGBOX: sw_if_index %d, next index %d\n",
             t->sw_if_index, t->next_index);
  s = format (s, "  proxy %U:%d, packet_len %d",
             format_ip4_address, &t->proxy_addr,
             t->proxy_port, t->packet_length);

  return s;
}

extern vlib_node_registration_t singbox_node;

#define foreach_singbox_error                    \
_(REDIRECTED, "Packets redirected to sing-box")  \
_(NO_CONFIG, "No sing-box config for interface") \
_(DISABLED, "Sing-box disabled on interface")

typedef enum
{
#define _(sym,str) SINGBOX_ERROR_##sym,
  foreach_singbox_error
#undef _
    SINGBOX_N_ERROR,
} singbox_error_t;

static char *singbox_error_strings[] = {
#define _(sym,string) string,
  foreach_singbox_error
#undef _
};

typedef enum
{
  SINGBOX_NEXT_IP4_LOOKUP,
  SINGBOX_NEXT_DROP,
  SINGBOX_N_NEXT,
} singbox_next_t;

/**
 * @brief Process packets and redirect to sing-box
 *
 * This is a simple implementation that marks packets for redirection.
 * In a production implementation, you would:
 * 1. Encapsulate packets in SOCKS5 protocol
 * 2. Rewrite destination to proxy address
 * 3. Handle connection management
 */
VLIB_NODE_FN (singbox_node)
(vlib_main_t *vm, vlib_node_runtime_t *node, vlib_frame_t *frame)
{
  u32 n_left_from, *from, *to_next;
  singbox_next_t next_index;
  singbox_main_t *sm = &singbox_main;
  u32 pkts_redirected = 0;
  u32 pkts_no_config = 0;
  u32 pkts_disabled = 0;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;
  next_index = node->cached_next_index;

  while (n_left_from > 0)
    {
      u32 n_left_to_next;

      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      while (n_left_from >= 4 && n_left_to_next >= 2)
        {
          u32 bi0, bi1;
          vlib_buffer_t *b0, *b1;
          u32 next0 = SINGBOX_NEXT_IP4_LOOKUP;
          u32 next1 = SINGBOX_NEXT_IP4_LOOKUP;
          u32 sw_if_index0, sw_if_index1;
          singbox_interface_config_t *config0, *config1;
          ip4_header_t *ip0, *ip1;

          /* Prefetch next iteration */
          {
            vlib_buffer_t *p2, *p3;

            p2 = vlib_get_buffer (vm, from[2]);
            p3 = vlib_get_buffer (vm, from[3]);

            vlib_prefetch_buffer_header (p2, LOAD);
            vlib_prefetch_buffer_header (p3, LOAD);

            clib_prefetch_load (p2->data);
            clib_prefetch_load (p3->data);
          }

          /* speculatively enqueue b0 and b1 to the current next frame */
          to_next[0] = bi0 = from[0];
          to_next[1] = bi1 = from[1];
          from += 2;
          to_next += 2;
          n_left_from -= 2;
          n_left_to_next -= 2;

          b0 = vlib_get_buffer (vm, bi0);
          b1 = vlib_get_buffer (vm, bi1);

          sw_if_index0 = vnet_buffer (b0)->sw_if_index[VLIB_RX];
          sw_if_index1 = vnet_buffer (b1)->sw_if_index[VLIB_RX];

          /* Get interface configurations */
          config0 = singbox_get_interface_config (sm, sw_if_index0);
          config1 = singbox_get_interface_config (sm, sw_if_index1);

          /* Process packet 0 */
          if (config0 && config0->endpoint.is_enabled)
            {
              ip0 = vlib_buffer_get_current (b0);

              /* Update statistics */
              config0->packets_redirected++;
              config0->bytes_redirected += vlib_buffer_length_in_chain (vm, b0);
              pkts_redirected++;

              /*
               * In a full implementation, you would:
               * 1. Encapsulate in SOCKS5 protocol
               * 2. Rewrite IP header to proxy address
               * 3. Update checksums
               *
               * For now, we just mark it and pass to IP lookup
               */
              next0 = SINGBOX_NEXT_IP4_LOOKUP;
            }
          else
            {
              if (config0)
                pkts_disabled++;
              else
                pkts_no_config++;
              next0 = SINGBOX_NEXT_IP4_LOOKUP;
            }

          /* Process packet 1 */
          if (config1 && config1->endpoint.is_enabled)
            {
              ip1 = vlib_buffer_get_current (b1);

              /* Update statistics */
              config1->packets_redirected++;
              config1->bytes_redirected += vlib_buffer_length_in_chain (vm, b1);
              pkts_redirected++;

              next1 = SINGBOX_NEXT_IP4_LOOKUP;
            }
          else
            {
              if (config1)
                pkts_disabled++;
              else
                pkts_no_config++;
              next1 = SINGBOX_NEXT_IP4_LOOKUP;
            }

          /* Tracing */
          if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE)))
            {
              if (b0->flags & VLIB_BUFFER_IS_TRACED)
                {
                  singbox_trace_t *t =
                    vlib_add_trace (vm, node, b0, sizeof (*t));
                  t->sw_if_index = sw_if_index0;
                  t->next_index = next0;
                  if (config0)
                    {
                      t->proxy_addr = config0->endpoint.proxy_addr;
                      t->proxy_port = config0->endpoint.proxy_port;
                    }
                  t->packet_length = vlib_buffer_length_in_chain (vm, b0);
                }

              if (b1->flags & VLIB_BUFFER_IS_TRACED)
                {
                  singbox_trace_t *t =
                    vlib_add_trace (vm, node, b1, sizeof (*t));
                  t->sw_if_index = sw_if_index1;
                  t->next_index = next1;
                  if (config1)
                    {
                      t->proxy_addr = config1->endpoint.proxy_addr;
                      t->proxy_port = config1->endpoint.proxy_port;
                    }
                  t->packet_length = vlib_buffer_length_in_chain (vm, b1);
                }
            }

          /* verify speculative enqueues, maybe switch current next frame */
          vlib_validate_buffer_enqueue_x2 (vm, node, next_index,
                                          to_next, n_left_to_next,
                                          bi0, bi1, next0, next1);
        }

      while (n_left_from > 0 && n_left_to_next > 0)
        {
          u32 bi0;
          vlib_buffer_t *b0;
          u32 next0 = SINGBOX_NEXT_IP4_LOOKUP;
          u32 sw_if_index0;
          singbox_interface_config_t *config0;
          ip4_header_t *ip0;

          /* speculatively enqueue b0 to the current next frame */
          bi0 = from[0];
          to_next[0] = bi0;
          from += 1;
          to_next += 1;
          n_left_from -= 1;
          n_left_to_next -= 1;

          b0 = vlib_get_buffer (vm, bi0);
          sw_if_index0 = vnet_buffer (b0)->sw_if_index[VLIB_RX];

          /* Get interface configuration */
          config0 = singbox_get_interface_config (sm, sw_if_index0);

          /* Process packet */
          if (config0 && config0->endpoint.is_enabled)
            {
              ip0 = vlib_buffer_get_current (b0);

              /* Update statistics */
              config0->packets_redirected++;
              config0->bytes_redirected += vlib_buffer_length_in_chain (vm, b0);
              pkts_redirected++;

              next0 = SINGBOX_NEXT_IP4_LOOKUP;
            }
          else
            {
              if (config0)
                pkts_disabled++;
              else
                pkts_no_config++;
              next0 = SINGBOX_NEXT_IP4_LOOKUP;
            }

          /* Tracing */
          if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&
                            (b0->flags & VLIB_BUFFER_IS_TRACED)))
            {
              singbox_trace_t *t = vlib_add_trace (vm, node, b0, sizeof (*t));
              t->sw_if_index = sw_if_index0;
              t->next_index = next0;
              if (config0)
                {
                  t->proxy_addr = config0->endpoint.proxy_addr;
                  t->proxy_port = config0->endpoint.proxy_port;
                }
              t->packet_length = vlib_buffer_length_in_chain (vm, b0);
            }

          /* verify speculative enqueue, maybe switch current next frame */
          vlib_validate_buffer_enqueue_x1 (vm, node, next_index,
                                          to_next, n_left_to_next,
                                          bi0, next0);
        }

      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  vlib_node_increment_counter (vm, singbox_node.index,
                              SINGBOX_ERROR_REDIRECTED, pkts_redirected);
  vlib_node_increment_counter (vm, singbox_node.index,
                              SINGBOX_ERROR_NO_CONFIG, pkts_no_config);
  vlib_node_increment_counter (vm, singbox_node.index,
                              SINGBOX_ERROR_DISABLED, pkts_disabled);

  return frame->n_vectors;
}

/* Register the graph node */
VLIB_REGISTER_NODE (singbox_node) = {
  .name = "singbox",
  .vector_size = sizeof (u32),
  .format_trace = format_singbox_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,

  .n_errors = ARRAY_LEN (singbox_error_strings),
  .error_strings = singbox_error_strings,

  .n_next_nodes = SINGBOX_N_NEXT,

  .next_nodes = {
    [SINGBOX_NEXT_IP4_LOOKUP] = "ip4-lookup",
    [SINGBOX_NEXT_DROP] = "error-drop",
  },
};
