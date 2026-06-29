#ifndef IDT_H
#define IDT_H

#include "kernel.h"

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t zero;
} __attribute__((packed)) IDTEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IDTPointer;

void idt_init(void);
void idt_set_gate(int num, uint64_t base, uint16_t sel, uint8_t flags);

#endif
