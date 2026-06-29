#include "kernel.h"
#include "net/net.h"
#include "net/eth.h"
#include "net/arp.h"
#include "net/ip.h"
#include "net/rtl8139.h"
#include "net/pcnet.h"
#include "net/dhcp.h"
#include "net/net_init.h"

/* ── Global network state ────────────────────────────────── */
MacAddr net_mac;
IpAddr  net_ip      = {{0,0,0,0}};
IpAddr  net_gw      = {{0,0,0,0}};
IpAddr  net_mask    = {{255,255,255,0}};
IpAddr  net_dns_srv = {{8,8,8,8}};
int     net_up      = 0;
int     net_has_ip  = 0;

/* 1 = RTL8139  |  2 = PCnet */
static int _driver = 0;

/* ── Ethernet send (routes to active driver) ─────────────── */
int eth_send(MacAddr dst, uint16_t etype, const void *payload, uint16_t len) {
    static uint8_t _frame[1514];
    uint16_t tot = (uint16_t)(sizeof(EthHdr) + len);
    if (tot > (uint16_t)sizeof(_frame)) return -1;

    EthHdr *eh = (EthHdr *)_frame;
    eh->dst   = dst;
    eh->src   = net_mac;
    eh->etype = htons(etype);

    const uint8_t *src = (const uint8_t *)payload;
    for (uint16_t i = 0; i < len; i++) _frame[sizeof(EthHdr) + i] = src[i];

    if (_driver == 1) return rtl8139_send(_frame, tot);
    if (_driver == 2) return pcnet_send(_frame, tot);
    return -1;
}

/* ── Ethernet receive (called from driver poll) ──────────── */
void eth_receive(uint8_t *data, uint16_t len) {
    if (len < (uint16_t)sizeof(EthHdr)) return;
    EthHdr *eh = (EthHdr *)data;
    uint16_t etype = htons(eh->etype);
    uint8_t  *pay  = data + sizeof(EthHdr);
    uint16_t  plen = (uint16_t)(len - sizeof(EthHdr));

    switch (etype) {
    case ETH_ARP:  arp_receive(pay, plen); break;
    case ETH_IPV4: ip_receive (pay, plen); break;
    }
}

/* ── Init: try RTL8139 first, PCnet-FAST III as fallback ─── */
void net_init(void) {
    if (rtl8139_init()) {
        _driver = 1;
        net_up  = 1;
    } else if (pcnet_init()) {
        _driver = 2;
        net_up  = 1;
    }
    /* Non-blocking: sends DISCOVER and returns immediately.
       DHCP state machine advances via dhcp_tick() in net_poll(). */
    if (net_up) dhcp_start();
}

/* ── Poll NIC only — for use in blocking waits (no DHCP tick) */
void net_poll_once(void) {
    if (!net_up) return;
    if (_driver == 1) rtl8139_poll();
    else if (_driver == 2) pcnet_poll();
}

/* ── Poll active driver + advance DHCP state machine ─────── */
void net_poll(void) {
    net_poll_once();
    if (net_up && !net_has_ip) dhcp_tick();
}