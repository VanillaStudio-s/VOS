#include "kernel.h"
#include "cpu/timer.h"
#include "net/net.h"
#include "net/udp.h"
#include "net/dns.h"
#include "net/net_init.h"   /* net_poll_once() */

/* ── DNS state ───────────────────────────────────────────── */
static int    _dns_got  = 0;
static IpAddr _dns_ans;
static uint16_t _dns_id = 0xAB12;

/* ── Encode a DNS name into buffer, return bytes written ─── */
static int _encode_name(const char *host, uint8_t *out) {
    int pos = 0;
    int i   = 0;
    while (host[i]) {
        int start = i;
        while (host[i] && host[i] != '.') i++;
        int llen = i - start;
        out[pos++] = (uint8_t)llen;
        for (int k = start; k < i; k++) out[pos++] = (uint8_t)host[k];
        if (host[i] == '.') i++;
    }
    out[pos++] = 0;  /* root label */
    return pos;
}

/* ── Build DNS A-query, return total packet length ───────── */
static uint16_t _build_query(uint8_t *buf, const char *host) {
    /* Header */
    buf[0] = (uint8_t)(_dns_id >> 8); buf[1] = (uint8_t)_dns_id;
    buf[2] = 0x01; buf[3] = 0x00;   /* flags: RD=1 */
    buf[4] = 0x00; buf[5] = 0x01;   /* QDCOUNT = 1 */
    buf[6] = 0x00; buf[7] = 0x00;   /* ANCOUNT = 0 */
    buf[8] = 0x00; buf[9] = 0x00;   /* NSCOUNT = 0 */
    buf[10]= 0x00; buf[11]= 0x00;   /* ARCOUNT = 0 */

    int pos = 12;
    pos += _encode_name(host, buf + pos);

    buf[pos++] = 0x00; buf[pos++] = 0x01;  /* QTYPE  = A */
    buf[pos++] = 0x00; buf[pos++] = 0x01;  /* QCLASS = IN */
    return (uint16_t)pos;
}

/* ── Receive handler ─────────────────────────────────────── */
void dns_receive(const uint8_t *data, uint16_t len) {
    if (len < 12) return;
    uint16_t id = (uint16_t)((data[0] << 8) | data[1]);
    if (id != _dns_id) return;
    uint16_t flags  = (uint16_t)((data[2] << 8) | data[3]);
    if (!(flags & 0x8000)) return;         /* not a response */
    if ((flags & 0x000F) != 0) return;     /* error code set */

    uint16_t ancount = (uint16_t)((data[6] << 8) | data[7]);
    if (!ancount) return;

    /* Skip questions section — scan past QDCOUNT question entries */
    uint16_t qdcount = (uint16_t)((data[4] << 8) | data[5]);
    int pos = 12;
    for (int q = 0; q < qdcount && pos < len; q++) {
        /* skip name */
        while (pos < len) {
            uint8_t c = data[pos];
            if (c == 0)   { pos++; break; }
            if ((c & 0xC0) == 0xC0) { pos += 2; break; }
            pos += c + 1;
        }
        pos += 4;  /* QTYPE + QCLASS */
    }

    /* Parse answer RRs */
    for (int a = 0; a < ancount && pos < len; a++) {
        /* skip name */
        while (pos < len) {
            uint8_t c = data[pos];
            if (c == 0)            { pos++; break; }
            if ((c & 0xC0) == 0xC0){ pos += 2; break; }
            pos += c + 1;
        }
        if (pos + 10 > len) break;
        uint16_t type     = (uint16_t)((data[pos]   << 8) | data[pos+1]);
        /* class at pos+2, ttl at pos+4, rdlength at pos+8 */
        uint16_t rdlength = (uint16_t)((data[pos+8] << 8) | data[pos+9]);
        pos += 10;
        if (type == 1 && rdlength == 4 && pos + 4 <= len) {
            _dns_ans.b[0] = data[pos];
            _dns_ans.b[1] = data[pos+1];
            _dns_ans.b[2] = data[pos+2];
            _dns_ans.b[3] = data[pos+3];
            _dns_got = 1;
            return;
        }
        pos += rdlength;
    }
}

/* ── Parse dotted-decimal ────────────────────────────────── */
int dns_parse_ip(const char *s, IpAddr *out) {
    uint32_t b[4];
    int octet = 0, val = 0, has = 0;
    for (int i = 0; ; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') { val = val * 10 + (c - '0'); has = 1; }
        else if ((c == '.' || c == '\0') && has) {
            if (octet > 3 || val > 255) return 0;
            b[octet++] = (uint32_t)val;
            val = 0; has = 0;
            if (c == '\0') break;
        } else return 0;
    }
    if (octet != 4) return 0;
    out->b[0]=(uint8_t)b[0]; out->b[1]=(uint8_t)b[1];
    out->b[2]=(uint8_t)b[2]; out->b[3]=(uint8_t)b[3];
    return 1;
}

/* ── Resolve ─────────────────────────────────────────────── */
int dns_resolve(const char *hostname, IpAddr *out) {
    /* Try direct IP first */
    if (dns_parse_ip(hostname, out)) return 1;

    if (!net_has_ip) return 0;
    if (ip_eq(net_dns_srv, IP_ZERO)) return 0;

    uint8_t buf[64];
    uint16_t qlen = _build_query(buf, hostname);

    _dns_got = 0;
    udp_send(net_ip, net_dns_srv, 1025, 53, buf, qlen);

    /* Poll up to ~3 s */
    uint32_t t0 = timer_get_ticks();
    while (!_dns_got && (timer_get_ticks() - t0) < 300)
        net_poll_once();   /* polls whichever NIC is active */

    if (_dns_got) { *out = _dns_ans; _dns_id++; return 1; }
    return 0;
}