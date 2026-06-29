#include "kernel.h"
#include "cpu/timer.h"
#include "net/net.h"
#include "net/udp.h"
#include "net/dhcp.h"
#include "net/net_init.h"   /* net_poll_once() */

/* ── DHCP constants ──────────────────────────────────────── */
#define DHCP_MAGIC   0x63825363UL
#define DHCP_DISC    1
#define DHCP_OFFER   2
#define DHCP_REQ     3
#define DHCP_ACK     5

/* RFC 1542 §2.1: BOOTP/DHCP payload must be at least 300 bytes.
   VirtualBox and many real servers silently drop shorter packets. */
#define DHCP_MIN_LEN 300

/* ── DHCP fixed header (236 bytes) ──────────────────────── */
typedef struct {
    uint8_t  op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
} __attribute__((packed)) DhcpFixed;

/* ── Async state machine ─────────────────────────────────── */
#define DS_IDLE  0
#define DS_DISC  1
#define DS_REQ   2
#define DS_DONE  3

static int      _state = DS_IDLE;
static uint32_t _t0    = 0;

/* 3 s at 100 Hz — was 2 s, increased to survive slower DHCP servers */
#define TIMEOUT_TICKS 300

/* ── Shared transaction state ────────────────────────────── */
static uint32_t _xid       = 0xDEAD1234UL;
static int      _got_offer = 0;
static int      _got_ack   = 0;
static IpAddr   _offered_ip;
static IpAddr   _server_ip;

/* ── Build DHCP packet, always padded to DHCP_MIN_LEN bytes ─ */
/*    buf must be at least DHCP_MIN_LEN bytes.                  */
static uint16_t _build(uint8_t *buf, uint8_t type,
                        IpAddr req_ip, IpAddr srv_ip) {
    /* zero entire buffer so pad bytes = option-0 (RFC-valid pad) */
    for (int i = 0; i < DHCP_MIN_LEN; i++) buf[i] = 0;

    DhcpFixed *f = (DhcpFixed *)buf;
    f->op    = 1;          /* BOOTREQUEST */
    f->htype = 1;          /* Ethernet */
    f->hlen  = 6;
    f->xid   = htonl(_xid);
    f->flags = htons(0x8000); /* broadcast flag → server must broadcast reply */
    for (int i = 0; i < 6; i++) f->chaddr[i] = net_mac.b[i];

    uint8_t *opt = buf + sizeof(DhcpFixed);  /* options start at byte 236 */
    /* magic cookie */
    opt[0] = (uint8_t)((DHCP_MAGIC >> 24) & 0xFF);
    opt[1] = (uint8_t)((DHCP_MAGIC >> 16) & 0xFF);
    opt[2] = (uint8_t)((DHCP_MAGIC >>  8) & 0xFF);
    opt[3] = (uint8_t)( DHCP_MAGIC        & 0xFF);
    int i = 4;

    /* option 53: DHCP message type */
    opt[i++] = 53; opt[i++] = 1; opt[i++] = type;

    if (type == DHCP_REQ) {
        /* option 50: requested IP */
        opt[i++] = 50; opt[i++] = 4;
        opt[i++] = req_ip.b[0]; opt[i++] = req_ip.b[1];
        opt[i++] = req_ip.b[2]; opt[i++] = req_ip.b[3];
        /* option 54: server identifier */
        opt[i++] = 54; opt[i++] = 4;
        opt[i++] = srv_ip.b[0]; opt[i++] = srv_ip.b[1];
        opt[i++] = srv_ip.b[2]; opt[i++] = srv_ip.b[3];
    }

    /* option 55: parameter request list */
    opt[i++] = 55; opt[i++] = 4;
    opt[i++] = 1;   /* subnet mask */
    opt[i++] = 3;   /* router / gateway */
    opt[i++] = 6;   /* DNS server */
    opt[i++] = 15;  /* domain name */

    /* option 255: end */
    opt[i++] = 255;

    /* The buffer was already zeroed, so bytes after END are valid
       pad (option code 0).  The total always equals DHCP_MIN_LEN
       because sizeof(DhcpFixed)=236 and options array=64 bytes.
       Return DHCP_MIN_LEN so UDP sends the full 300-byte payload. */
    return (uint16_t)DHCP_MIN_LEN;
}

/* ── Parse a DHCP option from options blob ───────────────── */
static int _opt(const uint8_t *opt, uint16_t olen, uint8_t code,
                uint8_t *out, int maxlen) {
    for (int i = 0; i < olen; ) {
        uint8_t t = opt[i++];
        if (t == 255) break;
        if (t == 0)   continue;    /* pad byte */
        if (i >= olen) break;
        uint8_t l = opt[i++];
        if (i + l > olen) break;
        if (t == code) {
            int c = l < maxlen ? l : maxlen;
            for (int k = 0; k < c; k++) out[k] = opt[i + k];
            return c;
        }
        i += l;
    }
    return 0;
}

