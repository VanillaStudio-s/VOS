#ifndef NET_DNS_H
#define NET_DNS_H
#include "kernel.h"
#include "net/net.h"

/* Resolve a hostname to an IPv4 address using the configured DNS server.
   Blocks up to ~3 s.  Returns 1 on success, 0 on failure/timeout.
   Also accepts dotted-decimal strings ("1.2.3.4") directly. */
int  dns_resolve(const char *hostname, IpAddr *out);

/* Called by udp.c for incoming DNS response packets (sport=53) */
void dns_receive(const uint8_t *data, uint16_t len);

/* Parse dotted-decimal string into IpAddr.  Returns 1 on success. */
int  dns_parse_ip(const char *s, IpAddr *out);

#endif
