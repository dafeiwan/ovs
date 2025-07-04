/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Nicira, Inc.
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

#ifndef PACKETS_H
#define PACKETS_H 1

#include <inttypes.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include "compiler.h"
#include "geneve.h"
#include "openvswitch/types.h"
#include "odp-netlink.h"
#include "random.h"
#include "hash.h"
#include "tun-metadata.h"
#include "unaligned.h"
#include "util.h"

struct dp_packet;
struct ds;

/* Tunnel information used in flow key and metadata. */
struct flow_tnl {
    ovs_be32 ip_dst;
    struct in6_addr ipv6_dst;
    ovs_be32 ip_src;
    struct in6_addr ipv6_src;
    ovs_be64 tun_id;
    uint16_t flags;
    uint8_t ip_tos;
    uint8_t ip_ttl;
    ovs_be16 tp_src;
    ovs_be16 tp_dst;
    ovs_be16 gbp_id;
    uint8_t  gbp_flags;
    uint8_t  pad1[5];        /* Pad to 64 bits. */
    struct tun_metadata metadata;
};

/* Some flags are exposed through OpenFlow while others are used only
 * internally. */

/* Public flags */
#define FLOW_TNL_F_OAM (1 << 0)

#define FLOW_TNL_PUB_F_MASK ((1 << 1) - 1)

/* Private flags */
#define FLOW_TNL_F_DONT_FRAGMENT (1 << 1)
#define FLOW_TNL_F_CSUM (1 << 2)
#define FLOW_TNL_F_KEY (1 << 3)

#define FLOW_TNL_F_MASK ((1 << 4) - 1)

/* Purely internal to OVS userspace. These flags should never be exposed to
 * the outside world and so aren't included in the flags mask. */

/* Tunnel information is in userspace datapath format. */
#define FLOW_TNL_F_UDPIF (1 << 4)

static inline bool ipv6_addr_is_set(const struct in6_addr *addr);

static inline bool
flow_tnl_dst_is_set(const struct flow_tnl *tnl)
{
    return tnl->ip_dst || ipv6_addr_is_set(&tnl->ipv6_dst);
}

struct in6_addr flow_tnl_dst(const struct flow_tnl *tnl);
struct in6_addr flow_tnl_src(const struct flow_tnl *tnl);

/* Returns an offset to 'src' covering all the meaningful fields in 'src'. */
static inline size_t
flow_tnl_size(const struct flow_tnl *src)
{
    if (!flow_tnl_dst_is_set(src)) {
        /* Covers ip_dst and ipv6_dst only. */
        return offsetof(struct flow_tnl, ip_src);
    }
    if (src->flags & FLOW_TNL_F_UDPIF) {
        /* Datapath format, cover all options we have. */
        return offsetof(struct flow_tnl, metadata.opts)
            + src->metadata.present.len;
    }
    if (!src->metadata.present.map) {
        /* No TLVs, opts is irrelevant. */
        return offsetof(struct flow_tnl, metadata.opts);
    }
    /* Have decoded TLVs, opts is relevant. */
    return sizeof *src;
}

/* Copy flow_tnl, but avoid copying unused portions of tun_metadata.  Unused
 * data in 'dst' is NOT cleared, so this must not be used in cases where the
 * uninitialized portion may be hashed over. */
static inline void
flow_tnl_copy__(struct flow_tnl *dst, const struct flow_tnl *src)
{
    memcpy(dst, src, flow_tnl_size(src));
}

static inline bool
flow_tnl_equal(const struct flow_tnl *a, const struct flow_tnl *b)
{
    size_t a_size = flow_tnl_size(a);

    return a_size == flow_tnl_size(b) && !memcmp(a, b, a_size);
}

/* Unfortunately, a "struct flow" sometimes has to handle OpenFlow port
 * numbers and other times datapath (dpif) port numbers.  This union allows
 * access to both. */
union flow_in_port {
    odp_port_t odp_port;
    ofp_port_t ofp_port;
};

/* Datapath packet metadata */
struct pkt_metadata {
    uint32_t recirc_id;         /* Recirculation id carried with the
                                   recirculating packets. 0 for packets
                                   received from the wire. */
    uint32_t dp_hash;           /* hash value computed by the recirculation
                                   action. */
    uint32_t skb_priority;      /* Packet priority for QoS. */
    uint32_t pkt_mark;          /* Packet mark. */
    uint16_t ct_state;          /* Connection state. */
    uint16_t ct_zone;           /* Connection zone. */
    uint32_t ct_mark;           /* Connection mark. */
    ovs_u128 ct_label;          /* Connection label. */
    union flow_in_port in_port; /* Input port. */
    struct flow_tnl tunnel;     /* Encapsulating tunnel parameters. Note that
                                 * if 'ip_dst' == 0, the rest of the fields may
                                 * be uninitialized. */
};

