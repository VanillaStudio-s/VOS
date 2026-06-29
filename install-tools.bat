@echo off
REM VanillaOS - Install build tools (runs as WSL root, no password needed)
title VanillaOS Tool Installer

echo.
echo  Installing build tools in WSL...
wsl -u root -e bash -c "apt-get update -q && apt-get install -y gcc nasm binutils grub-pc-bin grub-common xorriso mtools && echo OK"

if %errorlevel% neq 0 (
    echo.
    echo  [ERROR] Install failed. Is WSL installed?
    echo  Get WSL: https://aka.ms/wsl
    echo.
) else (
    echo.
    echo  [SUCCESS] Tools installed! Now run build.bat
    echo.
)
pause
