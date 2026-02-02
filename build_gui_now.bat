@echo off
cd /d C:\Users\CNK\Desktop\usb2network\usb-over-network
set PATH=C:\mingw64\bin;%PATH%

echo Building server GUI...
gcc -Wall -Wextra -O2 -D_WIN32_WINNT=0x0601 -o bin/usb-server-gui.exe common/error.c common/log.c common/network_win32.c common/protocol.c server/usb_host_win32.c server/device_list.c server/urb_handler.c server/client_manager.c gui/gui_common.c gui/server_gui.c -lws2_32 -lsetupapi -lwinusb -lgdi32 -lcomctl32 -lshell32 -mwindows -static -static-libgcc
if errorlevel 1 (
    echo ERROR: Server GUI build failed!
    pause
    exit /b 1
)
echo Server GUI built successfully!
echo.
dir bin\usb-server-gui.exe