static inline void
pkt_metadata_init(struct pkt_metadata *md, odp_port_t port)
{
    /* It can be expensive to zero out all of the tunnel metadata. However,
     * we can just zero out ip_dst and the rest of the data will never be
     * looked at. */
    memset(md, 0, offsetof(struct pkt_metadata, in_port));
    md->tunnel.ip_dst = 0;
    md->tunnel.ipv6_dst = in6addr_any;

    md->in_port.odp_port = port;
}

bool dpid_from_string(const char *s, uint64_t *dpidp);

#define ETH_ADDR_LEN           6

static const struct eth_addr eth_addr_broadcast OVS_UNUSED
    = { { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } } };

static const struct eth_addr eth_addr_exact OVS_UNUSED
    = { { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } } };

static const struct eth_addr eth_addr_zero OVS_UNUSED
    = { { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } } };

static const struct eth_addr eth_addr_stp OVS_UNUSED
    = { { { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x00 } } };

static const struct eth_addr eth_addr_lacp OVS_UNUSED
    = { { { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x02 } } };

static const struct eth_addr eth_addr_bfd OVS_UNUSED
    = { { { 0x00, 0x23, 0x20, 0x00, 0x00, 0x01 } } };

static inline bool eth_addr_is_broadcast(const struct eth_addr a)
{
    return (a.be16[0] & a.be16[1] & a.be16[2]) == htons(0xffff);
}

static inline bool eth_addr_is_multicast(const struct eth_addr a)
{
    return a.ea[0] & 1;
}

static inline bool eth_addr_is_local(const struct eth_addr a)
{
    /* Local if it is either a locally administered address or a Nicira random
     * address. */
    return a.ea[0] & 2
        || (a.be16[0] == htons(0x0023)
            && (a.be16[1] & htons(0xff80)) == htons(0x2080));
}
static inline bool eth_addr_is_zero(const struct eth_addr a)
{
    return !(a.be16[0] | a.be16[1] | a.be16[2]);
}

static inline int eth_mask_is_exact(const struct eth_addr a)
{
    return (a.be16[0] & a.be16[1] & a.be16[2]) == htons(0xffff);
}

static inline int eth_addr_compare_3way(const struct eth_addr a,
                                        const struct eth_addr b)
{
    return memcmp(&a, &b, sizeof a);
}

static inline bool eth_addr_equals(const struct eth_addr a,
                                   const struct eth_addr b)
{
    return !eth_addr_compare_3way(a, b);
}

static inline bool eth_addr_equal_except(const struct eth_addr a,
                                         const struct eth_addr b,
                                         const struct eth_addr mask)
{
    return !(((a.be16[0] ^ b.be16[0]) & mask.be16[0])
             || ((a.be16[1] ^ b.be16[1]) & mask.be16[1])
             || ((a.be16[2] ^ b.be16[2]) & mask.be16[2]));
}

static inline uint64_t eth_addr_to_uint64(const struct eth_addr ea)
{
    return (((uint64_t) ntohs(ea.be16[0]) << 32)
            | ((uint64_t) ntohs(ea.be16[1]) << 16)
            | ntohs(ea.be16[2]));
}

static inline uint64_t eth_addr_vlan_to_uint64(const struct eth_addr ea,
                                               uint16_t vlan)
{
    return (((uint64_t)vlan << 48) | eth_addr_to_uint64(ea));
}

static inline void eth_addr_from_uint64(uint64_t x, struct eth_addr *ea)
{
    ea->be16[0] = htons(x >> 32);
    ea->be16[1] = htons((x & 0xFFFF0000) >> 16);
    ea->be16[2] = htons(x & 0xFFFF);
}

static inline struct eth_addr eth_addr_invert(const struct eth_addr src)
{
    struct eth_addr dst;

    for (int i = 0; i < ARRAY_SIZE(src.be16); i++) {
        dst.be16[i] = ~src.be16[i];
    }

    return dst;
}

static inline void eth_addr_mark_random(struct eth_addr *ea)
{
    ea->ea[0] &= ~1;                /* Unicast. */
    ea->ea[0] |= 2;                 /* Private. */
}

static inline void eth_addr_random(struct eth_addr *ea)
{
    random_bytes((uint8_t *)ea, sizeof *ea);
    eth_addr_mark_random(ea);
}

static inline void eth_addr_nicira_random(struct eth_addr *ea)
{
    eth_addr_random(ea);

    /* Set the OUI to the Nicira one. */
    ea->ea[0] = 0x00;
    ea->ea[1] = 0x23;
    ea->ea[2] = 0x20;

    /* Set the top bit to indicate random Nicira address. */
    ea->ea[3] |= 0x80;
}
static inline uint32_t hash_mac(const struct eth_addr ea,
                                const uint16_t vlan, const uint32_t basis)
{
    return hash_uint64_basis(eth_addr_vlan_to_uint64(ea, vlan), basis);
}

bool eth_addr_is_reserved(const struct eth_addr);
bool eth_addr_from_string(const char *, struct eth_addr *);

void compose_rarp(struct dp_packet *, const struct eth_addr);

