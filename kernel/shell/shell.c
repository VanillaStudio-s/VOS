#include "kernel.h"
#include "drivers/screen.h"
#include "drivers/keyboard.h"
#include "fs/fs.h"
#include "sys/user.h"
#include "sys/system.h"
#include "sys/clipboard.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/notify.h"
#include "apps/texteditor.h"
#include "shell/shell.h"
#include "net/net.h"
#include "net/net_init.h"
#include "net/ip.h"
#include "net/dns.h"
#include "cpu/timer.h"
#include "sys/shutdown.h"

/* ── Output handler ────────────────────────────────────── */
static output_func _out_fn = (void*)0;

static void _o(const char* s) {
    if (_out_fn) _out_fn(s);
    else my_puts(s);
}

static void _puts_u8(uint8_t n) {
    char buf[4]; int i = 0;
    if (n >= 100) buf[i++] = (char)('0' + n / 100);
    if (n >= 10)  buf[i++] = (char)('0' + (n / 10) % 10);
    buf[i++] = (char)('0' + n % 10);
    buf[i]   = '\0';
    _o(buf);
}
static void _puts_ip(IpAddr ip) {
    _puts_u8(ip.b[0]); _o(".");
    _puts_u8(ip.b[1]); _o(".");
    _puts_u8(ip.b[2]); _o(".");
    _puts_u8(ip.b[3]);
}
static void _puts_mac(MacAddr m) {
    static const char h[] = "0123456789ABCDEF";
    char buf[3]; buf[2] = '\0';
    for (int i = 0; i < 6; i++) {
        if (i) _o(":");
        buf[0] = h[m.b[i] >> 4];
        buf[1] = h[m.b[i] & 0xF];
        _o(buf);
    }
}
static void _puts_u32(uint32_t n) {
    if (!n) { _o("0"); return; }
    char t[11]; int i = 0;
    while (n) { t[i++] = (char)('0' + n % 10); n /= 10; }
    char s[12]; int k;
    for (k = 0; k < i; k++) s[k] = t[i - 1 - k];
    s[k] = '\0';
    _o(s);
}

/* ── Shell prompt (GUI terminal windows only) ──────────── */
void shell_prompt(void) {
    char buf[80];
    fs_pwd(buf, 80);
    my_puts(buf);
    my_puts(" > ");
}

static int starts_with(const char* s, const char* prefix) {
    int i = 0;
    while (prefix[i]) { if (s[i] != prefix[i]) return 0; i++; }
    return 1;
}

