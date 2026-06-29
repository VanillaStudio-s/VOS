#ifndef HEAP_H
#define HEAP_H

#include "kernel.h"

#define HEAP_SIZE  (4 * 1024 * 1024)   /* 4 MB */

void  heap_init(void);

void* kmalloc(uint64_t size);
void* kcalloc(uint64_t count, uint64_t size);
void* krealloc(void* ptr, uint64_t new_size);
void  kfree(void* ptr);

uint64_t heap_used_bytes(void);
uint64_t heap_free_bytes(void);

#endif