void eth_push_vlan(struct dp_packet *, ovs_be16 tpid, ovs_be16 tci);
void eth_pop_vlan(struct dp_packet *);

const char *eth_from_hex(const char *hex, struct dp_packet **packetp);
void eth_format_masked(const struct eth_addr ea,
                       const struct eth_addr *mask, struct ds *s);

void set_mpls_lse(struct dp_packet *, ovs_be32 label);
void push_mpls(struct dp_packet *packet, ovs_be16 ethtype, ovs_be32 lse);
void pop_mpls(struct dp_packet *, ovs_be16 ethtype);

void set_mpls_lse_ttl(ovs_be32 *lse, uint8_t ttl);
void set_mpls_lse_tc(ovs_be32 *lse, uint8_t tc);
void set_mpls_lse_label(ovs_be32 *lse, ovs_be32 label);
void set_mpls_lse_bos(ovs_be32 *lse, uint8_t bos);
ovs_be32 set_mpls_lse_values(uint8_t ttl, uint8_t tc, uint8_t bos,
                             ovs_be32 label);

/* Example:
 *
 * struct eth_addr mac;
 *    [...]
 * printf("The Ethernet address is "ETH_ADDR_FMT"\n", ETH_ADDR_ARGS(mac));
 *
 */
#define ETH_ADDR_FMT                                                    \
    "%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8
#define ETH_ADDR_ARGS(EA) ETH_ADDR_BYTES_ARGS((EA).ea)
#define ETH_ADDR_BYTES_ARGS(EAB) \
         (EAB)[0], (EAB)[1], (EAB)[2], (EAB)[3], (EAB)[4], (EAB)[5]

/* Example:
 *
 * char *string = "1 00:11:22:33:44:55 2";
 * struct eth_addr mac;
 * int a, b;
 *
 * if (ovs_scan(string, "%d"ETH_ADDR_SCAN_FMT"%d",
 *              &a, ETH_ADDR_SCAN_ARGS(mac), &b)) {
 *     ...
 * }
 */
#define ETH_ADDR_SCAN_FMT "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8
#define ETH_ADDR_SCAN_ARGS(EA) \
    &(EA).ea[0], &(EA).ea[1], &(EA).ea[2], &(EA).ea[3], &(EA).ea[4], &(EA).ea[5]

#define ETH_TYPE_IP            0x0800
#define ETH_TYPE_ARP           0x0806
#define ETH_TYPE_TEB           0x6558
#define ETH_TYPE_VLAN_8021Q    0x8100
#define ETH_TYPE_VLAN          ETH_TYPE_VLAN_8021Q
#define ETH_TYPE_VLAN_8021AD   0x88a8
#define ETH_TYPE_IPV6          0x86dd
#define ETH_TYPE_LACP          0x8809
#define ETH_TYPE_RARP          0x8035
#define ETH_TYPE_MPLS          0x8847
#define ETH_TYPE_MPLS_MCAST    0x8848

static inline bool eth_type_mpls(ovs_be16 eth_type)
{
    return eth_type == htons(ETH_TYPE_MPLS) ||
        eth_type == htons(ETH_TYPE_MPLS_MCAST);
}

static inline bool eth_type_vlan(ovs_be16 eth_type)
{
    return eth_type == htons(ETH_TYPE_VLAN_8021Q) ||
        eth_type == htons(ETH_TYPE_VLAN_8021AD);
}


/* Minimum value for an Ethernet type.  Values below this are IEEE 802.2 frame
 * lengths. */
#define ETH_TYPE_MIN           0x600

#define ETH_HEADER_LEN 14
#define ETH_PAYLOAD_MIN 46
#define ETH_PAYLOAD_MAX 1500
#define ETH_TOTAL_MIN (ETH_HEADER_LEN + ETH_PAYLOAD_MIN)
#define ETH_TOTAL_MAX (ETH_HEADER_LEN + ETH_PAYLOAD_MAX)
#define ETH_VLAN_TOTAL_MAX (ETH_HEADER_LEN + VLAN_HEADER_LEN + ETH_PAYLOAD_MAX)
OVS_PACKED(
struct eth_header {
    struct eth_addr eth_dst;
    struct eth_addr eth_src;
    ovs_be16 eth_type;
});
BUILD_ASSERT_DECL(ETH_HEADER_LEN == sizeof(struct eth_header));

#define LLC_DSAP_SNAP 0xaa
#define LLC_SSAP_SNAP 0xaa
#define LLC_CNTL_SNAP 3

#define LLC_HEADER_LEN 3
OVS_PACKED(
struct llc_header {
    uint8_t llc_dsap;
    uint8_t llc_ssap;
    uint8_t llc_cntl;
});
BUILD_ASSERT_DECL(LLC_HEADER_LEN == sizeof(struct llc_header));

/* LLC field values used for STP frames. */
#define STP_LLC_SSAP 0x42
#define STP_LLC_DSAP 0x42
#define STP_LLC_CNTL 0x03

