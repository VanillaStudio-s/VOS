#!/usr/bin/env bash
# VanillaOS QEMU launcher (WSL / Linux)
# Usage: bash qemu.sh [build]
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

if [ "$1" = "build" ]; then
    echo "[VOS] Building first..."
    bash build.sh
fi

ISO="$DIR/VOS.iso"
if [ ! -f "$ISO" ]; then
    echo "[ERROR] VOS.iso not found. Run: bash build.sh"
    exit 1
fi

if ! command -v qemu-system-x86_64 &>/dev/null; then
    echo "[ERROR] qemu-system-x86_64 not found."
    echo "        Install: sudo apt install qemu-system-x86"
    exit 1
fi

KVM_FLAGS=""
if [ -e /dev/kvm ] && [ -r /dev/kvm ]; then
    KVM_FLAGS="-enable-kvm -cpu host"
    echo "[VOS] KVM available, using hardware acceleration."
else
    KVM_FLAGS="-cpu qemu64"
    echo "[VOS] No KVM, running in software mode."
fi

echo "[VOS] Starting VanillaOS..."
qemu-system-x86_64 \
    $KVM_FLAGS \
    -m 256M \
    -cdrom "$ISO" \
    -boot d \
    -vga std \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -name "VanillaOS"
