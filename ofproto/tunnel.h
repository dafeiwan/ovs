/* Copyright (c) 2013, 2015 Nicira, Inc.
 *
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

#ifndef TUNNEL_H
#define TUNNEL_H 1

#include <stdbool.h>
#include <stdint.h>
#include "flow.h"

/* Tunnel port emulation layer.
 *
 * These functions emulate tunnel virtual ports based on the outer
 * header information from the kernel. */

struct ovs_action_push_tnl;
struct ofport_dpif;
struct netdev;

void ofproto_tunnel_init(void);
bool tnl_port_reconfigure(const struct ofport_dpif *, const struct netdev *,
                          odp_port_t, bool native_tnl, const char name[]);

int tnl_port_add(const struct ofport_dpif *, const struct netdev *,
                 odp_port_t odp_port, bool native_tnl, const char name[]);
void tnl_port_del(const struct ofport_dpif *);

const struct ofport_dpif *tnl_port_receive(const struct flow *);
void tnl_wc_init(struct flow *, struct flow_wildcards *);
bool tnl_process_ecn(struct flow *);
odp_port_t tnl_port_send(const struct ofport_dpif *, struct flow *,
                         struct flow_wildcards *wc);

/* Returns true if 'flow' should be submitted to tnl_port_receive(). */
static inline bool
tnl_port_should_receive(const struct flow *flow)
{
    return flow_tnl_dst_is_set(&flow->tunnel);
}

int tnl_port_build_header(const struct ofport_dpif *ofport,
                          const struct flow *tnl_flow,
                          const struct eth_addr dmac,
                          const struct eth_addr smac,
                          ovs_be32 ip_src, struct ovs_action_push_tnl *data);

#endif /* tunnel.h */
