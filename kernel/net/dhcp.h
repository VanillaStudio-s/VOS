#ifndef NET_DHCP_H
#define NET_DHCP_H
#include "kernel.h"
#include "net/net.h"

/* Non-blocking: send DISCOVER and return immediately.
   dhcp_tick() (called from net_poll) advances the state machine. */
void dhcp_start(void);

/* Advance DHCP state machine — call once per net_poll tick. */
void dhcp_tick(void);

/* Blocking version used by the "Reconnect" button (acceptable UX wait). */
int  dhcp_discover(void);

/* Called by udp.c when a packet arrives on port 68 */
void dhcp_receive(const uint8_t *data, uint16_t len);

#endif
