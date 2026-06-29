#include "kernel.h"
#include "drivers/screen.h"
#include "mem/heap.h"
#include "fs/fs.h"
#include "fs/fs_ram.h"

static int _user_home_idx = 0;

void fs_init(const char* username) {
    for (int i = 0; i < MAX_NODES; i++) {
        fs_tree[i].name[0]     = '\0';
        fs_tree[i].type        = 0;
        fs_tree[i].parent_idx  = 0;
        fs_tree[i].content     = (char*)0;
        fs_tree[i].content_len = 0;
    }

    /* node 0 = root "/" (parent points to itself) */
    fs_tree[0].name[0] = '/'; fs_tree[0].name[1] = '\0';
    fs_tree[0].type       = NODE_TYPE_DIR;
    fs_tree[0].parent_idx = 0;
    fs_node_count = 1;

    int home_idx = fs_mknode("home",  NODE_TYPE_DIR, 0);
    int user_idx = fs_mknode(username, NODE_TYPE_DIR, home_idx);
    fs_mknode("Documents", NODE_TYPE_DIR, user_idx);
    fs_mknode("Downloads", NODE_TYPE_DIR, user_idx);
    fs_mknode("Desktop",   NODE_TYPE_DIR, user_idx);
    fs_mknode("Video",     NODE_TYPE_DIR, user_idx);
    fs_mknode("Picture",   NODE_TYPE_DIR, user_idx);
    fs_mknode("Music",     NODE_TYPE_DIR, user_idx);

    /* system dirs */
    int etc_idx  = fs_mknode("etc",  NODE_TYPE_DIR,  0);
    int etc_host = fs_mknode("hostname", NODE_TYPE_FILE, etc_idx);
    fs_tree[etc_host].content     = (char*)0;
    fs_tree[etc_host].content_len = 0;
    fs_mknode("tmp", NODE_TYPE_DIR, 0);
    fs_mknode("var", NODE_TYPE_DIR, 0);

    _user_home_idx  = user_idx;
    current_dir_idx = user_idx;
}

void fs_mkdir(const char* n) {
    if (fs_node_count >= MAX_NODES) { my_puts_color("Error: node limit\n", 0x0C); return; }
    if (!n[0]) { my_puts_color("Error: missing name\n", 0x0C); return; }
    for (int i = 0; i < fs_node_count; i++)
        if (fs_tree[i].parent_idx == current_dir_idx && my_strcmp(fs_tree[i].name, n) == 0)
            { my_puts_color("Error: already exists\n", 0x0C); return; }
    fs_mknode(n, NODE_TYPE_DIR, current_dir_idx);
    my_puts_color("Directory created.\n", 0x0A);
}

void fs_touch(const char* n) {
    if (fs_node_count >= MAX_NODES) { my_puts_color("Error: limit\n", 0x0C); return; }
    if (!n[0]) { my_puts_color("Error: missing filename\n", 0x0C); return; }
    fs_mknode(n, NODE_TYPE_FILE, current_dir_idx);
    my_puts_color("File created.\n", 0x0A);
}

void fs_write(const char* args) {
    char tgt[32]; int i = 0, j = 0;
    while (args[i] && args[i] != ' ' && j < 31) tgt[j++] = args[i++];
    tgt[j] = '\0'; if (args[i] == ' ') i++;
    for (int f = 0; f < fs_node_count; f++)
        if (fs_tree[f].parent_idx == current_dir_idx && fs_tree[f].type == NODE_TYPE_FILE
            && my_strcmp(fs_tree[f].name, tgt) == 0) {
            int len = 0; while (args[i + len]) len++;
            if (len > FS_CONTENT_MAX - 1) len = FS_CONTENT_MAX - 1;
            if (fs_tree[f].content) kfree(fs_tree[f].content);
            fs_tree[f].content = (char*)kmalloc(len + 1);
            if (fs_tree[f].content) {
                int c = 0; while (c < len) { fs_tree[f].content[c] = args[i + c]; c++; }
                fs_tree[f].content[c] = '\0';
                fs_tree[f].content_len = len;
            } else {
                fs_tree[f].content_len = 0;
            }
            my_puts_color("Written.\n", 0x0A); return;
        }
    my_puts_color("Error: file not found\n", 0x0C);
}

