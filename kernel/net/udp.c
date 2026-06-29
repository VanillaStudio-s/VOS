#include "kernel.h"
#include "net/net.h"
#include "net/ip.h"
#include "net/udp.h"
#include "net/dhcp.h"
#include "net/dns.h"

/* ── Send UDP ────────────────────────────────────────────── */
int udp_send(IpAddr src, IpAddr dst,
             uint16_t sport, uint16_t dport,
             const void *payload, uint16_t plen) {
    static uint8_t _buf[1024];
    uint16_t tot = (uint16_t)(sizeof(UdpHdr) + plen);
    if (tot > (uint16_t)sizeof(_buf)) return -1;

    UdpHdr *uh = (UdpHdr *)_buf;
    uh->sport = htons(sport);
    uh->dport = htons(dport);
    uh->len   = htons(tot);
    uh->csum  = 0;   /* checksum optional for UDP */

    uint8_t *body = _buf + sizeof(UdpHdr);
    for (uint16_t i = 0; i < plen; i++) body[i] = ((const uint8_t *)payload)[i];

    return ip_send(src, dst, IP_PROTO_UDP, _buf, tot);
}

/* ── Receive UDP ─────────────────────────────────────────── */
void udp_receive(IpAddr src, IpAddr dst, const uint8_t *data, uint16_t len) {
    if (len < (uint16_t)sizeof(UdpHdr)) return;
    const UdpHdr *uh  = (const UdpHdr *)data;
    uint16_t dport    = htons(uh->dport);
    uint16_t sport    = htons(uh->sport);
    const uint8_t *pay = data + sizeof(UdpHdr);
    uint16_t plen = (uint16_t)(len - sizeof(UdpHdr));

    (void)dst;

    if (dport == 68 && sport == 67)           /* DHCP reply */
        dhcp_receive(pay, plen);
    else if (sport == 53)                      /* DNS response */
        dns_receive(pay, plen);
}
