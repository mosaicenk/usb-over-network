Set-Location 'C:\Users\CNK\Desktop\usb2network\usb-over-network'
$env:PATH = 'C:\mingw64\bin;' + $env:PATH

Write-Host "Building server GUI..."
& gcc -Wall -Wextra -O2 -D_WIN32_WINNT=0x0601 -o bin/usb-server-gui.exe common/error.c common/log.c common/network_win32.c common/protocol.c server/usb_host_win32.c server/device_list.c server/urb_handler.c server/client_manager.c gui/gui_common.c gui/server_gui.c -lws2_32 -lsetupapi -lwinusb -lgdi32 -lcomctl32 -lshell32 -mwindows -static -static-libgcc 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "Server GUI build successful!"
} else {
    Write-Host "Server GUI build failed!"
    exit 1
}

Write-Host ""
Write-Host "Building client GUI..."
& gcc -Wall -Wextra -O2 -D_WIN32_WINNT=0x0601 -o bin/usb-client-gui.exe common/error.c common/log.c common/network_win32.c common/protocol.c client/discovery.c client/vhci_win32.c client/remote_device.c gui/gui_common.c gui/client_gui.c -lws2_32 -lsetupapi -lgdi32 -lcomctl32 -lshell32 -mwindows -static -static-libgcc 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "Client GUI build successful!"
} else {
    Write-Host "Client GUI build failed!"
    exit 1
}

Write-Host ""
Write-Host "Build complete!"
Get-ChildItem bin\*.exe | Select-Object Name, Length, LastWriteTime
