/*
 * USB Over Network - VHCI Implementation (Windows)
 * Windows-only implementation
 *
 * Note: Full VHCI support on Windows requires a kernel-mode driver.
 * This implementation provides a software-based solution that works
 * at the application level, suitable for specific device types.
 *
 * For full USB device emulation, install the USB/IP Windows driver
 * from: https://github.com/cezanne/usbip-win
 */

#include "vhci.h"
#include "../common/log.h"
#include "../common/config.h"
#include "../common/string_utils.h"
#include <windows.h>
#include <winioctl.h>
#include <string.h>

/* Windows VHCI driver device name */
#define VHCI_DEVICE_PATH    "\\\\.\\usbip_vhci"

/* usbip-win VHCI IOCTL codes.
 *   device type : FILE_DEVICE_UNKNOWN (0x22)
 *   base func   : 0x888
 *   method      : METHOD_BUFFERED
 *   access      : FILE_ANY_ACCESS
 * See cezanne/usbip-win driver headers. Plug takes {SOCKET sock; unsigned devid}. */
#define USBIP_VHCI_IOCTL_FUNC        0x888
#define IOCTL_USBIP_VHCI_PLUGIN_HARDWARE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, USBIP_VHCI_IOCTL_FUNC + 0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_USBIP_VHCI_UNPLUG_HARDWARE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, USBIP_VHCI_IOCTL_FUNC + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_USBIP_VHCI_GET_PORTS_STATUS  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, USBIP_VHCI_IOCTL_FUNC + 3, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Wire layout the usbip-win driver expects on plug-in. The driver takes
 * ownership of the socket handle; user-mode must not read/write it afterwards. */
#pragma pack(push, 1)
typedef struct ioctl_usbip_vhci_plugin {
    SOCKET sock;
    unsigned int devid;
} ioctl_usbip_vhci_plugin_t;
#pragma pack(pop)

typedef struct ioctl_usbip_vhci_unplug {
    unsigned int port;
} ioctl_usbip_vhci_unplug_t;

/* ----- Global State ----- */

static struct {
    bool driver_available;
    bool checked_driver;
} g_vhci = {0};

/* ----- Helper Functions ----- */

static bool check_vhci_driver(void) {
    if (g_vhci.checked_driver) {
        return g_vhci.driver_available;
    }

    g_vhci.checked_driver = true;

    /* Try to open VHCI device */
    HANDLE handle = CreateFileA(
        VHCI_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        g_vhci.driver_available = true;
        LOG_INFO("VHCI driver found at %s", VHCI_DEVICE_PATH);
    } else {
        g_vhci.driver_available = false;
        LOG_WARN("VHCI driver not found. Install usbip-win driver for full USB device support.");
        LOG_WARN("Download from: https://github.com/cezanne/usbip-win");
    }

    return g_vhci.driver_available;
}

/* ----- Initialization ----- */

error_code_t vhci_init(vhci_context_t *ctx) {
    if (ctx == NULL) {
        return ERR_INVALID_PARAM;
    }

    memset(ctx, 0, sizeof(vhci_context_t));

    /* Check if VHCI driver is available */
    bool driver_available = check_vhci_driver();

    /* Set max ports */
    ctx->max_ports = VHCI_MAX_PORTS;

    /* Allocate port array */
    ctx->ports = (vhci_port_info_t *)calloc(ctx->max_ports, sizeof(vhci_port_info_t));
    if (ctx->ports == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    /* Initialize port numbers */
    for (int i = 0; i < ctx->max_ports; i++) {
        ctx->ports[i].port_number = i;
        ctx->ports[i].state = VHCI_PORT_FREE;
    }

    /* Try to open driver if available */
    if (driver_available) {
        ctx->driver_handle = CreateFileA(
            VHCI_DEVICE_PATH,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (ctx->driver_handle == INVALID_HANDLE_VALUE) {
            LOG_WARN("Could not open VHCI driver");
            ctx->driver_handle = NULL;
        }
    }

    ctx->initialized = true;
    LOG_DEBUG("VHCI initialized with %d ports (driver: %s)",
        ctx->max_ports, driver_available ? "available" : "not available");

    return ERR_SUCCESS;
}

void vhci_cleanup(vhci_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return;
    }

    /* Detach all devices */
    vhci_detach_all(ctx);

    /* Close driver handle */
    if (ctx->driver_handle != NULL) {
        CloseHandle(ctx->driver_handle);
        ctx->driver_handle = NULL;
    }

    /* Free port array */
    if (ctx->ports != NULL) {
        free(ctx->ports);
        ctx->ports = NULL;
    }

    ctx->initialized = false;
    LOG_DEBUG("VHCI cleaned up");
}

bool vhci_is_available(void) {
    return check_vhci_driver();
}

/* ----- Port Management ----- */

int vhci_get_port_count(vhci_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return 0;
    }
    return ctx->max_ports;
}

