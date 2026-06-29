#include "kernel.h"
#include "net/pci.h"

#define PCI_ADDR 0xCF8u
#define PCI_DATA 0xCFCu

static uint32_t _mk_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    return (1u << 31) | ((uint32_t)bus  << 16) | ((uint32_t)slot << 11) |
           ((uint32_t)func << 8) | (off & 0xFC);
}

uint32_t pci_read32(uint8_t b, uint8_t s, uint8_t f, uint8_t o) {
    outl(PCI_ADDR, _mk_addr(b, s, f, o));
    return inl(PCI_DATA);
}

void pci_write32(uint8_t b, uint8_t s, uint8_t f, uint8_t o, uint32_t v) {
    outl(PCI_ADDR, _mk_addr(b, s, f, o));
    outl(PCI_DATA, v);
}

uint16_t pci_read16(uint8_t b, uint8_t s, uint8_t f, uint8_t o) {
    return (uint16_t)(pci_read32(b, s, f, o & 0xFC) >> ((o & 2) * 8));
}

uint8_t pci_read8(uint8_t b, uint8_t s, uint8_t f, uint8_t o) {
    return (uint8_t)(pci_read32(b, s, f, o & 0xFC) >> ((o & 3) * 8));
}

int pci_find(uint16_t vendor, uint16_t device,
             uint8_t *ob, uint8_t *os, uint8_t *of) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read32((uint8_t)bus, (uint8_t)slot, 0, 0);
            if ((id & 0xFFFF) == vendor && ((id >> 16) & 0xFFFF) == device) {
                *ob = (uint8_t)bus; *os = (uint8_t)slot; *of = 0;
                return 1;
            }
        }
    }
    return 0;
}

uint32_t pci_bar(uint8_t b, uint8_t s, uint8_t f, int bar) {
    return pci_read32(b, s, f, (uint8_t)(0x10 + bar * 4));
}

void pci_enable_busmaster(uint8_t b, uint8_t s, uint8_t f) {
    uint32_t cmd = pci_read32(b, s, f, 0x04) & 0xFFFF;
    cmd |= 0x07;  /* I/O space | MMIO space | bus master */
    pci_write32(b, s, f, 0x04, cmd);
}
