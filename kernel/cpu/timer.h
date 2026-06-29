#ifndef TIMER_H
#define TIMER_H
#include "kernel.h"
void timer_init(unsigned int hertz);
uint32_t timer_get_ticks(void);
void timer_sleep(uint32_t ticks);
#endif
