#include "kernel.h"
#include "mem/heap.h"
#include "cpu/rtc.h"
#include "drivers/screen.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "cpu/idt.h"
#include "cpu/irq.h"
#include "cpu/timer.h"
#include "fs/fs.h"
#include "sys/user.h"
#include "sys/system.h"
#include "gui/gui.h"
#include "gui/window.h"
#include "gui/apps.h"
#include "gui/lockscreen.h"
#include "gui/screensaver.h"
#include "gui/theme.h"
#include "gui/desktopctx.h"
#include "net/net_init.h"
#include "fs/fs_disk.h"

/* ── COM1 Serial Debug ─────────────────────────────────────────────────────── */
static void serial_init(void) {
    outb(0x3F8+1, 0x00); outb(0x3F8+3, 0x80);
    outb(0x3F8+0, 0x03); outb(0x3F8+1, 0x00);
    outb(0x3F8+3, 0x03); outb(0x3F8+2, 0xC7); outb(0x3F8+4, 0x0B);
}
static void serial_putc(char c) {
    while (!(inb(0x3F8+5) & 0x20));
    outb(0x3F8, (unsigned char)c);
}
static void serial_puts(const char* s) { while (*s) serial_putc(*s++); }
static void serial_puthex(uint64_t v) {
    const char h[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) serial_putc(h[(v >> i) & 0xF]);
}

/* ── Parse Multiboot2 framebuffer tag ──────────────────────────────────────── */
static uint64_t mb2_get_framebuffer(uint64_t mb2_ptr) {
    if (!mb2_ptr) return 0;
    MB2Info* info = (MB2Info*)mb2_ptr;
    uint8_t* tag  = (uint8_t*)(mb2_ptr + 8);
    uint8_t* end  = (uint8_t*)(mb2_ptr + info->total_size);
    while (tag < end) {
        MB2Tag* t = (MB2Tag*)tag;
        if (t->type == 0) break;
        if (t->type == 8) return ((MB2Framebuffer*)tag)->framebuffer_addr;
        tag += (t->size + 7) & ~7u;
    }
    return 0;
}

/* ── Shared globals ─────────────────────────────────────────────────────────── */
uint64_t sys_phys_ram_kb = 0;

