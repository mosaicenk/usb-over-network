# USB Over Network - Makefile
# Windows-only build using MSVC

# Compiler settings
CC = cl
CFLAGS = /nologo /W4 /WX /O2 /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN
LDFLAGS = /link ws2_32.lib iphlpapi.lib setupapi.lib winusb.lib
GUI_LDFLAGS = /link ws2_32.lib iphlpapi.lib setupapi.lib winusb.lib user32.lib gdi32.lib comctl32.lib shell32.lib

# Directories
COMMON_DIR = common
SERVER_DIR = server
CLIENT_DIR = client
GUI_DIR = gui
BIN_DIR = bin

# Common source files
COMMON_SRC = \
	$(COMMON_DIR)\error.c \
	$(COMMON_DIR)\log.c \
	$(COMMON_DIR)\auth.c \
	$(COMMON_DIR)\network_win32.c \
	$(COMMON_DIR)\protocol.c

# Server source files (core without main)
SERVER_CORE = \
	$(SERVER_DIR)\usb_host_win32.c \
	$(SERVER_DIR)\device_list.c \
	$(SERVER_DIR)\urb_handler.c \
	$(SERVER_DIR)\client_manager.c

SERVER_SRC = $(SERVER_DIR)\main.c $(SERVER_CORE)

# Client source files (core without main)
CLIENT_CORE = \
	$(CLIENT_DIR)\discovery.c \
	$(CLIENT_DIR)\vhci_win32.c \
	$(CLIENT_DIR)\remote_device.c

CLIENT_SRC = $(CLIENT_DIR)\main.c $(CLIENT_CORE)

# GUI source files
GUI_COMMON = $(GUI_DIR)\gui_common.c

# Output binaries
SERVER_BIN = $(BIN_DIR)\usb-server.exe
CLIENT_BIN = $(BIN_DIR)\usb-client.exe
SERVER_GUI_BIN = $(BIN_DIR)\usb-server-gui.exe
CLIENT_GUI_BIN = $(BIN_DIR)\usb-client-gui.exe
TEST_BIN = $(BIN_DIR)\test_protocol.exe

# Default target - build everything
all: dirs cli gui

# CLI only
cli: dirs server client

# GUI only
gui: dirs server-gui client-gui

# Tests only
test: dirs $(TEST_BIN)

# Create output directory
dirs:
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)

# Build CLI server
server: dirs
	$(CC) $(CFLAGS) /Fe:$(SERVER_BIN) $(COMMON_SRC) $(SERVER_SRC) $(LDFLAGS)
	@echo Server built: $(SERVER_BIN)

# Build CLI client
client: dirs
	$(CC) $(CFLAGS) /Fe:$(CLIENT_BIN) $(COMMON_SRC) $(CLIENT_SRC) $(LDFLAGS)
	@echo Client built: $(CLIENT_BIN)

# Build GUI server
server-gui: dirs
	$(CC) $(CFLAGS) /Fe:$(SERVER_GUI_BIN) $(COMMON_SRC) $(SERVER_CORE) $(GUI_COMMON) $(GUI_DIR)\server_gui.c $(GUI_LDFLAGS)
	@echo Server GUI built: $(SERVER_GUI_BIN)

# Build GUI client
client-gui: dirs
	$(CC) $(CFLAGS) /Fe:$(CLIENT_GUI_BIN) $(COMMON_SRC) $(CLIENT_CORE) $(GUI_COMMON) $(GUI_DIR)\client_gui.c $(GUI_LDFLAGS)
	@echo Client GUI built: $(CLIENT_GUI_BIN)

# Clean build artifacts
clean:
	@if exist $(BIN_DIR)\*.exe del /Q $(BIN_DIR)\*.exe
	@if exist $(BIN_DIR)\*.res del /Q $(BIN_DIR)\*.res
	@if exist *.obj del /Q *.obj
	@echo Clean complete

# Build protocol unit tests (pure logic, no USB/network)
$(TEST_BIN): dirs
	$(CC) $(CFLAGS) /Fe:$(TEST_BIN) $(COMMON_DIR)\protocol.c tests\test_protocol.c $(LDFLAGS)
	@echo Tests built: $(TEST_BIN)

# Install (copy to system path)
install: all
	@echo Install not implemented. Copy binaries manually from $(BIN_DIR)

# Help
help:
	@echo USB Over Network - Build System
	@echo.
	@echo Targets:
	@echo   all         - Build all (CLI + GUI) [default]
	@echo   cli         - Build CLI applications only
	@echo   gui         - Build GUI applications only
	@echo   test        - Build protocol unit tests
	@echo   server      - Build CLI server only
	@echo   client      - Build CLI client only
	@echo   server-gui  - Build GUI server only
	@echo   client-gui  - Build GUI client only
	@echo   clean       - Remove build artifacts
	@echo   help        - Show this help
	@echo.
	@echo Usage:
	@echo   nmake              - Build all
	@echo   nmake cli          - Build CLI apps
	@echo   nmake gui          - Build GUI apps
	@echo   nmake server-gui   - Build GUI server
	@echo   nmake clean        - Clean

.PHONY: all dirs cli gui test server client server-gui client-gui clean install help
