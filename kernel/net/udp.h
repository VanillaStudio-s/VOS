#ifndef NET_UDP_H
#define NET_UDP_H
#include "kernel.h"
#include "net/net.h"

/* Send a UDP datagram */
int  udp_send(IpAddr src, IpAddr dst,
              uint16_t sport, uint16_t dport,
              const void *payload, uint16_t len);

/* Called by ip.c when a UDP datagram arrives */
void udp_receive(IpAddr src, IpAddr dst, const uint8_t *data, uint16_t len);

#endif
