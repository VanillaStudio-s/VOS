#include "ata.h"
#include "../kernel.h"

/* ── Helpers ───────────────────────────────────────────────────────────────── */

/* 400 ns delay: read alt-status 4× */
static inline void ata_delay(void) {
    inb(ATA_ALT_STATUS); inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS); inb(ATA_ALT_STATUS);
}

/* Wait until BSY clears; returns status byte or 0xFF on timeout */
static uint8_t ata_wait_bsy(void) {
    uint32_t timeout = 0x100000;
    uint8_t  status;
    do {
        status = inb(ATA_STATUS);
        if (!(status & ATA_SR_BSY)) return status;
    } while (--timeout);
    return 0xFF; /* timeout */
}

/* Wait until DRQ is set (data ready); returns status or 0xFF on timeout */
static uint8_t ata_wait_drq(void) {
    uint32_t timeout = 0x100000;
    uint8_t  status;
    do {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR)  return status;   /* error */
        if (status & ATA_SR_DRQ)  return status;   /* ready */
    } while (--timeout);
    return 0xFF;
}

/* ── LBA28 command setup ────────────────────────────────────────────────────── */
static void ata_setup_lba28(uint32_t lba, uint8_t count) {
    ata_wait_bsy();
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); /* master + LBA mode */
    outb(ATA_SECT_COUNT, count);
    outb(ATA_LBA_LO,  (uint8_t)(lba      ));
    outb(ATA_LBA_MID, (uint8_t)(lba >>  8));
    outb(ATA_LBA_HI,  (uint8_t)(lba >> 16));
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

int ata_detect(void) {
    outb(ATA_DRIVE_HEAD, 0xA0);   /* select master */
    ata_delay();
    outb(ATA_CMD, ATA_CMD_ID);    /* IDENTIFY */
    ata_delay();
    uint8_t status = inb(ATA_STATUS);
    if (status == 0x00 || status == 0xFF) return 0;  /* no drive */
    ata_wait_bsy();
    /* discard IDENTIFY data */
    for (int i = 0; i < 256; i++) inb(ATA_DATA); /* byte-mode discard */
    return 1;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void* buf) {
    if (!count) return 0;
    ata_setup_lba28(lba, count);
    outb(ATA_CMD, ATA_CMD_READ);
    ata_delay();

    uint16_t* p = (uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        uint8_t st = ata_wait_drq();
        if (st == 0xFF || (st & ATA_SR_ERR)) return -1;
        /* read 256 words = 512 bytes */
        for (int i = 0; i < 256; i++) {
            uint16_t word;
            __asm__ volatile("inw %1, %0" : "=a"(word) : "Nd"((uint16_t)ATA_DATA));
            *p++ = word;
        }
        ata_delay();
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void* buf) {
    if (!count) return 0;
    ata_setup_lba28(lba, count);
    outb(ATA_CMD, ATA_CMD_WRITE);
    ata_delay();

    const uint16_t* p = (const uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        uint8_t st = ata_wait_drq();
        if (st == 0xFF || (st & ATA_SR_ERR)) return -1;
        /* write 256 words = 512 bytes */
        for (int i = 0; i < 256; i++) {
            uint16_t word = *p++;
            __asm__ volatile("outw %0, %1" :: "a"(word), "Nd"((uint16_t)ATA_DATA));
        }
        ata_delay();
    }

    /* flush write cache */
    outb(ATA_CMD, ATA_CMD_FLUSH);
    ata_wait_bsy();
    return 0;
}