#define SNAP_ORG_ETHERNET "\0\0" /* The compiler adds a null byte, so
                                    sizeof(SNAP_ORG_ETHERNET) == 3. */
#define SNAP_HEADER_LEN 5
OVS_PACKED(
struct snap_header {
    uint8_t snap_org[3];
    ovs_be16 snap_type;
});
BUILD_ASSERT_DECL(SNAP_HEADER_LEN == sizeof(struct snap_header));

#define LLC_SNAP_HEADER_LEN (LLC_HEADER_LEN + SNAP_HEADER_LEN)
OVS_PACKED(
struct llc_snap_header {
    struct llc_header llc;
    struct snap_header snap;
});
BUILD_ASSERT_DECL(LLC_SNAP_HEADER_LEN == sizeof(struct llc_snap_header));

#define VLAN_VID_MASK 0x0fff
#define VLAN_VID_SHIFT 0

#define VLAN_PCP_MASK 0xe000
#define VLAN_PCP_SHIFT 13

#define VLAN_CFI 0x1000
#define VLAN_CFI_SHIFT 12

/* Given the vlan_tci field from an 802.1Q header, in network byte order,
 * returns the VLAN ID in host byte order. */
static inline uint16_t
vlan_tci_to_vid(ovs_be16 vlan_tci)
{
    return (ntohs(vlan_tci) & VLAN_VID_MASK) >> VLAN_VID_SHIFT;
}

/* Given the vlan_tci field from an 802.1Q header, in network byte order,
 * returns the priority code point (PCP) in host byte order. */
static inline int
vlan_tci_to_pcp(ovs_be16 vlan_tci)
{
    return (ntohs(vlan_tci) & VLAN_PCP_MASK) >> VLAN_PCP_SHIFT;
}

/* Given the vlan_tci field from an 802.1Q header, in network byte order,
 * returns the Canonical Format Indicator (CFI). */
static inline int
vlan_tci_to_cfi(ovs_be16 vlan_tci)
{
    return (vlan_tci & htons(VLAN_CFI)) != 0;
}

#define VLAN_HEADER_LEN 4
struct vlan_header {
    ovs_be16 vlan_tci;          /* Lowest 12 bits are VLAN ID. */
    ovs_be16 vlan_next_type;
};
BUILD_ASSERT_DECL(VLAN_HEADER_LEN == sizeof(struct vlan_header));

#define VLAN_ETH_HEADER_LEN (ETH_HEADER_LEN + VLAN_HEADER_LEN)
OVS_PACKED(
struct vlan_eth_header {
    struct eth_addr veth_dst;
    struct eth_addr veth_src;
    ovs_be16 veth_type;         /* Always htons(ETH_TYPE_VLAN). */
    ovs_be16 veth_tci;          /* Lowest 12 bits are VLAN ID. */
    ovs_be16 veth_next_type;
});
BUILD_ASSERT_DECL(VLAN_ETH_HEADER_LEN == sizeof(struct vlan_eth_header));

/* MPLS related definitions */
#define MPLS_TTL_MASK       0x000000ff
#define MPLS_TTL_SHIFT      0

#define MPLS_BOS_MASK       0x00000100
#define MPLS_BOS_SHIFT      8

#define MPLS_TC_MASK        0x00000e00
#define MPLS_TC_SHIFT       9

#define MPLS_LABEL_MASK     0xfffff000
#define MPLS_LABEL_SHIFT    12

#define MPLS_HLEN           4

struct mpls_hdr {
    ovs_16aligned_be32 mpls_lse;
};
BUILD_ASSERT_DECL(MPLS_HLEN == sizeof(struct mpls_hdr));

/* Given a mpls label stack entry in network byte order
 * return mpls label in host byte order */
static inline uint32_t
mpls_lse_to_label(ovs_be32 mpls_lse)
{
    return (ntohl(mpls_lse) & MPLS_LABEL_MASK) >> MPLS_LABEL_SHIFT;
}

/* Given a mpls label stack entry in network byte order
 * return mpls tc */
static inline uint8_t
mpls_lse_to_tc(ovs_be32 mpls_lse)
{
    return (ntohl(mpls_lse) & MPLS_TC_MASK) >> MPLS_TC_SHIFT;
}

/* Given a mpls label stack entry in network byte order
 * return mpls ttl */
static inline uint8_t
mpls_lse_to_ttl(ovs_be32 mpls_lse)
{
    return (ntohl(mpls_lse) & MPLS_TTL_MASK) >> MPLS_TTL_SHIFT;
}

/* Set TTL in mpls lse. */
static inline void
flow_set_mpls_lse_ttl(ovs_be32 *mpls_lse, uint8_t ttl)
{
    *mpls_lse &= ~htonl(MPLS_TTL_MASK);
    *mpls_lse |= htonl(ttl << MPLS_TTL_SHIFT);
}

/* Given a mpls label stack entry in network byte order
 * return mpls BoS bit  */
static inline uint8_t
mpls_lse_to_bos(ovs_be32 mpls_lse)
{
    return (mpls_lse & htonl(MPLS_BOS_MASK)) != 0;
}

