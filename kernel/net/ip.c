#include "kernel.h"
#include "net/net.h"
#include "net/eth.h"
#include "net/arp.h"
#include "net/ip.h"
#include "net/udp.h"
#include "net/tcp.h"

/* ── ICMP echo reply state ────────────────────────────────── */
int      icmp_echo_got = 0;
uint16_t icmp_echo_seq = 0;
IpAddr   icmp_echo_src;

static uint16_t _ip_id = 1;

/* ── Routing: next-hop MAC ───────────────────────────────── */
static int _route(IpAddr dst, MacAddr *out) {
    if (ip_eq(dst, IP_BCAST)) { *out = MAC_BCAST; return 1; }
    /* same subnet? */
    int same = 1;
    for (int i = 0; i < 4; i++) {
        if ((dst.b[i] & net_mask.b[i]) != (net_ip.b[i] & net_mask.b[i]))
            same = 0;
    }
    return arp_resolve(same ? dst : net_gw, out);
}

/* ── Send IPv4 ───────────────────────────────────────────── */
int ip_send(IpAddr src, IpAddr dst, uint8_t proto,
            const void *payload, uint16_t plen) {
    static uint8_t _frame[1500];
    if ((uint16_t)(sizeof(IPv4Hdr) + plen) > (uint16_t)sizeof(_frame)) return -1;

    IPv4Hdr *ih = (IPv4Hdr *)_frame;
    ih->ver_ihl = 0x45;
    ih->dscp    = 0;
    ih->len     = htons((uint16_t)(sizeof(IPv4Hdr) + plen));
    ih->id      = htons(_ip_id++);
    ih->frag    = htons(0x4000);
    ih->ttl     = 64;
    ih->proto   = proto;
    ih->csum    = 0;
    ih->src     = src;
    ih->dst     = dst;
    ih->csum    = net_csum(ih, sizeof(IPv4Hdr));

    uint8_t *body = _frame + sizeof(IPv4Hdr);
    for (uint16_t i = 0; i < plen; i++) body[i] = ((const uint8_t *)payload)[i];

    MacAddr dst_mac;
    if (!_route(dst, &dst_mac)) return -1;

    return eth_send(dst_mac, ETH_IPV4, _frame,
                    (uint16_t)(sizeof(IPv4Hdr) + plen));
}

/* ── ICMP ────────────────────────────────────────────────── */
static void _icmp_recv(IpAddr src, const uint8_t *data, uint16_t len) {
    if (len < (uint16_t)sizeof(IcmpHdr)) return;
    const IcmpHdr *ih = (const IcmpHdr *)data;
    if (ih->type == 0) {
        /* Echo Reply */
        icmp_echo_got = 1;
        icmp_echo_seq = htons(ih->seq);
        icmp_echo_src = src;
    }
}

int icmp_ping(IpAddr dst, uint16_t id, uint16_t seq) {
    uint8_t buf[sizeof(IcmpHdr) + 32];
    IcmpHdr *ih = (IcmpHdr *)buf;
    ih->type = 8;
    ih->code = 0;
    ih->csum = 0;
    ih->id   = htons(id);
    ih->seq  = htons(seq);
    for (int i = 0; i < 32; i++) buf[sizeof(IcmpHdr) + i] = (uint8_t)i;
    ih->csum = net_csum(buf, (uint16_t)sizeof(buf));
    return ip_send(net_ip, dst, IP_PROTO_ICMP, buf, (uint16_t)sizeof(buf));
}

/* ── Dispatch incoming IPv4 ──────────────────────────────── */
void ip_receive(const uint8_t *data, uint16_t len) {
    if (len < (uint16_t)sizeof(IPv4Hdr)) return;
    const IPv4Hdr *ih = (const IPv4Hdr *)data;
    if ((ih->ver_ihl >> 4) != 4) return;
    uint8_t  ihl     = (uint8_t)((ih->ver_ihl & 0x0F) * 4);
    uint16_t tot_len = htons(ih->len);
    if (ihl < 20 || tot_len < ihl || tot_len > len) return;

    const uint8_t *pay = data + ihl;
    uint16_t       plen = (uint16_t)(tot_len - ihl);

    switch (ih->proto) {
    case IP_PROTO_ICMP: _icmp_recv(ih->src, pay, plen); break;
    case IP_PROTO_UDP:  udp_receive(ih->src, ih->dst, pay, plen); break;
    case IP_PROTO_TCP:  tcp_receive(ih->src, pay, plen); break;
    }
}
