#include "kernel.h"
#include "cpu/irq.h"

static IRQHandler handlers[16] = {0};

void irq_register(int irq, IRQHandler h) {
    if (irq >= 0 && irq < 16) handlers[irq] = h;
}

void irq_handler(Registers* r) {
    int irq = (int)(r->int_no - 32);
    if (irq >= 0 && irq < 16 && handlers[irq])
        handlers[irq](r);
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void irq_init(void) {
    for (int i = 0; i < 16; i++) handlers[i] = 0;
}
