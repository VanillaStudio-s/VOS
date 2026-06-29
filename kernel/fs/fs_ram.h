#ifndef FS_RAM_H
#define FS_RAM_H

#include "fs.h"

/* ── Live filesystem state (RAM) ─────────────────────────────────────────────
 *  Canonical runtime copies of the FS tree.
 *  fs_disk.c serialises/deserialises them to/from ATA.
 *  fs.c operates on them for all user-facing commands.
 * ─────────────────────────────────────────────────────────────────────────── */

extern struct FSNode fs_tree[MAX_NODES];
extern int           fs_node_count;
extern int           current_dir_idx;

/* Allocate a new node in fs_tree[]. Returns index, or -1 on overflow. */
int fs_mknode(const char* name, int type, int parent);

#endif