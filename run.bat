@echo off
REM VanillaOS - VirtualBox Launcher
REM Auto-creates VM "VOS" with VBoxVGA (required for VESA framebuffer)
REM Updates ISO on every launch

title VanillaOS Launcher

set VBOXMANAGE="C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
set VBOX="C:\Program Files\Oracle\VirtualBox\VirtualBox.exe"
set VM=VOS
set ISO=C:\Users\merda\Documents\VOS\VOS.iso

if not exist %VBOXMANAGE% (
    echo [ERROR] VBoxManage not found!
    echo         Install VirtualBox from https://virtualbox.org
    pause
    exit /b 1
)

if not exist "%ISO%" (
    echo [ERROR] VOS.iso not found!
    echo         Run build.bat first.
    pause
    exit /b 1
)

echo [VOS] Checking VM "%VM%"...
%VBOXMANAGE% showvminfo "%VM%" >nul 2>&1
if %errorlevel% neq 0 (
    echo [VOS] VM not found - creating...
    call :create_vm
)

echo [VOS] Refreshing ISO...
%VBOXMANAGE% storageattach "%VM%" --storagectl "IDE" --port 0 --device 0 ^
    --type dvddrive --medium "%ISO%" >nul 2>&1

echo [VOS] Starting VanillaOS...
%VBOX% --startvm "%VM%"
goto :eof

:create_vm
%VBOXMANAGE% createvm --name "%VM%" --ostype Other_64 --register
%VBOXMANAGE% modifyvm "%VM%" --memory 256 --vram 16
%VBOXMANAGE% modifyvm "%VM%" --graphicscontroller vboxvga
%VBOXMANAGE% modifyvm "%VM%" --boot1 dvd --boot2 none --boot3 none --boot4 none
%VBOXMANAGE% modifyvm "%VM%" --audio none
%VBOXMANAGE% modifyvm "%VM%" --nic1 nat --nictype1 Am79C973
%VBOXMANAGE% storagectl "%VM%" --name "IDE" --add ide
%VBOXMANAGE% storageattach "%VM%" --storagectl "IDE" --port 0 --device 0 ^
    --type dvddrive --medium "%ISO%"
echo [VOS] VM created. Graphics=VBoxVGA (VESA), NIC=PCnet-FAST III (Am79C973).
goto :eof