int vhci_find_free_port(vhci_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return -1;
    }

    for (int i = 0; i < ctx->max_ports; i++) {
        if (ctx->ports[i].state == VHCI_PORT_FREE) {
            return i;
        }
    }

    return -1;
}

error_code_t vhci_get_port_info(vhci_context_t *ctx, int port, vhci_port_info_t *info) {
    if (ctx == NULL || info == NULL || !ctx->initialized) {
        return ERR_INVALID_PARAM;
    }

    if (port < 0 || port >= ctx->max_ports) {
        return ERR_VHCI_INVALID_PORT;
    }

    memcpy(info, &ctx->ports[port], sizeof(vhci_port_info_t));
    return ERR_SUCCESS;
}

error_code_t vhci_get_status(vhci_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return ERR_INVALID_PARAM;
    }

    /* If driver is available, query actual status */
    if (ctx->driver_handle != NULL) {
        /* TODO: Implement IOCTL to get actual driver status */
        /* For now, rely on our internal state */
    }

    return ERR_SUCCESS;
}

/* ----- Device Attachment ----- */

error_code_t vhci_attach(vhci_context_t *ctx, int port, socket_t socket,
                          const usbip_usb_device_t *device,
                          const char *server_ip, uint16_t server_port) {
    if (ctx == NULL || device == NULL || !ctx->initialized) {
        return ERR_INVALID_PARAM;
    }

    if (port < 0 || port >= ctx->max_ports) {
        return ERR_VHCI_INVALID_PORT;
    }

    if (ctx->ports[port].state != VHCI_PORT_FREE) {
        LOG_ERROR("Port %d is not free", port);
        return ERR_DEVICE_BUSY;
    }

    LOG_INFO("Attaching device %s to VHCI port %d", device->busid, port);

    /* Update port state */
    ctx->ports[port].state = VHCI_PORT_CONNECTING;
    str_copy(ctx->ports[port].busid, device->busid, sizeof(ctx->ports[port].busid));
    if (server_ip) {
        str_copy(ctx->ports[port].server_ip, server_ip, sizeof(ctx->ports[port].server_ip));
    }
    ctx->ports[port].server_port = server_port;
    ctx->ports[port].devid = (device->busnum << 16) | device->devnum;
    ctx->ports[port].speed = device->speed;

    /* Without a VHCI driver we cannot expose the device to Windows. Fail
     * loudly instead of pretending success (the previous stub logged a
     * warning and returned OK, which misled callers). */
    if (ctx->driver_handle == NULL) {
        LOG_ERROR("No VHCI driver available; cannot attach %s", device->busid);
        LOG_ERROR("Install the usbip-win driver and retry.");
        ctx->ports[port].state = VHCI_PORT_ERROR;
        return ERR_VHCI_NOT_FOUND;
    }

    /* Hand the socket and devid to the kernel-mode VHCI driver. The driver
     * owns the socket from here on; the caller must not touch it again. */
    ioctl_usbip_vhci_plugin_t plug;
    memset(&plug, 0, sizeof(plug));
    plug.sock = socket;
    plug.devid = ctx->ports[port].devid;

    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(
        ctx->driver_handle,
        IOCTL_USBIP_VHCI_PLUGIN_HARDWARE,
        &plug, sizeof(plug),
        NULL, 0,
        &bytes_returned,
        NULL);

    if (!ok) {
        DWORD err = GetLastError();
        LOG_ERROR("VHCI plug IOCTL failed: %lu", err);
        ctx->ports[port].state = VHCI_PORT_ERROR;
        return ERR_VHCI_ATTACH_FAILED;
    }

    ctx->ports[port].state = VHCI_PORT_CONNECTED;

    LOG_INFO("Device %s attached to port %d (kernel-mode forwarding)", device->busid, port);
    LOG_INFO("  VID:PID = %04X:%04X", device->idVendor, device->idProduct);
    LOG_INFO("  Speed = %s", usb_speed_string(device->speed));

    return ERR_SUCCESS;
}

