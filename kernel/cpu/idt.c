#include "kernel.h"
#include "cpu/idt.h"
#include "cpu/irq.h"

extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

extern void idt_load(IDTPointer*);
extern void tss_load(uint16_t selector);

static IDTEntry   idt[256]  __attribute__((aligned(16)));
static IDTPointer idt_ptr   __attribute__((aligned(16)));

/* Double-fault dedicated stack (IST1) */
#define DF_STACK_SIZE 4096
static uint8_t df_stack[DF_STACK_SIZE] __attribute__((aligned(16)));

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} TSS64;

static TSS64 tss __attribute__((aligned(16)));

/* ── Minimal 64-bit GDT: null / code64 / data64 / TSS (2 slots) ──────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} GDTEntry;

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid1;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_mid2;
    uint32_t base_high;
    uint32_t reserved;
} TSSDescriptor;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} GDTPointer;

static struct __attribute__((packed)) {
    GDTEntry     null_desc;
    GDTEntry     code64;
    GDTEntry     data64;
    TSSDescriptor tss_desc;
} gdt;

static GDTPointer gdt_ptr;

static void gdt_init(void) {
    gdt.null_desc = (GDTEntry){0,0,0,0,0,0};

    gdt.code64.limit_low = 0xFFFF; gdt.code64.base_low = 0;
    gdt.code64.base_mid  = 0;      gdt.code64.access   = 0x9A;
    gdt.code64.gran      = 0xAF;   gdt.code64.base_high = 0;

    gdt.data64.limit_low = 0xFFFF; gdt.data64.base_low = 0;
    gdt.data64.base_mid  = 0;      gdt.data64.access   = 0x92;
    gdt.data64.gran      = 0xCF;   gdt.data64.base_high = 0;

    uint64_t tb = (uint64_t)&tss;
    uint16_t tl = sizeof(TSS64) - 1;
    gdt.tss_desc.limit_low  = tl & 0xFFFF;
    gdt.tss_desc.base_low   = (uint16_t)(tb & 0xFFFF);
    gdt.tss_desc.base_mid1  = (uint8_t)((tb >> 16) & 0xFF);
    gdt.tss_desc.access     = 0x89;
    gdt.tss_desc.gran       = (uint8_t)((tl >> 16) & 0xF);
    gdt.tss_desc.base_mid2  = (uint8_t)((tb >> 24) & 0xFF);
    gdt.tss_desc.base_high  = (uint32_t)(tb >> 32);
    gdt.tss_desc.reserved   = 0;

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    __asm__ volatile(
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        :: "m"(gdt_ptr) : "rax", "memory"
    );
}

/* ── exception_handler: BSOD to VGA text + VESA FB ─────────────────────────── */
static void bsod_puthex(volatile uint16_t* v, int* col, uint64_t n, uint16_t a) {
    const char h[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4)
        v[(*col)++] = (uint16_t)(a | h[(n >> i) & 0xF]);
}
static void bsod_puts(volatile uint16_t* v, int* col, const char* s, uint16_t a) {
    while (*s) v[(*col)++] = (uint16_t)(a | (unsigned char)*s++);
}

void exception_handler(Registers* r) {
    __asm__ volatile("cli");

    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    uint16_t RED = 0x4F00;
    for (int i = 0; i < 240; i++) vga[i] = RED | ' ';

    int col = 0;
    bsod_puts(vga, &col, " *** EXCEPTION #", RED);
    bsod_puthex(vga, &col, r->int_no,   RED);
    bsod_puts(vga, &col, " ERR=", RED);
    bsod_puthex(vga, &col, r->err_code, RED);
    bsod_puts(vga, &col, " ***", RED);

    col = 80;
    bsod_puts(vga, &col, " RIP=", RED); bsod_puthex(vga, &col, r->rip, RED);
    bsod_puts(vga, &col, " RSP=", RED); bsod_puthex(vga, &col, r->rsp, RED);

    col = 160;
    bsod_puts(vga, &col, " RAX=", RED); bsod_puthex(vga, &col, r->rax, RED);
    bsod_puts(vga, &col, " RBX=", RED); bsod_puthex(vga, &col, r->rbx, RED);

    __asm__ volatile("cli; hlt");
    while (1) {}
}

static void idt_set_ist(int n, uint64_t base, uint16_t sel, uint8_t fl, uint8_t ist) {
    idt[n].base_low  = (uint16_t)(base & 0xFFFF);
    idt[n].selector  = sel;
    idt[n].ist       = ist;
    idt[n].flags     = fl;
    idt[n].base_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[n].base_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[n].zero      = 0;
}

void idt_set_gate(int n, uint64_t base, uint16_t sel, uint8_t fl) {
    idt_set_ist(n, base, sel, fl, 0);
}

