#ifndef IRQ_H
#define IRQ_H

#include "kernel.h"

typedef void (*IRQHandler)(Registers*);

void irq_init(void);
void irq_register(int irq, IRQHandler handler);

#endif