error_code_t vhci_detach(vhci_context_t *ctx, int port) {
    if (ctx == NULL || !ctx->initialized) {
        return ERR_INVALID_PARAM;
    }

    if (port < 0 || port >= ctx->max_ports) {
        return ERR_VHCI_INVALID_PORT;
    }

    if (ctx->ports[port].state == VHCI_PORT_FREE) {
        return ERR_SUCCESS;  /* Already detached */
    }

    LOG_INFO("Detaching device from VHCI port %d", port);

    ctx->ports[port].state = VHCI_PORT_DISCONNECTING;

    /* Ask the kernel-mode VHCI driver to unplug the port. */
    if (ctx->driver_handle != NULL) {
        ioctl_usbip_vhci_unplug_t unplug;
        unplug.port = (unsigned int)port;

        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(
            ctx->driver_handle,
            IOCTL_USBIP_VHCI_UNPLUG_HARDWARE,
            &unplug, sizeof(unplug),
            NULL, 0,
            &bytes_returned,
            NULL);

        if (!ok) {
            DWORD err = GetLastError();
            LOG_WARN("VHCI unplug IOCTL failed on port %d: %lu", port, err);
            /* Continue clearing local state regardless. */
        }
    }

    /* Clear port info */
    ctx->ports[port].state = VHCI_PORT_FREE;
    ctx->ports[port].busid[0] = '\0';
    ctx->ports[port].server_ip[0] = '\0';
    ctx->ports[port].server_port = 0;
    ctx->ports[port].devid = 0;
    ctx->ports[port].speed = 0;

    LOG_INFO("Device detached from port %d", port);

    return ERR_SUCCESS;
}

void vhci_detach_all(vhci_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return;
    }

    for (int i = 0; i < ctx->max_ports; i++) {
        if (ctx->ports[i].state != VHCI_PORT_FREE) {
            vhci_detach(ctx, i);
        }
    }
}

/* ----- Status & Utility ----- */

const char* vhci_port_state_string(vhci_port_state_t state) {
    switch (state) {
        case VHCI_PORT_FREE:          return "Free";
        case VHCI_PORT_CONNECTING:    return "Connecting";
        case VHCI_PORT_CONNECTED:     return "Connected";
        case VHCI_PORT_DISCONNECTING: return "Disconnecting";
        case VHCI_PORT_ERROR:         return "Error";
        default:                      return "Unknown";
    }
}

void vhci_print_status(vhci_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        printf("VHCI not initialized\n");
        return;
    }

    printf("\nVHCI Status:\n");
    printf("------------\n");
    printf("Max ports: %d\n", ctx->max_ports);
    printf("Driver: %s\n", ctx->driver_handle != NULL ? "Available" : "Not available");
    printf("\nPorts:\n");

    bool any_connected = false;
    for (int i = 0; i < ctx->max_ports; i++) {
        if (ctx->ports[i].state != VHCI_PORT_FREE) {
            printf("  Port %d: %s\n", i, vhci_port_state_string(ctx->ports[i].state));
            printf("    Device: %s\n", ctx->ports[i].busid);
            printf("    Server: %s:%u\n", ctx->ports[i].server_ip, ctx->ports[i].server_port);
            any_connected = true;
        }
    }

    if (!any_connected) {
        printf("  No devices attached\n");
    }
    printf("\n");
}

bool vhci_is_port_connected(vhci_context_t *ctx, int port) {
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }

    if (port < 0 || port >= ctx->max_ports) {
        return false;
    }

    return ctx->ports[port].state == VHCI_PORT_CONNECTED;
}
