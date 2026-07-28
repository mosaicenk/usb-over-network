# USB Over Network

**CTK Technologies**

A Windows server-client application for sharing USB devices over a network.
USB devices plugged into one computer can be accessed from other machines on
the same local network as if they were connected locally.

## Features

- Pure C (C99) — no external dependencies
- Windows 10/11 support
- USB/IP protocol (TCP 3240, UDP 3241)
- Both GUI and CLI interfaces
- Multi-client support
- Network discovery via UDP broadcast
- System tray icon
- Optional preshared-token authentication (LAN access control)

## Important: The client side requires a VHCI driver

To expose a remote USB device as a real device on Windows, a
**kernel-mode VHCI driver** is required. This driver is **not** included in
this repository.

- Install the driver from the `usbip-win` project: <https://github.com/cezanne/usb-ip-win>
- Without the driver, the `usb-client.exe attach` command fails with
  `ERR_VHCI_NOT_FOUND`; the device is only tracked in software and is never
  presented to the operating system.
- With the driver installed, the client actually plugs the device via the
  `usbip-win` IOCTLs, and URB forwarding happens in kernel mode.

## Requirements

### Build
- MinGW-w64 (GCC) or Visual Studio 2019/2022
- Windows SDK

### Runtime
- Windows 10 or Windows 11
- Administrator privileges (to access USB devices)
- TCP 3240 and UDP 3241 open in the firewall
- A VHCI driver on the client side (see above)

## Build

### MinGW (recommended)

```cmd
build_mingw.bat gui
```

### Visual Studio

```cmd
build_win32.bat
```

### Tests

```cmd
nmake -f Makefile test
bin\test_protocol.exe
```

Build outputs land in the `bin/` folder:

| File | Description |
|------|-------------|
| `usb-server-gui.exe` | Server (GUI) |
| `usb-client-gui.exe` | Client (GUI) |
| `usb-server.exe` | Server (CLI) |
| `usb-client.exe` | Client (CLI) |
| `test_protocol.exe` | Protocol serialization tests |

## Usage

### GUI

1. **Server:** run `usb-server-gui.exe`. USB devices are listed automatically.
   Tick the checkbox next to each device you want to share.
2. **Client:** run `usb-client-gui.exe`. Enter the server IP address and click
   "Connect".

### CLI

```cmd
# Server - list devices
usb-server.exe -l

# Server - start sharing
usb-server.exe

# Server - start with an auth token
set USBIP_AUTH_TOKEN=secret123
usb-server.exe

# Client - discover servers
usb-client.exe discover

# Client - list devices
usb-client.exe list 192.168.1.100

# Client - attach a device
usb-client.exe attach 192.168.1.100 1-2

# Client - detach a device
usb-client.exe detach 0
```

### Authentication

If the server is started with a non-empty `USBIP_AUTH_TOKEN` environment
variable (or the `-k` / `--auth-token` argument), every client must send the
same token immediately after connecting. The client obtains its token the same
way (`USBIP_AUTH_TOKEN` env var or `-k` flag).

> **Note:** traffic is **not** encrypted (no TLS). The token can be sniffed on
> an untrusted network. Only use this on LANs you trust.

## Directory Structure

```
usb-over-network/
├── common/             # Shared modules
│   ├── config.h        # Configuration constants
│   ├── types.h         # Platform types
│   ├── usb_defs.h      # USB definitions
│   ├── error.c/h       # Error handling
│   ├── log.c/h         # Logging system
│   ├── auth.c/h        # Token authentication
│   ├── network.h       # Network interface
│   ├── network_win32.c # Winsock implementation
│   └── protocol.c/h    # USB/IP protocol structures
├── server/             # Server components
├── client/             # Client components
├── gui/                # GUI components
├── tests/              # Unit tests
├── .github/workflows/  # CI
├── Makefile            # nmake Makefile
└── README.md
```

## Protocol

USB/IP protocol (compatible with the Linux kernel standard):

- **TCP port:** 3240
- **UDP port:** 3241 (discovery)
- **Byte order:** big-endian (network byte order)
- **Version:** 0x0111

## License

MIT License — see the [LICENSE](LICENSE) file for details.

Copyright (c) 2025 CTK Technologies.
