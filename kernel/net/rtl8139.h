#ifndef NET_RTL8139_H
#define NET_RTL8139_H
#include "kernel.h"

/* Locate RTL8139 via PCI, initialize hardware; returns 1 on success */
int  rtl8139_init(void);

/* Copy data into a TX slot and start DMA; returns 0 on success */
int  rtl8139_send(const uint8_t *data, uint16_t len);

/* Drain the RX ring buffer, dispatch received frames via eth_receive() */
void rtl8139_poll(void);

/* I/O base port (0 = not found) */
extern uint16_t rtl8139_io;

#endif
