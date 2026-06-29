#include "kernel.h"
#include "mem/heap.h"

/* Block header placed immediately before each allocation */
typedef struct Block {
    uint64_t      size;   /* data bytes, not counting this header */
    uint8_t       free;
    uint8_t       _pad[7];
    struct Block* next;
} Block;

#define HDR  sizeof(Block)   /* 24 bytes */
#define MIN_SPLIT  16        /* don't split if leftover < this */

static uint8_t  _heap[HEAP_SIZE] __attribute__((aligned(16)));
static Block*   _head = (Block*)0;

void heap_init(void) {
    _head        = (Block*)_heap;
    _head->size  = HEAP_SIZE - HDR;
    _head->free  = 1;
    _head->next  = (Block*)0;
}

void* kmalloc(uint64_t size) {
    if (!size) return (void*)0;
    size = (size + 15) & ~15ULL;   /* 16-byte align */

    Block* b = _head;
    while (b) {
        if (b->free && b->size >= size) {
            /* Split block if remainder is large enough */
            if (b->size >= size + HDR + MIN_SPLIT) {
                Block* nb  = (Block*)((uint8_t*)b + HDR + size);
                nb->size   = b->size - size - HDR;
                nb->free   = 1;
                nb->next   = b->next;
                b->size    = size;
                b->next    = nb;
            }
            b->free = 0;
            return (uint8_t*)b + HDR;
        }
        b = b->next;
    }
    return (void*)0;   /* out of memory */
}

void kfree(void* ptr) {
    if (!ptr) return;
    Block* b = (Block*)((uint8_t*)ptr - HDR);
    b->free = 1;
    /* Coalesce contiguous free blocks forward */
    while (b->next && b->next->free) {
        b->size += HDR + b->next->size;
        b->next  = b->next->next;
    }
}

void* kcalloc(uint64_t count, uint64_t size) {
    uint64_t total = count * size;
    void* p = kmalloc(total);
    if (p) {
        uint8_t* bp = (uint8_t*)p;
        for (uint64_t i = 0; i < total; i++) bp[i] = 0;
    }
    return p;
}

void* krealloc(void* ptr, uint64_t new_size) {
    if (!ptr)      return kmalloc(new_size);
    if (!new_size) { kfree(ptr); return (void*)0; }

    Block* b = (Block*)((uint8_t*)ptr - HDR);
    if (b->size >= new_size) return ptr;

    void* np = kmalloc(new_size);
    if (!np) return (void*)0;

    uint8_t* src  = (uint8_t*)ptr;
    uint8_t* dst  = (uint8_t*)np;
    uint64_t copy = b->size < new_size ? b->size : new_size;
    for (uint64_t i = 0; i < copy; i++) dst[i] = src[i];

    kfree(ptr);
    return np;
}

uint64_t heap_used_bytes(void) {
    uint64_t used = 0;
    for (Block* b = _head; b; b = b->next)
        if (!b->free) used += b->size;
    return used;
}

uint64_t heap_free_bytes(void) {
    uint64_t free = 0;
    for (Block* b = _head; b; b = b->next)
        if (b->free) free += b->size;
    return free;
}
