#ifndef NET_PCI_H
#define NET_PCI_H
#include "kernel.h"

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void     pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t v);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
uint8_t  pci_read8 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);

/* Find device by vendor+device ID; fills bus/slot/func; returns 1 if found */
int      pci_find(uint16_t vendor, uint16_t device,
                  uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func);

/* Return raw BAR value for bar index 0-5 */
uint32_t pci_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar);

/* Enable I/O space + bus mastering in command register */
void     pci_enable_busmaster(uint8_t bus, uint8_t slot, uint8_t func);

#endif
