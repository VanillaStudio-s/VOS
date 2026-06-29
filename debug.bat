@echo off
REM VanillaOS - Debug Launch with Serial Output
REM Opens VirtualBox and writes serial port to serial_debug.txt

title VanillaOS Debug Launcher

set VBOXMANAGE="C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
set VBOX="C:\Program Files\Oracle\VirtualBox\VirtualBox.exe"
set VM=VOS
set SERIAL_LOG=C:\Users\merda\Documents\VOS\serial_debug.txt

if not exist %VBOXMANAGE% (
    echo [ERROR] VBoxManage not found!
    pause
    exit /b 1
)

echo [VOS] Configuring serial port for debug output...
%VBOXMANAGE% modifyvm "%VM%" --uart1 0x3F8 4
%VBOXMANAGE% modifyvm "%VM%" --uartmode1 file "%SERIAL_LOG%"

echo [VOS] Starting VanillaOS in debug mode...
echo      Serial output: %SERIAL_LOG%
echo.

%VBOX% --startvm "%VM%"

echo.
echo [VOS] VirtualBox closed.
echo      Check serial output: %SERIAL_LOG%
pause