#define IP_FMT "%"PRIu32".%"PRIu32".%"PRIu32".%"PRIu32
#define IP_ARGS(ip)                             \
    ntohl(ip) >> 24,                            \
    (ntohl(ip) >> 16) & 0xff,                   \
    (ntohl(ip) >> 8) & 0xff,                    \
    ntohl(ip) & 0xff

/* Example:
 *
 * char *string = "1 33.44.55.66 2";
 * ovs_be32 ip;
 * int a, b;
 *
 * if (ovs_scan(string, "%d"IP_SCAN_FMT"%d", &a, IP_SCAN_ARGS(&ip), &b)) {
 *     ...
 * }
 */
#define IP_SCAN_FMT "%"SCNu8".%"SCNu8".%"SCNu8".%"SCNu8
#define IP_SCAN_ARGS(ip)                                    \
        ((void) (ovs_be32) *(ip), &((uint8_t *) ip)[0]),    \
        &((uint8_t *) ip)[1],                               \
        &((uint8_t *) ip)[2],                               \
        &((uint8_t *) ip)[3]

/* Returns true if 'netmask' is a CIDR netmask, that is, if it consists of N
 * high-order 1-bits and 32-N low-order 0-bits. */
static inline bool
ip_is_cidr(ovs_be32 netmask)
{
    uint32_t x = ~ntohl(netmask);
    return !(x & (x + 1));
}
static inline bool
ip_is_multicast(ovs_be32 ip)
{
    return (ip & htonl(0xf0000000)) == htonl(0xe0000000);
}
static inline bool
ip_is_local_multicast(ovs_be32 ip)
{
    return (ip & htonl(0xffffff00)) == htonl(0xe0000000);
}
int ip_count_cidr_bits(ovs_be32 netmask);
void ip_format_masked(ovs_be32 ip, ovs_be32 mask, struct ds *);
char *ip_parse_masked(const char *s, ovs_be32 *ip, ovs_be32 *mask)
    OVS_WARN_UNUSED_RESULT;

#define IP_VER(ip_ihl_ver) ((ip_ihl_ver) >> 4)
#define IP_IHL(ip_ihl_ver) ((ip_ihl_ver) & 15)
#define IP_IHL_VER(ihl, ver) (((ver) << 4) | (ihl))

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

/* TOS fields. */
#define IP_ECN_NOT_ECT 0x0
#define IP_ECN_ECT_1 0x01
#define IP_ECN_ECT_0 0x02
#define IP_ECN_CE 0x03
#define IP_ECN_MASK 0x03
#define IP_DSCP_MASK 0xfc

#define IP_VERSION 4

#define IP_DONT_FRAGMENT  0x4000 /* Don't fragment. */
#define IP_MORE_FRAGMENTS 0x2000 /* More fragments. */
#define IP_FRAG_OFF_MASK  0x1fff /* Fragment offset. */
#define IP_IS_FRAGMENT(ip_frag_off) \
        ((ip_frag_off) & htons(IP_MORE_FRAGMENTS | IP_FRAG_OFF_MASK))

#define IP_HEADER_LEN 20
struct ip_header {
    uint8_t ip_ihl_ver;
    uint8_t ip_tos;
    ovs_be16 ip_tot_len;
    ovs_be16 ip_id;
    ovs_be16 ip_frag_off;
    uint8_t ip_ttl;
    uint8_t ip_proto;
    ovs_be16 ip_csum;
    ovs_16aligned_be32 ip_src;
    ovs_16aligned_be32 ip_dst;
};

BUILD_ASSERT_DECL(IP_HEADER_LEN == sizeof(struct ip_header));

#define ICMP_HEADER_LEN 8
struct icmp_header {
    uint8_t icmp_type;
    uint8_t icmp_code;
    ovs_be16 icmp_csum;
    union {
        struct {
            ovs_be16 id;
            ovs_be16 seq;
        } echo;
        struct {
            ovs_be16 empty;
            ovs_be16 mtu;
        } frag;
        ovs_16aligned_be32 gateway;
    } icmp_fields;
};
BUILD_ASSERT_DECL(ICMP_HEADER_LEN == sizeof(struct icmp_header));

#define IGMP_HEADER_LEN 8
struct igmp_header {
    uint8_t igmp_type;
    uint8_t igmp_code;
    ovs_be16 igmp_csum;
    ovs_16aligned_be32 group;
};
BUILD_ASSERT_DECL(IGMP_HEADER_LEN == sizeof(struct igmp_header));

#define IGMPV3_HEADER_LEN 8
struct igmpv3_header {
    uint8_t type;
    uint8_t rsvr1;
    ovs_be16 csum;
    ovs_be16 rsvr2;
    ovs_be16 ngrp;
};
BUILD_ASSERT_DECL(IGMPV3_HEADER_LEN == sizeof(struct igmpv3_header));

