# USB Over Network

**CTK Technologies**

USB cihazlarını ağ üzerinden paylaşmak için Windows tabanlı server-client uygulaması. Bir bilgisayara bağlı USB cihazları aynı yerel ağdaki başka bilgisayarlardan sanki yerel olarak bağlıymış gibi erişilebilir.

## Özellikler

- Saf C (C99) - Harici bağımlılık yok
- Windows 10/11 desteği
- USB/IP protokolü (TCP 3240, UDP 3241)
- GUI ve CLI arayüzü
- Çoklu client desteği
- Ağ keşif özelliği (UDP broadcast)
- Tray icon desteği

## Ekran Görüntüleri

### Server
- USB cihazlarını listeler
- Checkbox ile cihaz paylaşımı
- Bağlı client'ları gösterir

### Client
- Server IP ile bağlantı
- Otomatik sunucu keşfi (Discover)
- Checkbox ile uzak cihaz bağlama/ayırma

## Gereksinimler

### Derleme
- MinGW-w64 (GCC) veya Visual Studio 2019/2022
- Windows SDK

### Çalıştırma
- Windows 10 veya Windows 11
- Yönetici hakları (USB cihazlarına erişim için)
- Firewall'da TCP 3240 ve UDP 3241 portları açık olmalı

## Derleme

### MinGW ile (Önerilen)

```cmd
build_mingw.bat gui
```

### Visual Studio ile

```cmd
build_win32.bat
```

Çıktılar `bin/` klasöründe oluşur:

| Dosya | Açıklama |
|-------|----------|
| `usb-server-gui.exe` | Server (GUI) |
| `usb-client-gui.exe` | Client (GUI) |
| `usb-server.exe` | Server (CLI) |
| `usb-client.exe` | Client (CLI) |

## Kullanım

### GUI

1. **Server:** `usb-server-gui.exe` çalıştırın. USB cihazları otomatik listelenir. Paylaşmak istediğiniz cihazın checkbox'ını işaretleyin.
2. **Client:** `usb-client-gui.exe` çalıştırın. Server IP adresini girin ve "Connect" butonuna tıklayın. Bağlanmak istediğiniz cihazın checkbox'ını işaretleyin.

### CLI

```cmd
# Server - cihazları listele
usb-server.exe -l

# Server - paylaşımı başlat
usb-server.exe

# Client - sunucu keşfet
usb-client.exe discover

# Client - cihazları listele
usb-client.exe list 192.168.1.100

# Client - cihaz bağla
usb-client.exe attach 192.168.1.100 1-2

# Client - cihaz ayır
usb-client.exe detach 0
```

## Dizin Yapısı

```
usb-over-network/
├── common/             # Ortak modüller
│   ├── config.h        # Yapılandırma sabitleri
│   ├── types.h         # Platform tipleri
│   ├── usb_defs.h      # USB tanımları
│   ├── error.c/h       # Hata yönetimi
│   ├── log.c/h         # Log sistemi
│   ├── network.h       # Ağ arayüzü
│   ├── network_win32.c # Winsock implementasyonu
│   └── protocol.c/h    # USB/IP protokol yapıları
├── server/             # Server bileşenleri
│   ├── main.c          # Server giriş noktası
│   ├── usb_host.h      # USB host arayüzü
│   ├── usb_host_win32.c# Windows USB implementasyonu
│   ├── device_list.c/h # Cihaz listesi yönetimi
│   ├── urb_handler.c/h # URB işleme
│   └── client_manager.c/h # Client bağlantı yönetimi
├── client/             # Client bileşenleri
│   ├── main.c          # Client giriş noktası
│   ├── discovery.c/h   # Sunucu keşif
│   ├── vhci.h          # Virtual HCI arayüzü
│   ├── vhci_win32.c    # Windows VHCI implementasyonu
│   └── remote_device.c/h # Uzak cihaz yönetimi
├── gui/                # GUI bileşenleri
│   ├── gui_common.c/h  # Ortak GUI fonksiyonları
│   ├── server_gui.c/h  # Server GUI
│   ├── client_gui.c/h  # Client GUI
│   └── resource.h      # GUI sabitleri
├── build_mingw.bat     # MinGW build script
├── build_win32.bat     # MSVC build script
├── Makefile            # nmake Makefile
└── README.md
```

## Protokol

USB/IP protokolü (Linux kernel standardı ile uyumlu):

- **TCP Port:** 3240
- **UDP Port:** 3241 (keşif)
- **Byte Order:** Big-endian (network byte order)
- **Version:** 0x0111

## Lisans

Copyright (c) 2025 CTK Technologies. Tüm hakları saklıdır.
