@echo off
REM USB Over Network - Windows Build Script
REM Requires Visual Studio Build Tools or Visual Studio with C++ support

setlocal enabledelayedexpansion

echo.
echo =============================================
echo   USB Over Network - Windows Build
echo =============================================
echo.

REM Check for cl.exe (MSVC compiler)
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl.exe not found. Please run this script from
    echo        Developer Command Prompt for VS or run vcvarsall.bat first.
    echo.
    echo Example: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    exit /b 1
)

REM Configuration
set CC=cl
set RC=rc
set CFLAGS=/nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN
set LDFLAGS=/link ws2_32.lib setupapi.lib winusb.lib
set GUI_LDFLAGS=/link ws2_32.lib setupapi.lib winusb.lib user32.lib gdi32.lib comctl32.lib shell32.lib

REM Source directories
set COMMON_DIR=common
set SERVER_DIR=server
set CLIENT_DIR=client
set GUI_DIR=gui

REM Output directory
if not exist "bin" mkdir bin

REM Common source files
set COMMON_SRC=%COMMON_DIR%\error.c %COMMON_DIR%\log.c %COMMON_DIR%\network_win32.c %COMMON_DIR%\protocol.c

REM Server source files (without main.c for GUI)
set SERVER_CORE=%SERVER_DIR%\usb_host_win32.c %SERVER_DIR%\device_list.c %SERVER_DIR%\urb_handler.c %SERVER_DIR%\client_manager.c
set SERVER_SRC=%SERVER_DIR%\main.c %SERVER_CORE%

REM Client source files (without main.c for GUI)
set CLIENT_CORE=%CLIENT_DIR%\discovery.c %CLIENT_DIR%\vhci_win32.c %CLIENT_DIR%\remote_device.c
set CLIENT_SRC=%CLIENT_DIR%\main.c %CLIENT_CORE%

REM GUI source files
set GUI_COMMON=%GUI_DIR%\gui_common.c

REM Check build target
if "%1"=="gui" goto build_gui
if "%1"=="cli" goto build_cli
if "%1"=="all" goto build_all
if "%1"=="" goto build_all
echo Unknown target: %1
echo Usage: build_win32.bat [all^|cli^|gui]
exit /b 1

:build_all
call :build_cli_target
call :build_gui_target
goto done

:build_cli
call :build_cli_target
goto done

:build_gui
call :build_gui_target
goto done

:build_cli_target
echo.
echo --- Building CLI Applications ---
echo.

echo Building server (CLI)...
%CC% %CFLAGS% /Fe:bin\usb-server.exe %COMMON_SRC% %SERVER_SRC% %LDFLAGS%
if errorlevel 1 (
    echo ERROR: Server build failed!
    exit /b 1
)
echo Server built: bin\usb-server.exe

echo.
echo Building client (CLI)...
%CC% %CFLAGS% /Fe:bin\usb-client.exe %COMMON_SRC% %CLIENT_SRC% %LDFLAGS%
if errorlevel 1 (
    echo ERROR: Client build failed!
    exit /b 1
)
echo Client built: bin\usb-client.exe
exit /b 0

:build_gui_target
echo.
echo --- Building GUI Applications ---
echo.

echo Building server (GUI)...
%CC% %CFLAGS% /Fe:bin\usb-server-gui.exe %COMMON_SRC% %SERVER_CORE% %GUI_COMMON% %GUI_DIR%\server_gui.c %GUI_LDFLAGS%
if errorlevel 1 (
    echo ERROR: Server GUI build failed!
    exit /b 1
)
echo Server GUI built: bin\usb-server-gui.exe

echo.
echo Building client (GUI)...
%CC% %CFLAGS% /Fe:bin\usb-client-gui.exe %COMMON_SRC% %CLIENT_CORE% %GUI_COMMON% %GUI_DIR%\client_gui.c %GUI_LDFLAGS%
if errorlevel 1 (
    echo ERROR: Client GUI build failed!
    exit /b 1
)
echo Client GUI built: bin\usb-client-gui.exe
exit /b 0

:done
echo.
echo =============================================
echo   Build complete!
echo =============================================
echo.
echo CLI Binaries:
echo   bin\usb-server.exe      - Server (command line)
echo   bin\usb-client.exe      - Client (command line)
echo.
echo GUI Binaries:
echo   bin\usb-server-gui.exe  - Server (graphical interface)
echo   bin\usb-client-gui.exe  - Client (graphical interface)
echo.
echo Usage:
echo   CLI Server: usb-server.exe -l              (list USB devices)
echo   CLI Server: usb-server.exe                 (start server)
echo   CLI Client: usb-client.exe discover        (find servers)
echo   CLI Client: usb-client.exe list ^<ip^>       (list remote devices)
echo   CLI Client: usb-client.exe attach ^<ip^> ^<busid^>
echo.
echo   GUI: Just run usb-server-gui.exe or usb-client-gui.exe
echo.

REM Cleanup object files
del /Q *.obj 2>nul

endlocal