#define IGMPV3_RECORD_LEN 8
struct igmpv3_record {
    uint8_t type;
    uint8_t aux_len;
    ovs_be16 nsrcs;
    ovs_16aligned_be32 maddr;
};
BUILD_ASSERT_DECL(IGMPV3_RECORD_LEN == sizeof(struct igmpv3_record));

#define IGMP_HOST_MEMBERSHIP_QUERY    0x11 /* From RFC1112 */
#define IGMP_HOST_MEMBERSHIP_REPORT   0x12 /* Ditto */
#define IGMPV2_HOST_MEMBERSHIP_REPORT 0x16 /* V2 version of 0x12 */
#define IGMP_HOST_LEAVE_MESSAGE       0x17
#define IGMPV3_HOST_MEMBERSHIP_REPORT 0x22 /* V3 version of 0x12 */

/*
 * IGMPv3 and MLDv2 use the same codes.
 */
#define IGMPV3_MODE_IS_INCLUDE 1
#define IGMPV3_MODE_IS_EXCLUDE 2
#define IGMPV3_CHANGE_TO_INCLUDE_MODE 3
#define IGMPV3_CHANGE_TO_EXCLUDE_MODE 4
#define IGMPV3_ALLOW_NEW_SOURCES 5
#define IGMPV3_BLOCK_OLD_SOURCES 6

#define SCTP_HEADER_LEN 12
struct sctp_header {
    ovs_be16 sctp_src;
    ovs_be16 sctp_dst;
    ovs_16aligned_be32 sctp_vtag;
    ovs_16aligned_be32 sctp_csum;
};
BUILD_ASSERT_DECL(SCTP_HEADER_LEN == sizeof(struct sctp_header));

#define UDP_HEADER_LEN 8
struct udp_header {
    ovs_be16 udp_src;
    ovs_be16 udp_dst;
    ovs_be16 udp_len;
    ovs_be16 udp_csum;
};
BUILD_ASSERT_DECL(UDP_HEADER_LEN == sizeof(struct udp_header));

#define TCP_FIN 0x001
#define TCP_SYN 0x002
#define TCP_RST 0x004
#define TCP_PSH 0x008
#define TCP_ACK 0x010
#define TCP_URG 0x020
#define TCP_ECE 0x040
#define TCP_CWR 0x080
#define TCP_NS  0x100

#define TCP_CTL(flags, offset) (htons((flags) | ((offset) << 12)))
#define TCP_FLAGS(tcp_ctl) (ntohs(tcp_ctl) & 0x0fff)
#define TCP_FLAGS_BE16(tcp_ctl) ((tcp_ctl) & htons(0x0fff))
#define TCP_OFFSET(tcp_ctl) (ntohs(tcp_ctl) >> 12)

#define TCP_HEADER_LEN 20
struct tcp_header {
    ovs_be16 tcp_src;
    ovs_be16 tcp_dst;
    ovs_16aligned_be32 tcp_seq;
    ovs_16aligned_be32 tcp_ack;
    ovs_be16 tcp_ctl;
    ovs_be16 tcp_winsz;
    ovs_be16 tcp_csum;
    ovs_be16 tcp_urg;
};
BUILD_ASSERT_DECL(TCP_HEADER_LEN == sizeof(struct tcp_header));

/* Connection states */
#define CS_NEW               0x01
#define CS_ESTABLISHED       0x02
#define CS_RELATED           0x04
#define CS_REPLY_DIR         0x08
#define CS_INVALID           0x10
#define CS_TRACKED           0x20
#define CS_SRC_NAT           0x40
#define CS_DST_NAT           0x80

/* Undefined connection state bits. */
#define CS_SUPPORTED_MASK    (CS_NEW | CS_ESTABLISHED | CS_RELATED \
                              | CS_INVALID | CS_REPLY_DIR | CS_TRACKED \
                              | CS_SRC_NAT | CS_DST_NAT)
#define CS_UNSUPPORTED_MASK  (~(uint32_t)CS_SUPPORTED_MASK)

#define ARP_HRD_ETHERNET 1
#define ARP_PRO_IP 0x0800
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2
#define ARP_OP_RARP 3

#define ARP_ETH_HEADER_LEN 28
struct arp_eth_header {
    /* Generic members. */
    ovs_be16 ar_hrd;           /* Hardware type. */
    ovs_be16 ar_pro;           /* Protocol type. */
    uint8_t ar_hln;            /* Hardware address length. */
    uint8_t ar_pln;            /* Protocol address length. */
    ovs_be16 ar_op;            /* Opcode. */

    /* Ethernet+IPv4 specific members. */
    struct eth_addr ar_sha;     /* Sender hardware address. */
    ovs_16aligned_be32 ar_spa;  /* Sender protocol address. */
    struct eth_addr ar_tha;     /* Target hardware address. */
    ovs_16aligned_be32 ar_tpa;  /* Target protocol address. */
};
BUILD_ASSERT_DECL(ARP_ETH_HEADER_LEN == sizeof(struct arp_eth_header));