void fs_peek(const char* name) {
    for (int f = 0; f < fs_node_count; f++)
        if (fs_tree[f].parent_idx == current_dir_idx && fs_tree[f].type == NODE_TYPE_FILE
            && my_strcmp(fs_tree[f].name, name) == 0) {
            my_puts_color("[", 0x0E); my_puts_color(fs_tree[f].name, 0x0E);
            my_puts_color("]:\n", 0x0E);
            if (fs_tree[f].content) my_puts(fs_tree[f].content);
            my_puts("\n"); return;
        }
    my_puts_color("Error: not found\n", 0x0C);
}

void fs_cd(const char* p) {
    if (!p[0] || (p[0] == '.' && !p[1])) return;

    if (p[0] == '~' && !p[1]) { current_dir_idx = _user_home_idx; return; }
    if (p[0] == '/' && !p[1]) { current_dir_idx = 0; return; }

    if (p[0] == '.' && p[1] == '.' && !p[2]) {
        if (current_dir_idx != 0)
            current_dir_idx = fs_tree[current_dir_idx].parent_idx;
        return;
    }

    /* absolute path */
    if (p[0] == '/') {
        int idx = 0;
        const char* s = p + 1;
        while (*s) {
            char seg[32]; int j = 0;
            while (*s && *s != '/' && j < 31) { seg[j++] = *s++; }
            seg[j] = '\0';
            if (*s == '/') s++;
            if (!seg[0]) continue;
            int found = -1;
            for (int i = 0; i < fs_node_count; i++) {
                if (fs_tree[i].parent_idx == idx && fs_tree[i].type == NODE_TYPE_DIR
                    && my_strcmp(fs_tree[i].name, seg) == 0) { found = i; break; }
            }
            if (found < 0) { my_puts_color("Error: path not found\n", 0x0C); return; }
            idx = found;
        }
        current_dir_idx = idx;
        return;
    }

    /* relative path */
    for (int i = 0; i < fs_node_count; i++)
        if (fs_tree[i].parent_idx == current_dir_idx && fs_tree[i].type == NODE_TYPE_DIR
            && my_strcmp(fs_tree[i].name, p) == 0)
            { current_dir_idx = i; return; }
    my_puts_color("Error: dir not found\n", 0x0C);
}

void fs_ls(void) {
    my_puts("Contents:\n"); int cnt = 0;
    for (int i = 0; i < fs_node_count; i++)
        if (fs_tree[i].parent_idx == current_dir_idx && i != 0) {
            if (fs_tree[i].type == NODE_TYPE_DIR) my_puts_color(" [DIR]  ", 0x0B);
            else                                  my_puts_color(" [FILE] ", 0x0A);
            my_puts(fs_tree[i].name); my_puts("\n"); cnt++;
        }
    if (!cnt) my_puts_color("(empty)\n", 0x0E);
}

void fs_rm(const char* name) {
    for (int i = 1; i < fs_node_count; i++) {
        if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == current_dir_idx
            && my_strcmp(fs_tree[i].name, name) == 0) {
            if (fs_tree[i].type == NODE_TYPE_DIR) {
                for (int j = 1; j < fs_node_count; j++)
                    if (fs_tree[j].type != 0 && fs_tree[j].parent_idx == i)
                        { my_puts_color("Error: dir not empty\n", 0x0C); return; }
            }
            if (fs_tree[i].content) { kfree(fs_tree[i].content); fs_tree[i].content = (char*)0; }
            fs_tree[i].content_len = 0;
            fs_tree[i].type    = 0;
            fs_tree[i].name[0] = '\0';
            my_puts_color("Removed.\n", 0x0A); return;
        }
    }
    my_puts_color("Not found.\n", 0x0C);
}