/* ── Kernel Main ────────────────────────────────────────────────────────────── */
void kernel_main(uint64_t mb2_ptr) {

    heap_init();
    serial_init();
    serial_puts("\n[VOS] kernel_main start\n");

    uint64_t fb_addr = mb2_get_framebuffer(mb2_ptr);
    serial_puts("[VOS] fb_addr="); serial_puthex(fb_addr); serial_putc('\n');

    /* parse MB2 tag 4 for physical RAM size */
    if (mb2_ptr) {
        MB2Info* info = (MB2Info*)mb2_ptr;
        uint8_t* p   = (uint8_t*)(mb2_ptr + 8);
        uint8_t* end = (uint8_t*)(mb2_ptr + info->total_size);
        while (p < end) {
            MB2Tag* t = (MB2Tag*)p;
            if (t->type == 0) break;
            if (t->type == 4) {
                uint32_t* m = (uint32_t*)(p + 8);
                sys_phys_ram_kb = (uint64_t)m[0] + (uint64_t)m[1];
                break;
            }
            p += (t->size + 7) & ~7u;
        }
    }

    if (!fb_addr) {
        volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
        const char* msg = "VOS: No framebuffer from GRUB!";
        for (int i = 0; msg[i]; i++) vga[i] = (uint16_t)(0x4F00 | (unsigned char)msg[i]);
        __asm__ volatile("cli; hlt");
        while(1){}
    }

    __asm__ volatile("cli");
    idt_init(); irq_init(); timer_init(100);
    __asm__ volatile("sti");

    screen_init(fb_addr);

    /* boot stripe */
    for (int i = 0; i < SCREEN_WIDTH * 6; i++)
        ((volatile uint32_t*)fb_addr)[i] = 0x0000CC00;

    mouse_init();
    net_init();
    screensaver_init();

    if (fs_disk_load() != 0) {
        serial_puts("[VOS] No saved FS — fresh init.\n");
        fs_init(user_database[active_user_idx]);
    } else {
        serial_puts("[VOS] Filesystem loaded from disk.\n");
    }

    win_init();
    gui_init();

    win_create(50, 40, 240, 140, "VOS v0.3.2",
               WIN_FLAG_DRAGGABLE | WIN_FLAG_CLOSABLE | WIN_FLAG_MINIMIZABLE | WIN_FLAG_RESIZABLE);
    if (app_autostart_welcome) app_welcome_open();

    /* ── Auto-lock: show lockscreen if non-live user has a password ──────── */
    if (active_user_idx != 0 && pass_database[active_user_idx][0] != '\0') {
        gui_switch_mode(GUI_MODE_LOCKED);
        serial_puts("[VOS] Auto-lock: user requires password.\n");
    }

    serial_puts("[VOS] GUI ready\n");

    /* ── Main Loop ─────────────────────────────────────────────────────────── */
    int last_btn  = 0;
    uint32_t last_tick = 0;
    int ctrl_held = 0;
    int ext_key   = 0;

    while (1) {
        /* ── Tick / Render ─────────────────────────────────────────────────── */
        uint32_t tick = timer_get_ticks();
        if (tick != last_tick) {
            last_tick = tick;
            if (gui_mode == GUI_MODE_DESKTOP) {
                gui_tick();
                gui_render();
                /* NOTE: desktopctx_render() should be called inside gui_render()
                   just before screen_flip(). If you want it here instead,
                   call screen_flip() again after desktopctx_render(). */
                if (desktopctx_is_open()) desktopctx_render();
            } else if (gui_mode == GUI_MODE_SCREENSAVE) {
                gui_tick();
                screensaver_tick();
                screensaver_render();
            } else if (gui_mode == GUI_MODE_LOCKED) {
                gui_tick();
                lockscreen_render();
            }
        }

        /* ── Network ───────────────────────────────────────────────────────── */
        net_poll();

        /* ── Mouse ─────────────────────────────────────────────────────────── */
        int mx = mouse_get_x(), my = mouse_get_y(), mb = mouse_get_buttons();

        if (gui_mode == GUI_MODE_DESKTOP) {
            int consumed = win_handle_mouse(mx, my, mb);

            /* right-click → desktop context menu */
            if (!consumed && (mb & 2) && !(last_btn & 2)) {
                desktopctx_open(mx, my);
                consumed = 1;
            }

            /* left-click → close ctx menu or normal desktop click */
            if (!consumed && (mb & 1) && !(last_btn & 1)) {
                if (desktopctx_is_open())
                    consumed = desktopctx_click(mx, my);
                if (!consumed)
                    gui_handle_click(mx, my);
            }

            if (mb) gui_notify_activity();
        }
        last_btn = mb;

        /* ── Keyboard ──────────────────────────────────────────────────────── */
        if (!(inb(0x64) & 0x01)) continue;
        if (inb(0x64) & 0x20)    { inb(0x60); continue; }
        unsigned char sc = inb(0x60);

        if (sc == 0xE0) { ext_key = 1; continue; }

        if      (sc == 0x2A || sc == 0x36) { shift_pressed = 1; ext_key = 0; continue; }
        else if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; ext_key = 0; continue; }
        if      (sc == 0x1D) { ctrl_held = 1; ext_key = 0; continue; }
        else if (sc == 0x9D) { ctrl_held = 0; ext_key = 0; continue; }

        if (sc & 0x80) { ext_key = 0; continue; }

        gui_notify_activity();

        /* ── Lockscreen ────────────────────────────────────────────────────── */
        if (gui_mode == GUI_MODE_LOCKED) {
            char c = get_ascii_char(sc, shift_pressed);
            if (c > 0 || c == '\n' || c == '\b') {
                int r = lockscreen_input(c);
                if (r == 1) gui_switch_mode(GUI_MODE_DESKTOP);
            }
            ext_key = 0; continue;
        }

        /* ── Desktop / Screensaver keyboard ────────────────────────────────── */
        if (gui_mode == GUI_MODE_DESKTOP || gui_mode == GUI_MODE_SCREENSAVE) {

            /* extended (arrow) keys */
            if (ext_key) {
                ext_key = 0;
                if (ctrl_held) {
                    if (sc == 0x4B) { gui_start_menu_close(); win_switch_workspace(win_get_workspace()-1); gui_render(); continue; }
                    if (sc == 0x4D) { gui_start_menu_close(); win_switch_workspace(win_get_workspace()+1); gui_render(); continue; }
                }
                win_send_key(0, sc, ctrl_held); gui_render(); continue;
            }

            /* start menu search */
            if (gui_start_menu_is_open()) {
                if (sc == 0x01) { gui_start_menu_key(27); gui_render(); continue; }
                char kc = get_ascii_char(sc, shift_pressed);
                if (kc == '\b' || (kc >= ' ' && kc < 127)) {
                    gui_start_menu_key(kc); gui_render(); continue;
                }
            }

            /* ESC: close desktop context menu */
            if (sc == 0x01 && desktopctx_is_open()) {
                desktopctx_close(); gui_render(); ext_key = 0; continue;
            }

            /* F2 = Settings, F3 = Theme toggle, F11 = Lock */
            if (sc == 0x3C) { gui_start_menu_close(); app_settings_open(); gui_render(); continue; }
            if (sc == 0x3D) { gui_start_menu_close(); theme_toggle();      gui_render(); continue; }
            if (sc == 0x57) { gui_switch_mode(GUI_MODE_LOCKED); continue; }

            /* remaining keys → focused window */
            {
                char kc = get_ascii_char(sc, shift_pressed);
                if (sc == 0x01) kc = 0x1B;
                if (sc == 0x0F) kc = '\t';
                win_send_key(kc, sc, ctrl_held);
                gui_render();
            }
            continue;
        }
    }
}