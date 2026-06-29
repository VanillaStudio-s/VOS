#include "kernel.h"
#include "cpu/timer.h"
#include "net/net.h"
#include "net/ip.h"
#include "net/tcp.h"
#include "net/net_init.h"   /* net_poll_once() */

TcpConn *tcp_active = (void*)0;

/* ── Build + send a TCP segment ─────────────────────────── */
static int _tcp_send_seg(TcpConn *c, uint8_t flags,
                         const uint8_t *data, uint16_t dlen) {
    static uint8_t _buf[1024];
    uint16_t tot = (uint16_t)(sizeof(TcpHdr) + dlen);
    if (tot > (uint16_t)sizeof(_buf)) return -1;

    TcpHdr *th = (TcpHdr *)_buf;
    th->sport  = htons(c->local_port);
    th->dport  = htons(c->remote_port);
    th->seq    = htonl(c->seq);
    th->ack    = (flags & TCP_ACK) ? htonl(c->ack) : 0;
    th->doff   = 0x50;        /* 5 words = 20 bytes */
    th->flags  = flags;
    th->window = htons(4096);
    th->csum   = 0;
    th->urg    = 0;

    for (uint16_t i = 0; i < dlen; i++)
        _buf[sizeof(TcpHdr) + i] = data[i];

    /* TCP checksum (pseudo-header) */
    uint32_t s = 0;
    s += (uint32_t)(net_ip.b[0] << 8) | net_ip.b[1];
    s += (uint32_t)(net_ip.b[2] << 8) | net_ip.b[3];
    s += (uint32_t)(c->remote_ip.b[0] << 8) | c->remote_ip.b[1];
    s += (uint32_t)(c->remote_ip.b[2] << 8) | c->remote_ip.b[3];
    s += IP_PROTO_TCP;
    s += tot;
    const uint16_t *p = (const uint16_t *)_buf;
    for (int i = tot >> 1; i > 0; i--) s += *p++;
    if (tot & 1) s += *((const uint8_t *)p);
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    th->csum = (uint16_t)~s;

    return ip_send(net_ip, c->remote_ip, IP_PROTO_TCP, _buf, tot);
}

/* ── Connect (blocking 3-way handshake) ─────────────────── */
int tcp_connect(TcpConn *c, IpAddr dst, uint16_t dport) {
    c->remote_ip   = dst;
    c->remote_port = dport;
    c->local_port  = 49152;  /* ephemeral */
    c->seq         = 0x12345678UL;
    c->ack         = 0;
    c->state       = TCP_SYN_SENT;
    tcp_active     = c;

    /* Send SYN */
    _tcp_send_seg(c, TCP_SYN, (void*)0, 0);
    c->seq++;

    /* Wait for SYN+ACK */
    uint32_t t0 = timer_get_ticks();
    while (c->state != TCP_ESTABLISHED && (timer_get_ticks() - t0) < 300)
        net_poll_once();   /* polls whichever NIC is active */

    if (c->state != TCP_ESTABLISHED) { tcp_active = (void*)0; return 0; }
    return 1;
}

/* ── Send data ───────────────────────────────────────────── */
int tcp_send(TcpConn *c, const uint8_t *data, uint16_t len) {
    if (c->state != TCP_ESTABLISHED) return -1;
    int r = _tcp_send_seg(c, TCP_PSH | TCP_ACK, data, len);
    c->seq += len;
    return r;
}

/* ── Close ───────────────────────────────────────────────── */
void tcp_close(TcpConn *c) {
    if (c->state == TCP_ESTABLISHED) {
        _tcp_send_seg(c, TCP_FIN | TCP_ACK, (void*)0, 0);
        c->seq++;
        c->state = TCP_CLOSE_WAIT;
    }
    tcp_active = (void*)0;
}

/* ── Receive ─────────────────────────────────────────────── */
void tcp_receive(IpAddr src, const uint8_t *data, uint16_t len) {
    if (len < (uint16_t)sizeof(TcpHdr)) return;
    if (!tcp_active) return;

    const TcpHdr *th = (const TcpHdr *)data;
    if (htons(th->dport) != tcp_active->local_port)  return;
    if (!ip_eq(src, tcp_active->remote_ip))           return;

    uint8_t flags = th->flags;

    if (tcp_active->state == TCP_SYN_SENT) {
        if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            tcp_active->ack = htonl(th->seq) + 1;
            /* Send ACK to complete handshake */
            _tcp_send_seg(tcp_active, TCP_ACK, (void*)0, 0);
            tcp_active->state = TCP_ESTABLISHED;
        }
    }
    else if (tcp_active->state == TCP_ESTABLISHED) {
        uint8_t doff = (uint8_t)((th->doff >> 4) * 4);
        if (doff < 20 || doff > len) return;
        uint16_t payload_len = (uint16_t)(len - doff);
        if (payload_len > 0)
            tcp_active->ack = htonl(th->seq) + payload_len;
        if (flags & TCP_FIN) {
            tcp_active->ack++;
            _tcp_send_seg(tcp_active, TCP_FIN | TCP_ACK, (void*)0, 0);
            tcp_active->state = TCP_TIME_WAIT;
        } else if (flags & TCP_ACK) {
            /* Acknowledge data */
            _tcp_send_seg(tcp_active, TCP_ACK, (void*)0, 0);
        }
    }
}