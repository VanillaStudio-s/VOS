#ifndef NET_INIT_H
#define NET_INIT_H
#include "kernel.h"

/* Initialize NIC and run DHCP.  Called once from kernel_main. */
void net_init(void);

/* Poll for incoming packets; call every main-loop iteration. */
void net_poll(void);

/* Poll NIC only (no DHCP tick). Use inside blocking wait loops
   so any active driver (RTL8139 or PCnet) receives packets. */
void net_poll_once(void);

#endif