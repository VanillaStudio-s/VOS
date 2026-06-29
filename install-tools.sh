#!/usr/bin/env bash
# Install all build tools needed for VanillaOS in WSL (Ubuntu/Debian)
set -e

echo "[VOS] Updating package list..."
sudo apt-get update -q

echo "[VOS] Installing build tools..."
sudo apt-get install -y \
    gcc \
    nasm \
    binutils \
    grub-pc-bin \
    grub-common \
    xorriso \
    mtools

echo ""
echo "[VOS] All tools installed!"
echo "      Now run: bash build.sh"
echo "      Or from Windows: double-click build.bat"