void fs_cp(const char* src, const char* dst) {
    int si = -1;
    for (int i = 1; i < fs_node_count; i++)
        if (fs_tree[i].type == NODE_TYPE_FILE && fs_tree[i].parent_idx == current_dir_idx
            && my_strcmp(fs_tree[i].name, src) == 0) { si = i; break; }
    if (si < 0) { my_puts_color("Error: src not found\n", 0x0C); return; }
    for (int i = 1; i < fs_node_count; i++)
        if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == current_dir_idx
            && my_strcmp(fs_tree[i].name, dst) == 0)
            { my_puts_color("Error: dst exists\n", 0x0C); return; }
    int ni = fs_mknode(dst, NODE_TYPE_FILE, current_dir_idx);
    if (ni < 0) { my_puts_color("Error: limit\n", 0x0C); return; }
    if (fs_tree[si].content && fs_tree[si].content_len > 0) {
        fs_tree[ni].content = (char*)kmalloc(fs_tree[si].content_len + 1);
        if (fs_tree[ni].content) {
            int j = 0;
            while (j < fs_tree[si].content_len) { fs_tree[ni].content[j] = fs_tree[si].content[j]; j++; }
            fs_tree[ni].content[j] = '\0';
            fs_tree[ni].content_len = j;
        }
    }
    my_puts_color("Copied.\n", 0x0A);
}

void fs_mv(const char* src, const char* dst) {
    for (int i = 1; i < fs_node_count; i++) {
        if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == current_dir_idx
            && my_strcmp(fs_tree[i].name, src) == 0) {
            int j = 0;
            while (dst[j] && j < 31) { fs_tree[i].name[j] = dst[j]; j++; }
            fs_tree[i].name[j] = '\0';
            my_puts_color("Renamed.\n", 0x0A); return;
        }
    }
    my_puts_color("Not found.\n", 0x0C);
}

int fs_find_dir(const char* name) {
    for (int i = 1; i < fs_node_count; i++)
        if (fs_tree[i].type == NODE_TYPE_DIR && my_strcmp(fs_tree[i].name, name) == 0) return i;
    return -1;
}

void fs_screenshot(void) {
    int desk   = fs_find_dir("Desktop");
    int parent = (desk >= 0) ? desk : current_dir_idx;
    int fi = -1;
    for (int i = 1; i < fs_node_count; i++)
        if (fs_tree[i].type == NODE_TYPE_FILE && fs_tree[i].parent_idx == parent
            && my_strcmp(fs_tree[i].name, "screenshot.bmp") == 0) { fi = i; break; }
    if (fi < 0) fi = fs_mknode("screenshot.bmp", NODE_TYPE_FILE, parent);
    if (fi >= 0) {
        const char* info = "BMP 1024x768 @0x200000 (persists in RAM only)";
        int len = 0; while (info[len]) len++;
        if (fs_tree[fi].content) kfree(fs_tree[fi].content);
        fs_tree[fi].content = (char*)kmalloc(len + 1);
        if (fs_tree[fi].content) {
            int j = 0; while (j < len) { fs_tree[fi].content[j] = info[j]; j++; }
            fs_tree[fi].content[j] = '\0';
            fs_tree[fi].content_len = len;
        }
    }
}

int fs_get_home_idx(void) { return _user_home_idx; }

void fs_mkdir_in(int dir, const char* name) {
    if (!name || !name[0] || fs_node_count >= MAX_NODES) return;
    for (int i = 0; i < fs_node_count; i++)
        if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == dir
            && my_strcmp(fs_tree[i].name, name) == 0) return;
    fs_mknode(name, NODE_TYPE_DIR, dir);
}

void fs_touch_in(int dir, const char* name) {
    if (!name || !name[0] || fs_node_count >= MAX_NODES) return;
    fs_mknode(name, NODE_TYPE_FILE, dir);
}

void fs_get_path(int idx, char* buf, int maxlen) {
    int old = current_dir_idx;
    current_dir_idx = idx;
    fs_pwd(buf, maxlen);
    current_dir_idx = old;
}

void fs_pwd(char* buf, int maxlen) {
    if (current_dir_idx == 0) {
        if (maxlen > 1) { buf[0] = '/'; buf[1] = '\0'; }
        return;
    }
    int path[MAX_NODES];
    int depth = 0;
    int idx   = current_dir_idx;
    while (idx != 0 && depth < MAX_NODES) {
        path[depth++] = idx;
        idx = fs_tree[idx].parent_idx;
    }
    int pos = 0;
    for (int i = depth - 1; i >= 0 && pos < maxlen - 2; i--) {
        buf[pos++] = '/';
        const char* name = fs_tree[path[i]].name;
        for (int j = 0; name[j] && pos < maxlen - 2; j++)
            buf[pos++] = name[j];
    }
    if (pos == 0) buf[pos++] = '/';
    buf[pos] = '\0';
}