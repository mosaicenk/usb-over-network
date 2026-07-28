# USB Over Network - Makefile (MinGW-w64 / GNU make)
#
# This Makefile targets MinGW-w64 with GNU make (mingw32-make).
# It is the single source of truth for building the project.
#
# MSVC is not supported via this Makefile (nmake lacks GNU make features
# like ifeq/$(addprefix)); use the MinGW toolchain instead.
#
# Targets:
#   make            build everything (CLI + GUI)
#   make cli        build CLI server + client
#   make gui        build GUI server + client (with manifest/version resources)
#   make test       build & run protocol unit tests
#   make clean      remove build output
#   make help       show this help

# ----- Toolchain -----
# GNU make defaults CC to "cc"; force gcc (MinGW) explicitly.
ifndef CC
CC := gcc
endif

BIN_DIR  := bin
COMMON   := common/error.c common/log.c common/auth.c common/network_win32.c common/protocol.c
SRV_CORE := server/usb_host_win32.c server/device_list.c server/urb_handler.c server/client_manager.c
CLI_CORE := client/discovery.c client/vhci_win32.c client/remote_device.c
GUI_CMN  := gui/gui_common.c

# Warnings are errors: the build must stay clean.
CFLAGS  := -Wall -Wextra -Werror -O2 -D_WIN32_WINNT=0x0601
RC      := windres
RCFLAGS :=

# iphlpapi: GetAdaptersAddresses for broadcast/local-IP discovery.
LIBS_NET := ws2_32 iphlpapi setupapi winusb
LIBS_GUI := $(LIBS_NET) gdi32 comctl32 shell32
LDCLI    := $(addprefix -l,$(LIBS_NET))
LDGUI    := $(addprefix -l,$(LIBS_GUI)) -mwindows

# Output binaries
SERVER_BIN     := $(BIN_DIR)/usb-server.exe
CLIENT_BIN     := $(BIN_DIR)/usb-client.exe
SERVER_GUI_BIN := $(BIN_DIR)/usb-server-gui.exe
CLIENT_GUI_BIN := $(BIN_DIR)/usb-client-gui.exe
TEST_BIN       := $(BIN_DIR)/test_protocol.exe

.PHONY: all cli gui test clean help
all: dirs cli gui

cli: dirs $(SERVER_BIN) $(CLIENT_BIN)

gui: dirs $(SERVER_GUI_BIN) $(CLIENT_GUI_BIN)

dirs:
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"

# ----- CLI builds -----
$(SERVER_BIN): $(COMMON) $(SRV_CORE) server/main.c | dirs
	$(CC) $(CFLAGS) -o $@ $^ $(LDCLI)

$(CLIENT_BIN): $(COMMON) $(CLI_CORE) client/main.c | dirs
	$(CC) $(CFLAGS) -o $@ $^ $(LDCLI)

# ----- GUI resource objects (manifest + version info) -----
$(BIN_DIR)/server_gui.o: gui/server_gui.rc gui/app.manifest gui/resource.h | dirs
	$(RC) $(RCFLAGS) -o $@ gui/server_gui.rc

$(BIN_DIR)/client_gui.o: gui/client_gui.rc gui/app.manifest gui/resource.h | dirs
	$(RC) $(RCFLAGS) -o $@ gui/client_gui.rc

# ----- GUI builds (link the compiled resource for manifest + version info) -----
$(SERVER_GUI_BIN): $(COMMON) $(SRV_CORE) $(GUI_CMN) gui/server_gui.c $(BIN_DIR)/server_gui.o | dirs
	$(CC) $(CFLAGS) -o $@ $(COMMON) $(SRV_CORE) $(GUI_CMN) gui/server_gui.c $(BIN_DIR)/server_gui.o $(LDGUI)

$(CLIENT_GUI_BIN): $(COMMON) $(CLI_CORE) $(GUI_CMN) gui/client_gui.c $(BIN_DIR)/client_gui.o | dirs
	$(CC) $(CFLAGS) -o $@ $(COMMON) $(CLI_CORE) $(GUI_CMN) gui/client_gui.c $(BIN_DIR)/client_gui.o $(LDGUI)

# ----- Tests (pure logic, no USB/network) -----
test: dirs $(TEST_BIN)
	@echo Running protocol tests...
	@./$(TEST_BIN)

$(TEST_BIN): $(COMMON) tests/test_protocol.c | dirs
	$(CC) $(CFLAGS) -o $@ $^ $(LDCLI)

# ----- Housekeeping -----
# cmd.exe-compatible: mingw32-make defaults to cmd.exe on Windows
# when sh.exe is not on PATH.
clean:
	@if exist "$(BIN_DIR)\*.exe" del /Q "$(BIN_DIR)\*.exe"
	@if exist "$(BIN_DIR)\*.res" del /Q "$(BIN_DIR)\*.res"
	@if exist "$(BIN_DIR)\*.o"   del /Q "$(BIN_DIR)\*.o"

help:
	@echo USB Over Network - Build System (MinGW-w64)
	@echo.
	@echo Targets:
	@echo   make            build CLI + GUI
	@echo   make cli        build CLI server + client
	@echo   make gui        build GUI server + client (with manifest)
	@echo   make test       build and run protocol tests
	@echo   make clean      remove build output
	@echo.
	@echo Override the compiler with CC=... (default: gcc).