/* Like struct in6_addr, but whereas that struct requires 32-bit alignment on
 * most implementations, this one only requires 16-bit alignment. */
union ovs_16aligned_in6_addr {
    ovs_be16 be16[8];
    ovs_16aligned_be32 be32[4];
};

/* Like struct in6_hdr, but whereas that struct requires 32-bit alignment, this
 * one only requires 16-bit alignment. */
struct ovs_16aligned_ip6_hdr {
    union {
        struct ovs_16aligned_ip6_hdrctl {
            ovs_16aligned_be32 ip6_un1_flow;
            ovs_be16 ip6_un1_plen;
            uint8_t ip6_un1_nxt;
            uint8_t ip6_un1_hlim;
        } ip6_un1;
        uint8_t ip6_un2_vfc;
    } ip6_ctlun;
    union ovs_16aligned_in6_addr ip6_src;
    union ovs_16aligned_in6_addr ip6_dst;
};

/* Like struct in6_frag, but whereas that struct requires 32-bit alignment,
 * this one only requires 16-bit alignment. */
struct ovs_16aligned_ip6_frag {
    uint8_t ip6f_nxt;
    uint8_t ip6f_reserved;
    ovs_be16 ip6f_offlg;
    ovs_16aligned_be32 ip6f_ident;
};

#define ICMP6_HEADER_LEN 4
struct icmp6_header {
    uint8_t icmp6_type;
    uint8_t icmp6_code;
    ovs_be16 icmp6_cksum;
};
BUILD_ASSERT_DECL(ICMP6_HEADER_LEN == sizeof(struct icmp6_header));

/* Neighbor Discovery option field.
 * ND options are always a multiple of 8 bytes in size. */
#define ND_OPT_LEN 8
struct ovs_nd_opt {
    uint8_t  nd_opt_type;      /* Values defined in icmp6.h */
    uint8_t  nd_opt_len;       /* in units of 8 octets (the size of this struct) */
    struct eth_addr nd_opt_mac;   /* Ethernet address in the case of SLL or TLL options */
};
BUILD_ASSERT_DECL(ND_OPT_LEN == sizeof(struct ovs_nd_opt));

/* Like struct nd_msg (from ndisc.h), but whereas that struct requires 32-bit
 * alignment, this one only requires 16-bit alignment. */
#define ND_MSG_LEN 24
struct ovs_nd_msg {
    struct icmp6_header icmph;
    ovs_16aligned_be32 rco_flags;
    union ovs_16aligned_in6_addr target;
    struct ovs_nd_opt options[0];
};
BUILD_ASSERT_DECL(ND_MSG_LEN == sizeof(struct ovs_nd_msg));

/*
 * Use the same struct for MLD and MLD2, naming members as the defined fields in
 * in the corresponding version of the protocol, though they are reserved in the
 * other one.
 */
#define MLD_HEADER_LEN 8
struct mld_header {
    uint8_t type;
    uint8_t code;
    ovs_be16 csum;
    ovs_be16 mrd;
    ovs_be16 ngrp;
};
BUILD_ASSERT_DECL(MLD_HEADER_LEN == sizeof(struct mld_header));

#define MLD2_RECORD_LEN 20
struct mld2_record {
    uint8_t type;
    uint8_t aux_len;
    ovs_be16 nsrcs;
    union ovs_16aligned_in6_addr maddr;
};
BUILD_ASSERT_DECL(MLD2_RECORD_LEN == sizeof(struct mld2_record));

#define MLD_QUERY 130
#define MLD_REPORT 131
#define MLD_DONE 132
#define MLD2_REPORT 143

/* The IPv6 flow label is in the lower 20 bits of the first 32-bit word. */
#define IPV6_LABEL_MASK 0x000fffff

/* Example:
 *
 * char *string = "1 ::1 2";
 * char ipv6_s[IPV6_SCAN_LEN + 1];
 * struct in6_addr ipv6;
 *
 * if (ovs_scan(string, "%d"IPV6_SCAN_FMT"%d", &a, ipv6_s, &b)
 *     && inet_pton(AF_INET6, ipv6_s, &ipv6) == 1) {
 *     ...
 * }
 */
#define IPV6_SCAN_FMT "%46[0123456789abcdefABCDEF:.]"
#define IPV6_SCAN_LEN 46

extern const struct in6_addr in6addr_exact;
#define IN6ADDR_EXACT_INIT { { { 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
                                 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff } } }

extern const struct in6_addr in6addr_all_hosts;
#define IN6ADDR_ALL_HOSTS_INIT { { { 0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00, \
                                     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01 } } }

static inline bool ipv6_addr_equals(const struct in6_addr *a,
                                    const struct in6_addr *b)
{
#ifdef IN6_ARE_ADDR_EQUAL
    return IN6_ARE_ADDR_EQUAL(a, b);
#else
    return !memcmp(a, b, sizeof(*a));
#endif
}

static inline bool ipv6_mask_is_any(const struct in6_addr *mask) {
    return ipv6_addr_equals(mask, &in6addr_any);
}

