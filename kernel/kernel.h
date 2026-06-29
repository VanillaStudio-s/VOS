#ifndef KERNEL_H
#define KERNEL_H

/* ── Primitive Types ───────────────────────────────────────────────────────── */
#define NULL ((void*)0)
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* ── Multiboot2 ────────────────────────────────────────────────────────────── */
#define MB2_BOOTLOADER_MAGIC  0x36D76289

/* Multiboot2 fixed header at start of info struct */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) MB2Info;

/* Generic tag header */
typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) MB2Tag;

/* Tag type 8: framebuffer info */
typedef struct {
    MB2Tag   tag;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
} __attribute__((packed)) MB2Framebuffer;

/* ── I/O Port Helpers ──────────────────────────────────────────────────────── */
static inline unsigned char inb(unsigned short port) {
    unsigned char r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void outb(unsigned short port, unsigned char v) {
    __asm__ volatile("outb %0, %1" :: "a"(v), "Nd"(port));
}
static inline void outw(unsigned short port, unsigned short v) {
    __asm__ volatile("outw %0, %1" :: "a"(v), "Nd"(port));
}
static inline unsigned int inl(unsigned short port) {
    unsigned int r;
    __asm__ volatile("inl %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void outl(unsigned short port, unsigned int v) {
    __asm__ volatile("outl %0, %1" :: "a"(v), "Nd"(port));
}

/* ── Interrupt Register Frame (pushed by idt.asm) ─────────────────────────── */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) Registers;

/* ── Shared globals ─────────────────────────────────────────────────────────── */
extern int shift_pressed;
extern int current_layout;
extern int current_system_lang;
extern int is_locked;
extern int is_changing_pw;
extern int is_retype_pw;
extern uint64_t sys_phys_ram_kb;   /* physical RAM from Multiboot2 */

#endif
