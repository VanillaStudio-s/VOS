#ifndef NET_PCNET_H
#define NET_PCNET_H
#include "kernel.h"

extern uint16_t pcnet_io;

int  pcnet_init(void);
int  pcnet_send(const uint8_t *data, uint16_t len);
void pcnet_poll(void);

#endif
