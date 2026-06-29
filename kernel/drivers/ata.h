#ifndef ATA_H
#define ATA_H

#include "../kernel.h"

/* ── ATA PIO (Primary Bus) ─────────────────────────────────────────────────── */
/* Ports */
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECT_COUNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_CMD         0x1F7
#define ATA_ALT_STATUS  0x3F6

/* Status bits */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* Commands */
#define ATA_CMD_READ   0x20
#define ATA_CMD_WRITE  0x30
#define ATA_CMD_FLUSH  0xE7
#define ATA_CMD_ID     0xEC

/* VOS stores its FS at this LBA (sector 2048 = 1 MiB offset on disk) */
#define ATA_FS_START_LBA 2048

/* Returns 1 if an ATA drive is present, 0 otherwise */
int  ata_detect(void);

/* Read/write 'count' 512-byte sectors starting at LBA 'lba'.
   buf must be at least count*512 bytes.
   Returns 0 on success, -1 on error. */
int  ata_read_sectors (uint32_t lba, uint8_t count, void* buf);
int  ata_write_sectors(uint32_t lba, uint8_t count, const void* buf);

#endif