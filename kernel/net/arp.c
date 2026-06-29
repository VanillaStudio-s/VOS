#include "kernel.h"
#include "cpu/timer.h"
#include "net/net.h"
#include "net/eth.h"
#include "net/arp.h"
#include "net/net_init.h"   /* net_poll_once() */

/* ── ARP cache ───────────────────────────────────────────── */
#define ARP_CACHE_SZ 16

typedef struct {
    IpAddr  ip;
    MacAddr mac;
    int     valid;
} ArpEntry;

static ArpEntry _cache[ARP_CACHE_SZ];

void arp_cache_set(IpAddr ip, MacAddr mac) {
    /* update existing entry */
    for (int i = 0; i < ARP_CACHE_SZ; i++) {
        if (_cache[i].valid && ip_eq(_cache[i].ip, ip)) {
            _cache[i].mac = mac;
            return;
        }
    }
    /* find free slot */
    for (int i = 0; i < ARP_CACHE_SZ; i++) {
        if (!_cache[i].valid) {
            _cache[i].ip    = ip;
            _cache[i].mac   = mac;
            _cache[i].valid = 1;
            return;
        }
    }
    /* evict slot 0 (simple strategy) */
    _cache[0].ip    = ip;
    _cache[0].mac   = mac;
    _cache[0].valid = 1;
}

int arp_cache_get(IpAddr ip, MacAddr *out) {
    for (int i = 0; i < ARP_CACHE_SZ; i++) {
        if (_cache[i].valid && ip_eq(_cache[i].ip, ip)) {
            *out = _cache[i].mac;
            return 1;
        }
    }
    return 0;
}

/* ── Send ARP request ────────────────────────────────────── */
void arp_request(IpAddr target_ip) {
    ArpPkt pkt;
    pkt.hwtype = htons(1);
    pkt.proto  = htons(ETH_IPV4);
    pkt.hwlen  = 6;
    pkt.plen   = 4;
    pkt.op     = htons(1);           /* request */
    pkt.smac   = net_mac;
    pkt.sip    = net_ip;
    pkt.tmac   = MAC_BCAST;
    pkt.tip    = target_ip;
    eth_send(MAC_BCAST, ETH_ARP, &pkt, sizeof(pkt));
}

/* ── Handle received ARP ─────────────────────────────────── */
void arp_receive(const uint8_t *data, uint16_t len) {
    if (len < (uint16_t)sizeof(ArpPkt)) return;
    const ArpPkt *p = (const ArpPkt *)data;
    if (htons(p->hwtype) != 1)        return;
    if (htons(p->proto)  != ETH_IPV4) return;
    if (p->hwlen != 6 || p->plen != 4) return;

    uint16_t op = htons(p->op);

    /* Always add sender to cache */
    arp_cache_set(p->sip, p->smac);

    /* If it's a request for our IP, send a reply */
    if (op == 1 && ip_eq(p->tip, net_ip)) {
        ArpPkt reply;
        reply.hwtype = htons(1);
        reply.proto  = htons(ETH_IPV4);
        reply.hwlen  = 6;
        reply.plen   = 4;
        reply.op     = htons(2);     /* reply */
        reply.smac   = net_mac;
        reply.sip    = net_ip;
        reply.tmac   = p->smac;
        reply.tip    = p->sip;
        eth_send(p->smac, ETH_ARP, &reply, sizeof(reply));
    }
}

/* ── Blocking resolve ────────────────────────────────────── */
int arp_resolve(IpAddr ip, MacAddr *out) {
    /* broadcast / own IP shortcuts */
    if (ip_eq(ip, IP_BCAST)) { *out = MAC_BCAST; return 1; }
    if (ip_eq(ip, net_ip))   { *out = net_mac;   return 1; }

    /* hit cache first */
    if (arp_cache_get(ip, out)) return 1;

    /* send request and poll up to ~500 ms (50 * 10 ms) */
    arp_request(ip);
    uint32_t t0 = timer_get_ticks();
    while (timer_get_ticks() - t0 < 50) {
        net_poll_once();   /* polls whichever NIC is active */
        if (arp_cache_get(ip, out)) return 1;
    }
    return 0;
}