# USB Over Network

**CTK Technologies**

USB cihazlarını ağ üzerinden paylaşmak için Windows tabanlı server-client
uygulaması. Bir bilgisayara bağlı USB cihazları aynı yerel ağdaki başka
bilgisayarlardan sanki yerel olarak bağlıymış gibi erişilebilir.

## Özellikler

- Saf C (C99) - Harici bağımlılık yok
- Windows 10/11 desteği
- USB/IP protokolü (TCP 3240, UDP 3241)
- GUI ve CLI arayüzü
- Çoklu client desteği
- Ağ keşif özelliği (UDP broadcast)
- Tray icon desteği
- Opsiyonel preshared-token kimlik doğrulama (LAN erişim kontrolü)

## Önemli: Client tarafı VHCI sürücüsü gerektirir

Uzak USB cihazını Windows'ta gerçek bir cihaz olarak göstermek için
**kernel-mode VHCI sürücüsü** gerekir. Bu sürücü bu depoya dahil değildir.

- Sürücüyü `usbip-win` projesinden yükleyin: <https://github.com/cezanne/usb-ip-win>
- Sürücü yüklü değilse `usb-client.exe attach` komutu `ERR_VHCI_NOT_FOUND`
  hatasıyla başarısız olur; cihaz yalnızca yazılımsal olarak takip edilir,
  işletim sistemine tanıtılmaz.
- Sürücü yüklüyse client, `usbip-win` IOCTL'leri ile cihazı gerçekten plug
  eder ve URB yönlendirme kernel-mode'da sürücü tarafından yapılır.

## Gereksinimler

### Derleme
- MinGW-w64 (GCC) veya Visual Studio 2019/2022
- Windows SDK

### Çalıştırma
- Windows 10 veya Windows 11
- Yönetici hakları (USB cihazlarına erişim için)
- Firewall'da TCP 3240 ve UDP 3241 portları açık olmalı
- Client tarafında VHCI sürücüsü (yukarıya bakın)

## Derleme

### MinGW ile (Önerilen)

```cmd
build_mingw.bat gui
```

### Visual Studio ile

```cmd
build_win32.bat
```

### Testler

```cmd
nmake -f Makefile test
bin\test_protocol.exe
```

Çıktılar `bin/` klasöründe oluşur:

| Dosya | Açıklama |
|-------|----------|
| `usb-server-gui.exe` | Server (GUI) |
| `usb-client-gui.exe` | Client (GUI) |
| `usb-server.exe` | Server (CLI) |
| `usb-client.exe` | Client (CLI) |
| `test_protocol.exe` | Protokol serileştirme testleri |

## Kullanım

### GUI

1. **Server:** `usb-server-gui.exe` çalıştırın. USB cihazları otomatik listelenir.
   Paylaşmak istediğiniz cihazın checkbox'ını işaretleyin.
2. **Client:** `usb-client-gui.exe` çalıştırın. Server IP adresini girin ve
   "Connect" butonuna tıklayın.

### CLI

```cmd
# Server - cihazları listele
usb-server.exe -l

# Server - paylaşımı başlat
usb-server.exe

# Server - auth token ile başlat
set USBIP_AUTH_TOKEN=secret123
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

### Kimlik Doğrulama

Server boş olmayan bir `USBIP_AUTH_TOKEN` ortam değişkeni (veya `-k`/`--auth-token`
parametresi) ile başlatılırsa, her clientin bağlantıdan hemen sonra aynı token'ı
göndermesi gerekir. Client aynı şekilde `USBIP_AUTH_TOKEN` ile token alır.

> **Not:** Bu iletişim şifrelenmemiştir (TLS yok). Token, güvenmediğiniz bir
> ağda dinlenerek ele geçebilir. Yalnızca güvendiğiniz LAN'larda kullanın.

## Dizin Yapısı

```
usb-over-network/
├── common/             # Ortak modüller
│   ├── config.h        # Yapılandırma sabitleri
│   ├── types.h         # Platform tipleri
│   ├── usb_defs.h      # USB tanımları
│   ├── error.c/h       # Hata yönetimi
│   ├── log.c/h         # Log sistemi
│   ├── auth.c/h        # Token kimlik doğrulama
│   ├── network.h       # Ağ arayüzü
│   ├── network_win32.c # Winsock implementasyonu
│   └── protocol.c/h    # USB/IP protokol yapıları
├── server/             # Server bileşenleri
├── client/             # Client bileşenleri
├── gui/                # GUI bileşenleri
├── tests/              # Birim testleri
├── .github/workflows/  # CI
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

MIT License - ayrıntılar için [LICENSE](LICENSE) dosyasına bakın.

Copyright (c) 2025 CTK Technologies.
