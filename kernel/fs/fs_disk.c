#include "fs_disk.h"
#include "../drivers/ata.h"
#include "../mem/heap.h"
#include "../drivers/screen.h"

/* ── Static sector buffers (avoids stack overflow in kernel) ─────────────── */
static uint8_t _sector_buf[512];
static uint8_t _node_buf[FS_DISK_NODE_SECTORS * 512];   /* 2560 bytes */

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static void _mem_zero(void* dst, int n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = 0;
}

static void _mem_copy(void* dst, const void* src, int n) {
    uint8_t* d       = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

int fs_disk_present(void) {
    if (!ata_detect()) return 0;
    if (ata_read_sectors(ATA_FS_START_LBA, 1, _sector_buf) != 0) return 0;
    FSDiskHeader* hdr = (FSDiskHeader*)_sector_buf;
    return (hdr->magic == FS_DISK_MAGIC && hdr->version == FS_DISK_VERSION);
}

int fs_disk_save(void) {
    if (!ata_detect()) return -1;

    /* ── Write header ────────────────────────────────────────────────────── */
    _mem_zero(_sector_buf, 512);
    FSDiskHeader* hdr = (FSDiskHeader*)_sector_buf;
    hdr->magic       = FS_DISK_MAGIC;
    hdr->version     = FS_DISK_VERSION;
    hdr->node_count  = (uint32_t)fs_node_count;
    hdr->current_dir = (uint32_t)current_dir_idx;

    if (ata_write_sectors(ATA_FS_START_LBA, 1, _sector_buf) != 0) return -1;

    /* ── Write each node ─────────────────────────────────────────────────── */
    for (int i = 0; i < fs_node_count; i++) {
        _mem_zero(_node_buf, sizeof(_node_buf));
        FSDiskNode* dn = (FSDiskNode*)_node_buf;

        for (int k = 0; k < 32; k++) dn->name[k] = fs_tree[i].name[k];
        dn->type       = fs_tree[i].type;
        dn->parent_idx = fs_tree[i].parent_idx;

        if (fs_tree[i].content && fs_tree[i].content_len > 0) {
            int clen = fs_tree[i].content_len;
            if (clen >= FS_CONTENT_MAX) clen = FS_CONTENT_MAX - 1;
            _mem_copy(dn->content, fs_tree[i].content, clen);
            dn->content[clen] = '\0';
            dn->content_len   = clen;
        } else {
            dn->content_len = 0;
        }

        uint32_t lba = ATA_FS_START_LBA + 1 + (uint32_t)(i * FS_DISK_NODE_SECTORS);
        if (ata_write_sectors(lba, FS_DISK_NODE_SECTORS, _node_buf) != 0) return -1;
    }

    return 0;
}

int fs_disk_load(void) {
    if (!ata_detect()) return -1;

    /* ── Read + verify header ────────────────────────────────────────────── */
    if (ata_read_sectors(ATA_FS_START_LBA, 1, _sector_buf) != 0) return -1;

    FSDiskHeader* hdr = (FSDiskHeader*)_sector_buf;
    if (hdr->magic   != FS_DISK_MAGIC)   return -1;
    if (hdr->version != FS_DISK_VERSION) return -1;

    int node_count = (int)hdr->node_count;
    int saved_dir  = (int)hdr->current_dir;
    if (node_count <= 0 || node_count > MAX_NODES) return -1;

    /* ── Clear current RAM tree ──────────────────────────────────────────── */
    for (int i = 0; i < MAX_NODES; i++) {
        if (fs_tree[i].content) { kfree(fs_tree[i].content); fs_tree[i].content = (char*)0; }
        fs_tree[i].content_len = 0;
        fs_tree[i].type        = 0;
        fs_tree[i].name[0]     = '\0';
        fs_tree[i].parent_idx  = 0;
    }

    /* ── Read nodes into RAM ─────────────────────────────────────────────── */
    for (int i = 0; i < node_count; i++) {
        uint32_t lba = ATA_FS_START_LBA + 1 + (uint32_t)(i * FS_DISK_NODE_SECTORS);
        if (ata_read_sectors(lba, FS_DISK_NODE_SECTORS, _node_buf) != 0) return -1;

        FSDiskNode* dn = (FSDiskNode*)_node_buf;

        for (int k = 0; k < 32; k++) fs_tree[i].name[k] = dn->name[k];
        fs_tree[i].type        = dn->type;
        fs_tree[i].parent_idx  = dn->parent_idx;
        fs_tree[i].content     = (char*)0;
        fs_tree[i].content_len = 0;

        if (dn->content_len > 0) {
            int clen = dn->content_len;
            if (clen >= FS_CONTENT_MAX) clen = FS_CONTENT_MAX - 1;
            fs_tree[i].content = (char*)kmalloc(clen + 1);
            if (fs_tree[i].content) {
                _mem_copy(fs_tree[i].content, dn->content, clen);
                fs_tree[i].content[clen] = '\0';
                fs_tree[i].content_len   = clen;
            }
        }
    }

    fs_node_count   = node_count;
    current_dir_idx = (saved_dir < node_count) ? saved_dir : 0;
    return 0;
}