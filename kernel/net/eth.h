#ifndef NET_ETH_H
#define NET_ETH_H
#include "kernel.h"
#include "net/net.h"

/* Called by rtl8139 driver when a raw Ethernet frame arrives */
void eth_receive(uint8_t *data, uint16_t len);

/* Build Ethernet frame and hand to NIC; returns 0 on success */
int  eth_send(MacAddr dst, uint16_t etype, const void *payload, uint16_t len);

#endif
