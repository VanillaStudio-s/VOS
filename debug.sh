#!/usr/bin/env bash
# VanillaOS QEMU debug launcher — starts GDB server on :1234
# Usage: bash debug.sh
# Then in another terminal: gdb build/kernel.elf
#   (gdb) target remote :1234
#   (gdb) continue
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

ISO="$DIR/VOS.iso"
if [ ! -f "$ISO" ]; then
    echo "[ERROR] VOS.iso not found. Run: bash build.sh"
    exit 1
fi

echo "[VOS] Debug mode: GDB server on localhost:1234"
echo "      In another terminal:"
echo "      gdb build/kernel.elf"
echo "      (gdb) target remote :1234"
echo "      (gdb) continue"
echo ""

qemu-system-x86_64 \
    -cpu qemu64 \
    -m 256M \
    -cdrom "$ISO" \
    -boot d \
    -vga std \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -s -S \
    -name "VanillaOS [DEBUG]"