/* ── Receive handler (called from udp.c on port 68) ─────── */
void dhcp_receive(const uint8_t *data, uint16_t len) {
    if (len < (uint16_t)(sizeof(DhcpFixed) + 4)) return;
    const DhcpFixed *f = (const DhcpFixed *)data;

    /* XID must match our outstanding transaction */
    if (htonl(f->xid) != _xid) return;

    const uint8_t *opt  = data + sizeof(DhcpFixed);
    uint16_t       olen = (uint16_t)(len - sizeof(DhcpFixed));
    if (olen < 4) return;

    /* verify magic cookie */
    uint32_t magic = ((uint32_t)opt[0] << 24) | ((uint32_t)opt[1] << 16) |
                     ((uint32_t)opt[2] <<  8) |  (uint32_t)opt[3];
    if (magic != DHCP_MAGIC) return;
    opt += 4; olen = (uint16_t)(olen - 4);

    uint8_t msg_type = 0;
    _opt(opt, olen, 53, &msg_type, 1);

    if (msg_type == DHCP_OFFER && !_got_offer) {
        /* yiaddr: network byte order → host byte order */
        uint32_t y = htonl(f->yiaddr);
        _offered_ip.b[0] = (uint8_t)(y >> 24);
        _offered_ip.b[1] = (uint8_t)(y >> 16);
        _offered_ip.b[2] = (uint8_t)(y >>  8);
        _offered_ip.b[3] = (uint8_t)( y      );
        _opt(opt, olen, 54, _server_ip.b, 4);
        _got_offer = 1;
    }
    else if (msg_type == DHCP_ACK && _got_offer) {
        uint32_t y = htonl(f->yiaddr);
        net_ip.b[0] = (uint8_t)(y >> 24);
        net_ip.b[1] = (uint8_t)(y >> 16);
        net_ip.b[2] = (uint8_t)(y >>  8);
        net_ip.b[3] = (uint8_t)( y      );
        _opt(opt, olen,  1, net_mask.b,    4);
        _opt(opt, olen,  3, net_gw.b,      4);
        _opt(opt, olen,  6, net_dns_srv.b, 4);
        net_has_ip = 1;
        _got_ack   = 1;
        _state     = DS_DONE;
    }
}

/* ── Non-blocking: send DISCOVER, return immediately ─────── */
void dhcp_start(void) {
    _got_offer = 0;
    _got_ack   = 0;
    _state     = DS_DISC;
    _t0        = timer_get_ticks();

    uint8_t  buf[DHCP_MIN_LEN];
    uint16_t len = _build(buf, DHCP_DISC, IP_ZERO, IP_ZERO);
    udp_send(IP_ZERO, IP_BCAST, 68, 67, buf, len);
}

/* ── State-machine tick: called from net_poll() every loop ── */
void dhcp_tick(void) {
    uint32_t now = timer_get_ticks();

    switch (_state) {
    case DS_DISC:
        if (_got_offer) {
            uint8_t  buf[DHCP_MIN_LEN];
            uint16_t len = _build(buf, DHCP_REQ, _offered_ip, _server_ip);
            udp_send(IP_ZERO, IP_BCAST, 68, 67, buf, len);
            _t0    = now;
            _state = DS_REQ;
        } else if (now - _t0 >= TIMEOUT_TICKS) {
            dhcp_start();   /* retry DISCOVER */
        }
        break;

    case DS_REQ:
        if (_got_ack) {
            _state = DS_DONE;
        } else if (now - _t0 >= TIMEOUT_TICKS) {
            _state = DS_IDLE;
            dhcp_start();   /* start over */
        }
        break;

    default:
        break;
    }
}

/* ── Blocking DISCOVER: used by Settings "Reconnect" button ─ */
int dhcp_discover(void) {
    _got_offer = 0;
    _got_ack   = 0;
    _state     = DS_DISC;

    uint8_t buf[DHCP_MIN_LEN];
    IpAddr  src = IP_ZERO, dst = IP_BCAST;

    /* send DISCOVER — return 0 immediately if NIC can't transmit */
    uint16_t len = _build(buf, DHCP_DISC, src, src);
    if (udp_send(src, dst, 68, 67, buf, len) < 0) {
        _state = DS_IDLE;
        return -1;   /* caller can show "send failed" vs "no response" */
    }

    /* wait for OFFER */
    uint32_t t0 = timer_get_ticks();
    while (!_got_offer && (timer_get_ticks() - t0) < TIMEOUT_TICKS)
        net_poll_once();
    if (!_got_offer) { _state = DS_IDLE; return 0; }

    /* send REQUEST */
    len = _build(buf, DHCP_REQ, _offered_ip, _server_ip);
    udp_send(src, dst, 68, 67, buf, len);

    /* wait for ACK */
    t0 = timer_get_ticks();
    while (!_got_ack && (timer_get_ticks() - t0) < TIMEOUT_TICKS)
        net_poll_once();

    return _got_ack;
}