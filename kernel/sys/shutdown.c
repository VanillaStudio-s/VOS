#include "shutdown.h"
#include "../kernel.h"
#include "../fs/fs_disk.h"
#include "../drivers/screen.h"

/* ── ACPI S5 shutdown (works in QEMU & VirtualBox with ACPI enabled) ────────── */
static void _acpi_shutdown(void) {
    /* VirtualBox / QEMU ACPI power-off port */
    outw(0x4004, 0x3400);   /* QEMU   */
    outw(0xB004, 0x2000);   /* Bochs / older QEMU */
    /* VirtualBox fallback */
    outw(0x4004, 0x3400);
}

/* ── APM real-mode power-off (last resort) ──────────────────────────────────── */
static void _apm_shutdown(void) {
    /* APM bios call is only reachable from real mode; in 64-bit long mode
       we can only use ACPI or the debug exit port.  No-op here. */
}

/* ── Public ─────────────────────────────────────────────────────────────────── */

void sys_shutdown(void) {
    __asm__ volatile("cli");

    my_puts_color("\n[VOS] Saving filesystem to disk...\n", 0x0E);

    int r = fs_disk_save();
    if (r == 0)
        my_puts_color("[VOS] Filesystem saved OK.\n", 0x0A);
    else
        my_puts_color("[VOS] WARNING: Disk save failed (no ATA drive?).\n", 0x0C);

    my_puts_color("[VOS] Shutting down...\n", 0x0E);

    /* Try ACPI first */
    _acpi_shutdown();
    _apm_shutdown();

    /* If still running, just halt */
    my_puts_color("[VOS] Power off failed — CPU halted. Safe to close window.\n", 0x07);
    while (1) __asm__ volatile("hlt");
}

void sys_reboot(void) {
    __asm__ volatile("cli");

    my_puts_color("\n[VOS] Saving filesystem to disk...\n", 0x0E);
    int r = fs_disk_save();
    if (r == 0)
        my_puts_color("[VOS] Filesystem saved OK.\n", 0x0A);
    else
        my_puts_color("[VOS] WARNING: Disk save failed.\n", 0x0C);

    my_puts_color("[VOS] Rebooting...\n", 0x0E);

    /* Keyboard controller reboot pulse */
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);   /* pulse reset line */

    /* fallback triple-fault */
    __asm__ volatile("lidt (0); int $0");
    while (1) __asm__ volatile("hlt");
}