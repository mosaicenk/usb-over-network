@echo off
REM USB Over Network - MinGW Build Script

setlocal enabledelayedexpansion

echo.
echo =============================================
echo   USB Over Network - MinGW Build
echo =============================================
echo.

REM Set MinGW path
set PATH=C:\msys64\mingw64\bin;C:\mingw64\bin;%PATH%

REM Check for gcc
where gcc >nul 2>&1
if errorlevel 1 (
    echo ERROR: gcc not found in C:\mingw64\bin
    echo Please ensure MinGW is installed correctly.
    exit /b 1
)

REM Show compiler version
echo Using compiler:
gcc --version 2>&1 | findstr /n "^" | findstr "^1:"
echo.

REM Configuration
set CC=gcc
set CFLAGS=-Wall -Wextra -O2 -D_WIN32_WINNT=0x0601
set LDFLAGS=-lws2_32 -lsetupapi -lwinusb
set GUI_LDFLAGS=-lws2_32 -lsetupapi -lwinusb -lgdi32 -lcomctl32 -lshell32 -mwindows

REM Source directories
set COMMON_DIR=common
set SERVER_DIR=server
set CLIENT_DIR=client
set GUI_DIR=gui

REM Output directory
if not exist "bin" mkdir bin

REM Common source files
set COMMON_SRC=%COMMON_DIR%\error.c %COMMON_DIR%\log.c %COMMON_DIR%\network_win32.c %COMMON_DIR%\protocol.c

REM Server source files
set SERVER_CORE=%SERVER_DIR%\usb_host_win32.c %SERVER_DIR%\device_list.c %SERVER_DIR%\client_manager.c %SERVER_DIR%\urb_handler.c
set SERVER_SRC=%SERVER_DIR%\main.c %SERVER_CORE%

REM Client source files
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
echo Usage: build_mingw.bat [all^|cli^|gui]
exit /b 1

:build_all
call :build_cli_target
if errorlevel 1 exit /b 1
call :build_gui_target
if errorlevel 1 exit /b 1
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
%CC% %CFLAGS% -o bin/usb-server.exe %COMMON_SRC% %SERVER_SRC% %LDFLAGS%
if errorlevel 1 (
    echo ERROR: Server build failed!
    exit /b 1
)
echo Server built: bin\usb-server.exe

echo.
echo Building client (CLI)...
%CC% %CFLAGS% -o bin/usb-client.exe %COMMON_SRC% %CLIENT_SRC% %LDFLAGS%
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
%CC% %CFLAGS% -DUNICODE -D_UNICODE -o bin\usb-server-gui.exe %COMMON_SRC% %SERVER_CORE% %GUI_COMMON% %GUI_DIR%\server_gui.c %GUI_LDFLAGS% -static
if errorlevel 1 (
    echo ERROR: Server GUI build failed!
    exit /b 1
)
echo Server GUI built: bin\usb-server-gui.exe

echo.
echo Building client (GUI)...
%CC% %CFLAGS% -DUNICODE -D_UNICODE -o bin\usb-client-gui.exe %COMMON_SRC% %CLIENT_CORE% %GUI_COMMON% %GUI_DIR%\client_gui.c %GUI_LDFLAGS% -static
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

endlocal
