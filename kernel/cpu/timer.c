#include "kernel.h"
#include "cpu/irq.h"
#include "cpu/timer.h"

static volatile uint32_t ticks = 0;

static void timer_handler(Registers* r) { (void)r; ticks++; }

void timer_init(unsigned int hz) {
    irq_register(0, timer_handler);
    unsigned int div = 1193180 / hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

uint32_t timer_get_ticks(void) { return ticks; }

void timer_sleep(uint32_t n) {
    uint32_t s = ticks;
    while ((ticks - s) < n) __asm__ volatile("hlt");
}
