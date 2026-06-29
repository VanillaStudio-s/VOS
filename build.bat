@echo off
REM VanillaOS - Windows Build Script
REM Calls build.sh inside WSL (Ubuntu). Requires WSL + tools installed.
REM First-time setup: run install-tools.bat

title VanillaOS Build

echo.
echo  ================================================
echo   VanillaOS Build System
echo  ================================================
echo.

set VOSDIR=/mnt/c/Users/merda/Documents/VOS

echo [VOS] Starting WSL build...
wsl -e bash -c "cd %VOSDIR% && bash build.sh %1"

if %errorlevel% neq 0 (
    echo.
    echo  [ERROR] Build FAILED! Check errors above.
    echo  Hint: Run install-tools.bat first if tools are missing.
    echo.
    pause
    exit /b 1
)

echo.
echo  [SUCCESS] VOS.iso is ready!
echo  Now run: run.bat to start VirtualBox
echo.
pause