void process_command(const char* cmd, output_func out) {
    _out_fn = out;

    if (my_strcmp(cmd, "help") == 0) {
        _o("--- VOS Terminal Help ---\n\n");
        _o("[Filesystem]\n");
        _o("  ls, pwd, cd, mkdir, touch\n");
        _o("  cat, peek, write, rm, cp, mv\n\n");
        _o("[System & Tools]\n");
        _o("  neofetch, clear, scrot, edit\n");
        _o("  echo, copy, paste, theme\n\n");
        _o("[Network & User]\n");
        _o("  user, users, deluser, pw, logout\n");
        _o("  ifconfig, ping\n\n");
        _o("[Power]\n");
        _o("  shutdown, restart\n");
    }
    else if (my_strcmp(cmd, "clear") == 0)    my_clear_screen();
    else if (my_strcmp(cmd, "pwd") == 0) {
        char buf[80]; fs_pwd(buf, 80); _o(buf); _o("\n");
    }
    else if (my_strcmp(cmd, "shutdown") == 0) sys_shutdown();
    else if (my_strcmp(cmd, "restart") == 0)  sys_restart();
    else if (my_strcmp(cmd, "logout") == 0)   sys_logout();
    else if (my_strcmp(cmd, "ls") == 0)       fs_ls();
    else if (my_strcmp(cmd, "users") == 0)    list_user_system();
    else if (my_strcmp(cmd, "pw") == 0)       handle_pw_system();
    else if (my_strcmp(cmd, "keyboardlang") == 0) show_keyboard_types();
    else if (my_strcmp(cmd, "lang") == 0)     language_selector("");
    else if (my_strcmp(cmd, "paste") == 0) {
        if (clipboard_empty()) _o("Clipboard empty.\n");
        else { _o(clipboard_paste()); _o("\n"); }
    }
    else if (my_strcmp(cmd, "theme") == 0 || my_strcmp(cmd, "theme dark") == 0) {
        theme_apply_dark();  _o("Theme: Dark\n");
    }
    else if (my_strcmp(cmd, "theme light") == 0) {
        theme_apply_light(); _o("Theme: Light\n");
    }
    else if (starts_with(cmd, "deluser "))        delete_user_system(cmd + 8);
    else if (starts_with(cmd, "keyboardlang ")) {
        if (cmd[13] == '0')      { keyboard_lang_selector(0); _o("US-English\n"); }
        else if (cmd[13] == '1') { keyboard_lang_selector(1); _o("DE-German\n");  }
        else                       _o("Invalid layout (0=EN, 1=DE)\n");
    }
    else if (starts_with(cmd, "lang "))           language_selector(cmd + 5);
    else if (my_strcmp(cmd, "scrot") == 0) {
        fs_screenshot();
        notify_push("Screenshot", "Saved to Desktop/screenshot.bmp", NOTIFY_SUCCESS);
        _o("Screenshot captured.\n");
    }
    else if (starts_with(cmd, "cp ")) {
        const char* rest = cmd + 3; char src[32]; int i = 0;
        while (rest[i] && rest[i] != ' ' && i < 31) { src[i] = rest[i]; i++; } src[i] = '\0';
        const char* dst = (rest[i] == ' ') ? (rest + i + 1) : "";
        if (!src[0] || !dst[0]) _o("Usage: cp <src> <dst>\n");
        else fs_cp(src, dst);
    }
    else if (starts_with(cmd, "mv ")) {
        const char* rest = cmd + 3; char src[32]; int i = 0;
        while (rest[i] && rest[i] != ' ' && i < 31) { src[i] = rest[i]; i++; } src[i] = '\0';
        const char* dst = (rest[i] == ' ') ? (rest + i + 1) : "";
        if (!src[0] || !dst[0]) _o("Usage: mv <src> <dst>\n");
        else fs_mv(src, dst);
    }
    else if (starts_with(cmd, "rm "))    fs_rm(cmd + 3);
    else if (starts_with(cmd, "edit ")) {
        app_texteditor_open(cmd + 5);
        _out_fn = (void*)0; return;
    }
    else if (my_strcmp(cmd, "edit") == 0) {
        app_texteditor_open((void*)0);
        _out_fn = (void*)0; return;
    }
    else if (starts_with(cmd, "mkdir "))  fs_mkdir(cmd + 6);
    else if (starts_with(cmd, "cd "))     fs_cd(cmd + 3);
    else if (starts_with(cmd, "touch "))  fs_touch(cmd + 6);
    else if (starts_with(cmd, "write "))  fs_write(cmd + 6);
    else if (starts_with(cmd, "peek "))   fs_peek(cmd + 5);
    else if (starts_with(cmd, "cat "))    fs_peek(cmd + 4);
    else if (starts_with(cmd, "user "))   handle_user_system(cmd);
    else if (starts_with(cmd, "copy ")) { clipboard_copy(cmd + 5); _o("Copied.\n"); }
    else if (starts_with(cmd, "echo "))   { _o(cmd + 5); _o("\n"); }

    /* ── Network ──────────────────────────────────────────── */
    else if (my_strcmp(cmd, "ifconfig") == 0) {
        if (!net_up) { _o("No network interface found.\n"); }
        else {
            _o("eth0  MAC: "); _puts_mac(net_mac); _o("\n");
            if (net_has_ip) {
                _o("      IP:  "); _puts_ip(net_ip);      _o("\n");
                _o("      Mask:"); _puts_ip(net_mask);    _o("\n");
                _o("      GW:  "); _puts_ip(net_gw);      _o("\n");
                _o("      DNS: "); _puts_ip(net_dns_srv); _o("\n");
            } else {
                _o("      No IP address (DHCP failed)\n");
            }
        }
    }
    else if (starts_with(cmd, "ping ")) {
        if (!net_up || !net_has_ip) {
            _o("ping: no network\n");
        } else {
            const char* host = cmd + 5;
            IpAddr dst;
            if (!dns_resolve(host, &dst)) {
                _o("ping: unknown host\n");
            } else {
                _o("PING "); _o(host); _o(" ("); _puts_ip(dst); _o(")\n");
                for (int i = 1; i <= 4; i++) {
                    icmp_echo_got = 0;
                    uint32_t t0 = timer_get_ticks();
                    icmp_ping(dst, 0x4F53, (uint16_t)i);
                    while (!icmp_echo_got && (timer_get_ticks() - t0) < 100)
                        net_poll();
                    if (icmp_echo_got) {
                        uint32_t ms = (timer_get_ticks() - t0) * 10;
                        _o("Reply from "); _puts_ip(dst);
                        _o(": seq=");      _puts_u32((uint32_t)i);
                        _o(" time=");      _puts_u32(ms);
                        _o("ms\n");
                    } else {
                        _o("Request timeout for seq=");
                        _puts_u32((uint32_t)i); _o("\n");
                    }
                }
            }
        }
    }
    else if (cmd[0] != '\0') neofetch_command(cmd, out);

    _out_fn = (void*)0;
}

void neofetch_command(const char* cmd, output_func out) {
    _out_fn = out;
    if (my_strcmp(cmd, "neofetch") == 0) {
        _o("  __      ______   _____   | VOS v0.4.2\n");
        _o("  \\ \\    / / __ \\ / ____|  | USER: ");
        _o(user_database[active_user_idx]); _o("\n");
        _o("   \\ \\  / / |  | | (___    | DEVELOP BY MERDO ONLY\n");
        _o("    \\ \\/ /| |  | |\\___ \\   | ---------------------------\n");
        _o("     \\  / | |__| |____) |  | "); show_cpu_info();
        _o("      \\/   \\____/|_____/   | "); show_ram_info();
        _o("                           | "); show_storage_info();
    } else {
        _o("Unknown command. Type 'help'.\n");
    }
    _out_fn = (void*)0;
}