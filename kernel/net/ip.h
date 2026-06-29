#ifndef NET_IP_H
#define NET_IP_H
#include "kernel.h"
#include "net/net.h"

/* Send an IPv4 packet; src = net_ip (or IP_ZERO for DHCP bootstrap) */
int  ip_send(IpAddr src, IpAddr dst, uint8_t proto,
             const void *payload, uint16_t len);

/* Dispatch a received IPv4 payload (already stripped of Ethernet header) */
void ip_receive(const uint8_t *data, uint16_t len);

/* Send ICMP echo request (ping) */
int  icmp_ping(IpAddr dst, uint16_t id, uint16_t seq);

/* Set by ip.c when an ICMP echo reply arrives */
extern int      icmp_echo_got;
extern uint16_t icmp_echo_seq;
extern IpAddr   icmp_echo_src;

#endif