void idt_init(void) {
    __asm__ volatile("cli");

    gdt_init();

    tss.ist1       = (uint64_t)(df_stack + DF_STACK_SIZE);
    tss.rsp0       = (uint64_t)(df_stack + DF_STACK_SIZE);
    tss.iopb_offset = sizeof(TSS64);
    tss_load(0x18);

    idt_ptr.limit = (uint16_t)(sizeof(IDTEntry) * 256 - 1);
    idt_ptr.base  = (uint64_t)&idt;

    idt_set_ist( 0,(uint64_t)isr0, 0x08,0x8E,0);
    idt_set_ist( 1,(uint64_t)isr1, 0x08,0x8E,0);
    idt_set_ist( 2,(uint64_t)isr2, 0x08,0x8E,0);
    idt_set_ist( 3,(uint64_t)isr3, 0x08,0x8E,0);
    idt_set_ist( 4,(uint64_t)isr4, 0x08,0x8E,0);
    idt_set_ist( 5,(uint64_t)isr5, 0x08,0x8E,0);
    idt_set_ist( 6,(uint64_t)isr6, 0x08,0x8E,0);
    idt_set_ist( 7,(uint64_t)isr7, 0x08,0x8E,0);
    idt_set_ist( 8,(uint64_t)isr8, 0x08,0x8E,1); /* #DF: IST1 */
    idt_set_ist( 9,(uint64_t)isr9, 0x08,0x8E,0);
    idt_set_ist(10,(uint64_t)isr10,0x08,0x8E,0);
    idt_set_ist(11,(uint64_t)isr11,0x08,0x8E,0);
    idt_set_ist(12,(uint64_t)isr12,0x08,0x8E,0);
    idt_set_ist(13,(uint64_t)isr13,0x08,0x8E,0);
    idt_set_ist(14,(uint64_t)isr14,0x08,0x8E,0);
    idt_set_ist(15,(uint64_t)isr15,0x08,0x8E,0);
    idt_set_ist(16,(uint64_t)isr16,0x08,0x8E,0);
    idt_set_ist(17,(uint64_t)isr17,0x08,0x8E,0);
    idt_set_ist(18,(uint64_t)isr18,0x08,0x8E,0);
    idt_set_ist(19,(uint64_t)isr19,0x08,0x8E,0);
    idt_set_ist(20,(uint64_t)isr20,0x08,0x8E,0);
    idt_set_ist(21,(uint64_t)isr21,0x08,0x8E,0);
    idt_set_ist(22,(uint64_t)isr22,0x08,0x8E,0);
    idt_set_ist(23,(uint64_t)isr23,0x08,0x8E,0);
    idt_set_ist(24,(uint64_t)isr24,0x08,0x8E,0);
    idt_set_ist(25,(uint64_t)isr25,0x08,0x8E,0);
    idt_set_ist(26,(uint64_t)isr26,0x08,0x8E,0);
    idt_set_ist(27,(uint64_t)isr27,0x08,0x8E,0);
    idt_set_ist(28,(uint64_t)isr28,0x08,0x8E,0);
    idt_set_ist(29,(uint64_t)isr29,0x08,0x8E,0);
    idt_set_ist(30,(uint64_t)isr30,0x08,0x8E,0);
    idt_set_ist(31,(uint64_t)isr31,0x08,0x8E,0);

    /* PIC remap: IRQ0-7 -> INT32-39, IRQ8-15 -> INT40-47 */
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,0x00); outb(0xA1,0x00);

    idt_set_ist(32,(uint64_t)irq0, 0x08,0x8E,0);
    idt_set_ist(33,(uint64_t)irq1, 0x08,0x8E,0);
    idt_set_ist(34,(uint64_t)irq2, 0x08,0x8E,0);
    idt_set_ist(35,(uint64_t)irq3, 0x08,0x8E,0);
    idt_set_ist(36,(uint64_t)irq4, 0x08,0x8E,0);
    idt_set_ist(37,(uint64_t)irq5, 0x08,0x8E,0);
    idt_set_ist(38,(uint64_t)irq6, 0x08,0x8E,0);
    idt_set_ist(39,(uint64_t)irq7, 0x08,0x8E,0);
    idt_set_ist(40,(uint64_t)irq8, 0x08,0x8E,0);
    idt_set_ist(41,(uint64_t)irq9, 0x08,0x8E,0);
    idt_set_ist(42,(uint64_t)irq10,0x08,0x8E,0);
    idt_set_ist(43,(uint64_t)irq11,0x08,0x8E,0);
    idt_set_ist(44,(uint64_t)irq12,0x08,0x8E,0);
    idt_set_ist(45,(uint64_t)irq13,0x08,0x8E,0);
    idt_set_ist(46,(uint64_t)irq14,0x08,0x8E,0);
    idt_set_ist(47,(uint64_t)irq15,0x08,0x8E,0);

    idt_load(&idt_ptr);
}
