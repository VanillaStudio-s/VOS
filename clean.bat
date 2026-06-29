@echo off
REM VanillaOS - Clean build directory
title VanillaOS Clean

echo [VOS] Cleaning build directory...
wsl -e bash -c "cd /mnt/c/Users/merda/Documents/VOS && make clean"

if %errorlevel% neq 0 (
    echo [ERROR] Clean failed!
    pause
    exit /b 1
)

echo [VOS] Build directory cleaned.
pause