static inline bool ipv6_mask_is_exact(const struct in6_addr *mask) {
    return ipv6_addr_equals(mask, &in6addr_exact);
}

static inline bool ipv6_is_all_hosts(const struct in6_addr *addr) {
    return ipv6_addr_equals(addr, &in6addr_all_hosts);
}

static inline bool ipv6_addr_is_set(const struct in6_addr *addr) {
    return !ipv6_addr_equals(addr, &in6addr_any);
}

static inline bool ipv6_addr_is_multicast(const struct in6_addr *ip) {
    return ip->s6_addr[0] == 0xff;
}

static inline void
in6_addr_set_mapped_ipv4(struct in6_addr *addr, ovs_be32 ip4)
{
    union ovs_16aligned_in6_addr *taddr = (void *) addr;
    memset(taddr->be16, 0, sizeof(taddr->be16));
    taddr->be16[5] = OVS_BE16_MAX;
    put_16aligned_be32(&taddr->be32[3], ip4);
}

static inline ovs_be32
in6_addr_get_mapped_ipv4(const struct in6_addr *addr)
{
    union ovs_16aligned_in6_addr *taddr = (void *) addr;
    if (IN6_IS_ADDR_V4MAPPED(addr)) {
        return get_16aligned_be32(&taddr->be32[3]);
    } else {
        return INADDR_ANY;
    }
}

static inline bool dl_type_is_ip_any(ovs_be16 dl_type)
{
    return dl_type == htons(ETH_TYPE_IP)
        || dl_type == htons(ETH_TYPE_IPV6);
}

/* Tunnel header */

/* GRE protocol header */
struct gre_base_hdr {
    ovs_be16 flags;
    ovs_be16 protocol;
};

#define GRE_CSUM        0x8000
#define GRE_ROUTING     0x4000
#define GRE_KEY         0x2000
#define GRE_SEQ         0x1000
#define GRE_STRICT      0x0800
#define GRE_REC         0x0700
#define GRE_FLAGS       0x00F8
#define GRE_VERSION     0x0007

/* VXLAN protocol header */
struct vxlanhdr {
    ovs_16aligned_be32 vx_flags;
    ovs_16aligned_be32 vx_vni;
};

#define VXLAN_FLAGS 0x08000000  /* struct vxlanhdr.vx_flags required value. */

void ipv6_format_addr(const struct in6_addr *addr, struct ds *);
void ipv6_format_addr_bracket(const struct in6_addr *addr, struct ds *,
                              bool bracket);
void ipv6_format_mapped(const struct in6_addr *addr, struct ds *);
void ipv6_format_masked(const struct in6_addr *addr,
                        const struct in6_addr *mask, struct ds *);
const char * ipv6_string_mapped(char *addr_str, const struct in6_addr *addr);
struct in6_addr ipv6_addr_bitand(const struct in6_addr *src,
                                 const struct in6_addr *mask);
struct in6_addr ipv6_create_mask(int mask);
int ipv6_count_cidr_bits(const struct in6_addr *netmask);
bool ipv6_is_cidr(const struct in6_addr *netmask);
char *ipv6_parse_masked(const char *s, struct in6_addr *ipv6,
                        struct in6_addr *mask);

void *eth_compose(struct dp_packet *, const struct eth_addr eth_dst,
                  const struct eth_addr eth_src, uint16_t eth_type,
                  size_t size);
void *snap_compose(struct dp_packet *, const struct eth_addr eth_dst,
                   const struct eth_addr eth_src,
                   unsigned int oui, uint16_t snap_type, size_t size);
void packet_set_ipv4(struct dp_packet *, ovs_be32 src, ovs_be32 dst, uint8_t tos,
                     uint8_t ttl);
void packet_set_ipv6(struct dp_packet *, uint8_t proto, const ovs_be32 src[4],
                     const ovs_be32 dst[4], uint8_t tc,
                     ovs_be32 fl, uint8_t hlmit);
void packet_set_tcp_port(struct dp_packet *, ovs_be16 src, ovs_be16 dst);
void packet_set_udp_port(struct dp_packet *, ovs_be16 src, ovs_be16 dst);
void packet_set_sctp_port(struct dp_packet *, ovs_be16 src, ovs_be16 dst);
void packet_set_icmp(struct dp_packet *, uint8_t type, uint8_t code);
void packet_set_nd(struct dp_packet *, const ovs_be32 target[4],
                   const struct eth_addr sll, const struct eth_addr tll);

void packet_format_tcp_flags(struct ds *, uint16_t);
const char *packet_tcp_flag_to_string(uint32_t flag);
void compose_arp(struct dp_packet *, uint16_t arp_op,
                 const struct eth_addr arp_sha,
                 const struct eth_addr arp_tha, bool broadcast,
                 ovs_be32 arp_spa, ovs_be32 arp_tpa);
uint32_t packet_csum_pseudoheader(const struct ip_header *);

#endif /* packets.h */
