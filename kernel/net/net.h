#ifndef NET_NET_H
#define NET_NET_H
#include "kernel.h"

/* ── Byte-swap helpers (network = big-endian, x86 = little-endian) ── */
static inline uint16_t htons(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
static inline uint32_t htonl(uint32_t x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
}
#define ntohs htons
#define ntohl htonl

/* ── Address types ──────────────────────────────────────────────── */
typedef struct { uint8_t b[6]; } __attribute__((packed)) MacAddr;
typedef struct { uint8_t b[4]; } __attribute__((packed)) IpAddr;

static inline int mac_eq(MacAddr a, MacAddr b) {
    return a.b[0]==b.b[0]&&a.b[1]==b.b[1]&&a.b[2]==b.b[2]&&
           a.b[3]==b.b[3]&&a.b[4]==b.b[4]&&a.b[5]==b.b[5];
}
static inline int ip_eq(IpAddr a, IpAddr b) {
    return a.b[0]==b.b[0]&&a.b[1]==b.b[1]&&a.b[2]==b.b[2]&&a.b[3]==b.b[3];
}

#define MAC_BCAST ((MacAddr){{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}})
#define IP_BCAST  ((IpAddr){{255,255,255,255}})
#define IP_ZERO   ((IpAddr){{0,0,0,0}})

/* ── EtherTypes ─────────────────────────────────────────────────── */
#define ETH_ARP   0x0806
#define ETH_IPV4  0x0800

/* ── IP protocol numbers ────────────────────────────────────────── */
#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP  17

/* ── Ethernet header (14 bytes) ─────────────────────────────────── */
typedef struct {
    MacAddr  dst;
    MacAddr  src;
    uint16_t etype;   /* big-endian */
} __attribute__((packed)) EthHdr;

/* ── IPv4 header (20 bytes, no options) ─────────────────────────── */
typedef struct {
    uint8_t  ver_ihl;     /* 0x45 */
    uint8_t  dscp;
    uint16_t len;         /* total length, big-endian */
    uint16_t id;
    uint16_t frag;        /* 0x4000 = DF */
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t csum;
    IpAddr   src;
    IpAddr   dst;
} __attribute__((packed)) IPv4Hdr;

/* ── UDP header (8 bytes) ───────────────────────────────────────── */
typedef struct {
    uint16_t sport, dport, len, csum;
} __attribute__((packed)) UdpHdr;

/* ── TCP header (20 bytes) ──────────────────────────────────────── */
typedef struct {
    uint16_t sport, dport;
    uint32_t seq, ack;
    uint8_t  doff;   /* header length in 32-bit words, shifted left 4 */
    uint8_t  flags;  /* FIN=1 SYN=2 RST=4 PSH=8 ACK=16 */
    uint16_t window, csum, urg;
} __attribute__((packed)) TcpHdr;

/* ── ICMP header ────────────────────────────────────────────────── */
typedef struct {
    uint8_t  type, code;
    uint16_t csum;
    uint16_t id, seq;
} __attribute__((packed)) IcmpHdr;

/* ── ARP packet (Ethernet / IPv4) ───────────────────────────────── */
typedef struct {
    uint16_t hwtype;    /* 1 = Ethernet */
    uint16_t proto;     /* 0x0800 = IPv4 */
    uint8_t  hwlen;     /* 6 */
    uint8_t  plen;      /* 4 */
    uint16_t op;        /* 1=request, 2=reply */
    MacAddr  smac; IpAddr sip;
    MacAddr  tmac; IpAddr tip;
} __attribute__((packed)) ArpPkt;

/* ── Internet checksum ──────────────────────────────────────────── */
static inline uint16_t net_csum(const void *data, uint32_t len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t s = 0;
    while (len > 1) { s += *p++; len -= 2; }
    if (len)         s += *(const uint8_t *)p;
    while (s >> 16)  s  = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}

/* ── Global network state ───────────────────────────────────────── */
extern MacAddr net_mac;       /* our MAC (from NIC) */
extern IpAddr  net_ip;        /* our IP (from DHCP) */
extern IpAddr  net_gw;        /* default gateway    */
extern IpAddr  net_mask;      /* subnet mask        */
extern IpAddr  net_dns_srv;   /* DNS server         */
extern int     net_up;        /* 1 = NIC found & init */
extern int     net_has_ip;    /* 1 = DHCP successful  */

#endif
