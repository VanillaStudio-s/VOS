@echo off
REM VanillaOS - QEMU Launcher for Windows
REM Requires: QEMU installed at C:\Program Files\qemu\
REM Download: https://www.qemu.org/download/#windows

title VanillaOS QEMU

set QEMU="C:\Program Files\qemu\qemu-system-x86_64.exe"
set ISO=%~dp0VOS.iso

if not exist %QEMU% (
    echo [ERROR] QEMU not found at C:\Program Files\qemu\
    echo         Download from https://www.qemu.org/download/#windows
    pause
    exit /b 1
)

if not exist "%ISO%" (
    echo [ERROR] VOS.iso not found!
    echo         Run build.bat ^(WSL^) first.
    pause
    exit /b 1
)

echo [VOS] Starting VanillaOS in QEMU...
%QEMU% ^
    -cpu qemu64 ^
    -m 256M ^
    -cdrom "%ISO%" ^
    -boot d ^
    -vga std ^
    -serial stdio ^
    -no-reboot ^
    -no-shutdown ^
    -device rtl8139,netdev=net0 ^
    -netdev user,id=net0 ^
    -name "VanillaOS"
