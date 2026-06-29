# =============================================================================
# VanillaOS Build System
# Usage (in WSL):
#   make          - build VOS.iso
#   make clean    - remove build/ directory
#
# Requirements: gcc nasm ld grub-mkrescue xorriso
# Install: bash install-tools.sh
# =============================================================================

CC        := gcc
CFLAGS    := -m64 -ffreestanding -fno-stack-protector -mno-red-zone \
             -mcmodel=large -nostdinc -Wall -std=c11 -Ikernel
NASM      := nasm
NASMFLAGS := -f elf64
LD        := ld
LDFLAGS   := -T kernel/linker.ld -nostdlib -z max-page-size=0x1000

KERNEL   := kernel
BUILD    := build
ISO_ROOT := $(BUILD)/iso_root

# C sources (explicit list preserves link order awareness)
C_SRCS := \
    $(KERNEL)/kernel.c \
    $(KERNEL)/mem/heap.c \
    $(KERNEL)/cpu/idt.c \
    $(KERNEL)/cpu/irq.c \
    $(KERNEL)/cpu/rtc.c \
    $(KERNEL)/cpu/timer.c \
    $(KERNEL)/drivers/screen.c \
    $(KERNEL)/drivers/keyboard.c \
    $(KERNEL)/drivers/ata.c \
    $(KERNEL)/drivers/keyboardlang/keylang_en.c \
    $(KERNEL)/drivers/keyboardlang/keylang_de.c \
    $(KERNEL)/drivers/keyboardlang/keylang_fr.c \
    $(KERNEL)/drivers/mouse.c \
    $(KERNEL)/fs/fs.c \
    $(KERNEL)/fs/fs_disk.c \
    $(KERNEL)/fs/fs_ram.c \
    $(KERNEL)/gfx/gfx.c \
    $(KERNEL)/gui/gui.c \
    $(KERNEL)/gui/desktopctx.c \
    $(KERNEL)/gui/theme.c \
    $(KERNEL)/gui/net_panel.c \
    $(KERNEL)/gui/tray.c \
    $(KERNEL)/gui/notify.c \
    $(KERNEL)/gui/window.c \
    $(KERNEL)/gui/startmenu.c \
    $(KERNEL)/gui/taskbar.c \
    $(KERNEL)/gui/lockscreen.c \
    $(KERNEL)/gui/screensaver.c \
    $(KERNEL)/apps/welcome.c \
    $(KERNEL)/apps/settings.c \
    $(KERNEL)/apps/calculator.c \
    $(KERNEL)/apps/sysmon.c \
    $(KERNEL)/apps/texteditor.c \
    $(KERNEL)/apps/terminal_app.c \
    $(KERNEL)/apps/vosdolp.c \
    $(KERNEL)/apps/design.c \
    $(KERNEL)/shell/shell.c \
    $(KERNEL)/sys/system.c \
    $(KERNEL)/sys/user.c \
    $(KERNEL)/sys/clipboard.c \
    $(KERNEL)/sys/shutdown.c \
    $(KERNEL)/net/pci.c \
    $(KERNEL)/net/rtl8139.c \
    $(KERNEL)/net/pcnet.c \
    $(KERNEL)/net/arp.c \
    $(KERNEL)/net/ip.c \
    $(KERNEL)/net/udp.c \
    $(KERNEL)/net/tcp.c \
    $(KERNEL)/net/dhcp.c \
    $(KERNEL)/net/dns.c \
    $(KERNEL)/net/net_init.c

C_OBJS   := $(patsubst $(KERNEL)/%.c, $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS := $(BUILD)/entry.o $(BUILD)/idt_asm.o

.PHONY: all clean

all: $(BUILD)/VOS.iso
	cp $(BUILD)/VOS.iso VOS.iso
	@echo ""
	@echo "  BUILD OK -> VOS.iso"
	@echo "  Now run: run.bat"

# --- Compile C ---------------------------------------------------------------
$(BUILD)/%.o: $(KERNEL)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# --- Assemble ----------------------------------------------------------------
$(BUILD)/entry.o: $(KERNEL)/entry.asm | $(BUILD)
	$(NASM) $(NASMFLAGS) $< -o $@

# entry.asm -> entry.o, idt.asm -> idt_asm.o  (avoids clash with idt.c -> idt.o)
$(BUILD)/idt_asm.o: $(KERNEL)/cpu/idt.asm | $(BUILD)
	$(NASM) $(NASMFLAGS) $< -o $@

# --- Link kernel ELF ---------------------------------------------------------
$(BUILD)/kernel.elf: $(ASM_OBJS) $(C_OBJS)
	$(LD) $(LDFLAGS) \
	    $(BUILD)/entry.o $(BUILD)/idt_asm.o \
	    $(C_OBJS) \
	    -o $@

# --- ISO layout --------------------------------------------------------------
$(ISO_ROOT)/boot/kernel.elf: $(BUILD)/kernel.elf | $(ISO_ROOT)/boot
	cp $< $@

$(ISO_ROOT)/boot/grub/grub.cfg: grub.cfg | $(ISO_ROOT)/boot/grub
	cp $< $@

# --- Build ISO ---------------------------------------------------------------
$(BUILD)/VOS.iso: $(ISO_ROOT)/boot/kernel.elf $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_ROOT) 2>/dev/null || \
	grub2-mkrescue -o $@ $(ISO_ROOT) 2>/dev/null

# --- Directories -------------------------------------------------------------
$(BUILD):
	mkdir -p $@

$(ISO_ROOT)/boot/grub: | $(ISO_ROOT)/boot
	mkdir -p $@

$(ISO_ROOT)/boot: | $(BUILD)
	mkdir -p $@

# --- Clean -------------------------------------------------------------------
clean:
	rm -rf $(BUILD)
	@echo "Cleaned."
