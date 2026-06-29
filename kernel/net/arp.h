#ifndef NET_ARP_H
#define NET_ARP_H
#include "kernel.h"
#include "net/net.h"

/* Send ARP request: "who has <ip>?" */
void arp_request(IpAddr ip);

/* Handle incoming ARP packet (already stripped of Ethernet header) */
void arp_receive(const uint8_t *data, uint16_t len);

/* Resolve IP -> MAC.  Sends ARP request and polls up to ~500 ms.
   Returns 1 on success, 0 on timeout. */
int  arp_resolve(IpAddr ip, MacAddr *out);

/* Update/add an ARP cache entry (also called internally on ARP reply) */
void arp_cache_set(IpAddr ip, MacAddr mac);

/* Lookup cache only (no request).  Returns 1 if found. */
int  arp_cache_get(IpAddr ip, MacAddr *out);

#endif
