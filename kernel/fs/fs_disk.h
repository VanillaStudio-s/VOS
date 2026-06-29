#ifndef FS_DISK_H
#define FS_DISK_H

#include "../kernel.h"
#include "fs_ram.h"   /* gives us FSNode, MAX_NODES, FS_CONTENT_MAX + live globals */

/* ── On-disk layout ─────────────────────────────────────────────────────────
 *
 *  LBA ATA_FS_START_LBA+0   : FSDiskHeader  (512 bytes, 1 sector)
 *  LBA ATA_FS_START_LBA+1 … : FSDiskNode[0..node_count-1]
 *                             each node = 2092 bytes → 5 sectors (padded)
 *
 *  Total worst case (64 nodes):
 *    1 header sector + 64 * 5 node sectors = 321 sectors ≈ 160 KiB
 * ─────────────────────────────────────────────────────────────────────────── */

#define FS_DISK_MAGIC        0x534F5646u   /* "VFOS" */
#define FS_DISK_VERSION      1u
#define FS_DISK_NODE_SECTORS 5            /* sectors per node (5 * 512 = 2560 ≥ 2092) */

/* Header — exactly 512 bytes */
typedef struct {
    uint32_t magic;          /* FS_DISK_MAGIC                      */
    uint32_t version;        /* FS_DISK_VERSION                    */
    uint32_t node_count;     /* number of valid nodes saved        */
    uint32_t current_dir;    /* current_dir_idx at save time       */
    uint8_t  reserved[496]; /* pad to 512 bytes                   */
} __attribute__((packed)) FSDiskHeader;

/* Per-node record stored on disk — content embedded, no pointers */
typedef struct {
    char     name[32];
    int32_t  type;
    int32_t  parent_idx;
    int32_t  content_len;
    char     content[FS_CONTENT_MAX];   /* up to 2048 bytes */
    /* total: 32+4+4+4+2048 = 2092 bytes; padded to FS_DISK_NODE_SECTORS*512 by write */
} __attribute__((packed)) FSDiskNode;

/* Save fs_tree[] + current_dir_idx to ATA disk.
   Returns 0 on success, -1 on error. */
int fs_disk_save(void);

/* Load fs_tree[] + current_dir_idx from ATA disk.
   Returns 0 on success, -1 if no valid FS found (fresh boot). */
int fs_disk_load(void);

/* Returns 1 if a valid VOS filesystem is present on disk. */
int fs_disk_present(void);

#endif