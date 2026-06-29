#ifndef NET_TCP_H
#define NET_TCP_H
#include "kernel.h"
#include "net/net.h"

/* TCP flags */
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10

/* Connection states */
#define TCP_CLOSED      0
#define TCP_SYN_SENT    1
#define TCP_ESTABLISHED 2
#define TCP_CLOSE_WAIT  3
#define TCP_LAST_ACK    4
#define TCP_TIME_WAIT   5

/* Single TCP connection context */
typedef struct {
    IpAddr   remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq;   /* our next send seq */
    uint32_t ack;   /* next expected from remote */
    int      state;
} TcpConn;

/* Open a TCP connection (blocking, 3-way handshake).
   Returns 1 on success, 0 on failure. */
int  tcp_connect(TcpConn *c, IpAddr dst, uint16_t dport);

/* Send data over an ESTABLISHED connection */
int  tcp_send(TcpConn *c, const uint8_t *data, uint16_t len);

/* Close connection (sends FIN) */
void tcp_close(TcpConn *c);

/* Called by ip.c for incoming TCP segments */
void tcp_receive(IpAddr src, const uint8_t *data, uint16_t len);

/* Active connection pointer (single-connection model) */
extern TcpConn *tcp_active;

#endif
