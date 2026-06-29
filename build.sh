#!/usr/bin/env bash
# VanillaOS build script (run from WSL)
# Usage: bash build.sh [clean]
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

if [ "$1" = "clean" ]; then
    echo "[VOS] Cleaning..."
    make clean
    exit 0
fi

# Check for required tools
MISSING=0
for tool in gcc nasm ld xorriso; do
    if ! command -v "$tool" &>/dev/null; then
        echo "[VOS] Missing: $tool"
        MISSING=1
    fi
done

if ! command -v grub-mkrescue &>/dev/null && ! command -v grub2-mkrescue &>/dev/null; then
    echo "[VOS] Missing: grub-mkrescue"
    MISSING=1
fi

if [ "$MISSING" -ne 0 ]; then
    echo ""
    echo "[VOS] Run 'bash install-tools.sh' to install missing tools."
    exit 1
fi

echo "[VOS] Building VanillaOS..."
make -j"$(nproc 2>/dev/null || echo 1)"

echo "[VOS] Done! VOS.iso is ready."
