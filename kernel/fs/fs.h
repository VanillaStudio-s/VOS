#ifndef FS_H
#define FS_H

#define MAX_NODES      64
#define NODE_TYPE_DIR   1
#define NODE_TYPE_FILE  2
#define NVRAM_MAGIC     0x414E4F56
#define FS_CONTENT_MAX  2048   /* max bytes per file, heap-allocated */

struct FSNode {
    char  name[32];
    int   type;
    int   parent_idx;
    char* content;      /* heap-alloc via kmalloc, NULL if empty or dir */
    int   content_len;
};

/* ── Live RAM state (defined in fs_ram.c) ────────────────────────────────── */
extern struct FSNode fs_tree[MAX_NODES];
extern int           fs_node_count;
extern int           current_dir_idx;

void fs_init(const char* username);
void fs_mkdir(const char* name);
void fs_touch(const char* name);
void fs_write(const char* args);
void fs_peek(const char* name);
void fs_cd(const char* path);
void fs_ls(void);
void fs_pwd(char* buf, int maxlen);
void fs_rm(const char* name);
void fs_cp(const char* src, const char* dst);
void fs_mv(const char* src, const char* dst);
void fs_screenshot(void);
int  fs_find_dir(const char* name);
int  fs_get_home_idx(void);
void fs_get_path(int idx, char* buf, int maxlen);
void fs_mkdir_in(int dir, const char* name);
void fs_touch_in(int dir, const char* name);

#endif