#include "fs_ram.h"

/* ── Live RAM state ──────────────────────────────────────────────────────── */
struct FSNode fs_tree[MAX_NODES];
int           fs_node_count   = 0;
int           current_dir_idx = 0;

/* ── Node allocator ──────────────────────────────────────────────────────── */
int fs_mknode(const char* name, int type, int parent) {
    if (fs_node_count >= MAX_NODES) return -1;
    int idx = fs_node_count++;
    int k = 0;
    while (name[k] && k < 31) { fs_tree[idx].name[k] = name[k]; k++; }
    fs_tree[idx].name[k]     = '\0';
    fs_tree[idx].type        = type;
    fs_tree[idx].parent_idx  = parent;
    fs_tree[idx].content     = (char*)0;
    fs_tree[idx].content_len = 0;
    return idx;
}