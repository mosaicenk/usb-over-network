/*
 * USB Over Network - USB Host Implementation (Windows)
 * Windows SetupAPI + WinUSB implementation
 */

#include "usb_host.h"
#include "../common/log.h"
#include "../common/config.h"

#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <usbiodef.h>
#include <devguid.h>
#include <cfgmgr32.h>

/* Link libraries (MSVC only) */
#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
#endif

/* GUID for USB devices - use existing from usbiodef.h if available */
#ifndef GUID_DEVINTERFACE_USB_DEVICE_DEFINED
static const GUID MY_GUID_DEVINTERFACE_USB_DEVICE =
    {0xA5DCBF10L, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
#define GUID_DEVINTERFACE_USB_DEVICE MY_GUID_DEVINTERFACE_USB_DEVICE
#endif

/* WinUSB interface GUID (generic) */
static const GUID MY_GUID_DEVINTERFACE_WINUSB =
    {0xDEE824EF, 0x729B, 0x4A0E, {0x9C, 0x14, 0xB7, 0x11, 0x7D, 0x33, 0xA8, 0x17}};

/* Global state */
static struct {
    bool initialized;
    usb_hotplug_callback_t hotplug_callback;
    void *hotplug_user_data;
} g_usb_host = {0};

/* ----- Helper Functions ----- */

static uint32_t get_usb_speed(USB_DEVICE_SPEED speed) {
    switch (speed) {
        case UsbLowSpeed:   return USB_SPEED_LOW;
        case UsbFullSpeed:  return USB_SPEED_FULL;
        case UsbHighSpeed:  return USB_SPEED_HIGH;
        case UsbSuperSpeed: return USB_SPEED_SUPER;
        default:            return USB_SPEED_UNKNOWN;
    }
}

static void parse_busid_from_path(const char *path, char *busid, size_t busid_len) {
    /* Extract bus ID from device instance path */
    /* Format example: USB\VID_1234&PID_5678\serial -> 1-1 style */
    /* For simplicity, we'll create a synthetic bus ID */

    const char *vid_pos = strstr(path, "VID_");
    const char *pid_pos = strstr(path, "PID_");

    if (vid_pos && pid_pos) {
        unsigned int vid = 0, pid = 0;
        sscanf(vid_pos, "VID_%04X", &vid);
        sscanf(pid_pos, "PID_%04X", &pid);
        /* Create a unique bus ID based on VID/PID and hash of path */
        unsigned int hash = 0;
        for (const char *p = path; *p; p++) {
            hash = hash * 31 + *p;
        }
        snprintf(busid, busid_len, "%u-%u", (hash % 8) + 1, (hash >> 8) % 8);
    } else {
        snprintf(busid, busid_len, "1-1");
    }
}

/* ----- Initialization ----- */

error_code_t usb_host_init(void) {
    if (g_usb_host.initialized) {
        return ERR_SUCCESS;
    }

    LOG_INFO("Initializing USB host subsystem (Windows)");
    g_usb_host.initialized = true;

    return ERR_SUCCESS;
}

void usb_host_cleanup(void) {
    if (!g_usb_host.initialized) {
        return;
    }

    LOG_INFO("Cleaning up USB host subsystem");
    g_usb_host.hotplug_callback = NULL;
    g_usb_host.hotplug_user_data = NULL;
    g_usb_host.initialized = false;
}

/* ----- Device Enumeration ----- */

/* Case-insensitive strstr */
static const char* stristr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return haystack;

    for (; *haystack; haystack++) {
        if (_strnicmp(haystack, needle, needle_len) == 0) {
            return haystack;
        }
    }
    return NULL;
}

/* Helper to check if hardware ID indicates a USB hub */
static bool is_usb_hub(const char *hardware_id) {
    if (strstr(hardware_id, "ROOT_HUB") != NULL) return true;
    if (strstr(hardware_id, "USB\\HUB") != NULL) return true;
    if (strstr(hardware_id, "Class_09") != NULL) return true;
    return false;
}

/* Check if this is a real external USB device worth showing */
static bool is_real_usb_device(const char *hardware_id, const char *device_desc, const char *service_name) {
    (void)hardware_id;
    (void)device_desc;

    /* USB Mass Storage - flash drives, external HDDs */
    if (service_name && _stricmp(service_name, "USBSTOR") == 0) return true;

    /* USB Printing */
    if (service_name && _stricmp(service_name, "usbprint") == 0) return true;

    /* USB Serial/CDC */
    if (service_name && _stricmp(service_name, "usbser") == 0) return true;

    /* WinUSB devices (many dongles use this) */
    if (service_name && _stricmp(service_name, "WinUSB") == 0) return true;

    /* USB Audio */
    if (service_name && _stricmp(service_name, "usbaudio") == 0) return true;

    /* USB Video (webcams) */
    if (service_name && _stricmp(service_name, "usbvideo") == 0) return true;

    /* SafeNet/Gemalto dongles (096E = SafeNet) */
    if (strstr(hardware_id, "VID_096E") != NULL) return true;

    /* HASP/Sentinel dongles (0529 = Aladdin) */
    if (strstr(hardware_id, "VID_0529") != NULL) return true;

    /* Wibu CodeMeter (064F) */
    if (strstr(hardware_id, "VID_064F") != NULL) return true;

    /* FTDI USB-Serial (0403) */
    if (strstr(hardware_id, "VID_0403") != NULL) return true;

    /* Silicon Labs USB-Serial (10C4) */
    if (strstr(hardware_id, "VID_10C4") != NULL) return true;

    /* Prolific USB-Serial (067B) */
    if (strstr(hardware_id, "VID_067B") != NULL) return true;

    return false;
}

/* Helper to check if device should be skipped */
static bool should_skip_device(const char *hardware_id, const char *device_desc, const char *service_name) {
    /* Bluetooth adapters */
    if (strstr(hardware_id, "BTHUSB") != NULL) return true;
    if (strstr(hardware_id, "BTH\\") != NULL) return true;
    if (device_desc && stristr(device_desc, "bluetooth") != NULL) return true;

    /* Intel internal (VID 8087) */
    if (strstr(hardware_id, "VID_8087") != NULL) return true;

    /* Generic USB controllers and hubs */
    if (device_desc && stristr(device_desc, "Host Controller") != NULL) return true;
    if (device_desc && stristr(device_desc, "Root Hub") != NULL) return true;

    /* HidUsb (generic HID) - skip unless it's a known dongle */
    if (service_name && _stricmp(service_name, "HidUsb") == 0) {
        /* Check if it's a known dongle VID */
        if (strstr(hardware_id, "VID_096E") != NULL) return false;  /* SafeNet */
        if (strstr(hardware_id, "VID_0529") != NULL) return false;  /* Aladdin */
        if (strstr(hardware_id, "VID_064F") != NULL) return false;  /* Wibu */
        return true;  /* Skip other generic HID */
    }

    return false;
}

/* Convert wide string to UTF-8 */
static void wide_to_utf8(const wchar_t *wide, char *utf8, size_t utf8_size) {
    if (wide == NULL || utf8 == NULL || utf8_size == 0) return;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, (int)utf8_size, NULL, NULL);
    if (len == 0) {
        utf8[0] = '\0';
    }
}

error_code_t usb_enumerate_devices(usb_enum_callback_t callback, void *user_data) {
    HDEVINFO dev_info;
    SP_DEVINFO_DATA dev_data;
    DWORD index = 0;
    int device_count = 0;

    if (!g_usb_host.initialized) {
        return ERR_NOT_INITIALIZED;
    }

    LOG_DEBUG("Enumerating USB devices");

    /* Get all present USB devices using "USB" enumerator - this finds ALL USB devices */
    dev_info = SetupDiGetClassDevsW(NULL, L"USB", NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (dev_info == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR("SetupDiGetClassDevs failed");
        return ERR_USB_INIT;
    }

    dev_data.cbSize = sizeof(SP_DEVINFO_DATA);

    while (SetupDiEnumDeviceInfo(dev_info, index, &dev_data)) {
        char hardware_id[512] = {0};
        wchar_t device_desc_w[256] = {0};
        wchar_t friendly_name_w[256] = {0};
        char instance_id[256] = {0};
        char service_name[64] = {0};

        /* Get hardware ID (ASCII is fine for VID/PID parsing) */
        SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data, SPDRP_HARDWAREID,
            NULL, (PBYTE)hardware_id, sizeof(hardware_id), NULL);

        /* Skip if no VID/PID - not a real USB device */
        if (strstr(hardware_id, "VID_") == NULL || strstr(hardware_id, "PID_") == NULL) {
            index++;
            continue;
        }

        /* Skip USB hubs */
        if (is_usb_hub(hardware_id)) {
            index++;
            continue;
        }

        /* Get service name to skip composite parents */
        SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data, SPDRP_SERVICE,
            NULL, (PBYTE)service_name, sizeof(service_name), NULL);

        /* Skip usbccgp (composite) parent devices - we want children */
        if (service_name[0] && _stricmp(service_name, "usbccgp") == 0) {
            index++;
            continue;
        }

        /* Get device description (ASCII for filtering) */
        char device_desc_ascii[256] = {0};
        SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data, SPDRP_DEVICEDESC,
            NULL, (PBYTE)device_desc_ascii, sizeof(device_desc_ascii), NULL);

        /* Skip internal/virtual devices (Bluetooth, WiFi, etc.) */
        if (should_skip_device(hardware_id, device_desc_ascii, service_name)) {
            index++;
            continue;
        }

        /* Only show real external USB devices */
        if (!is_real_usb_device(hardware_id, device_desc_ascii, service_name)) {
            index++;
            continue;
        }

        /* Get device description using Unicode API */
        if (!SetupDiGetDeviceRegistryPropertyW(dev_info, &dev_data, SPDRP_DEVICEDESC,
                NULL, (PBYTE)device_desc_w, sizeof(device_desc_w), NULL)) {
            SetupDiGetDeviceRegistryPropertyW(dev_info, &dev_data, SPDRP_FRIENDLYNAME,
                NULL, (PBYTE)device_desc_w, sizeof(device_desc_w), NULL);
        }

        /* Get friendly name using Unicode API */
        SetupDiGetDeviceRegistryPropertyW(dev_info, &dev_data, SPDRP_FRIENDLYNAME,
            NULL, (PBYTE)friendly_name_w, sizeof(friendly_name_w), NULL);

        /* Get instance ID */
        SetupDiGetDeviceInstanceIdA(dev_info, &dev_data, instance_id, sizeof(instance_id), NULL);

        {
            usb_device_t device = {0};

            /* Store instance ID as path */
            strncpy(device.path, instance_id, sizeof(device.path) - 1);

            /* Parse bus ID */
            parse_busid_from_path(instance_id, device.busid, sizeof(device.busid));

            /* Parse VID/PID from hardware ID */
            char *vid_pos = strstr(hardware_id, "VID_");
            char *pid_pos = strstr(hardware_id, "PID_");
            if (vid_pos && pid_pos) {
                sscanf(vid_pos, "VID_%04hX", &device.vendor_id);
                sscanf(pid_pos, "PID_%04hX", &device.product_id);
            }

            /* Generate bus/dev numbers from instance ID */
            unsigned int hash = 0;
            for (char *p = instance_id; *p; p++) {
                hash = hash * 31 + *p;
            }
            device.busnum = (hash % 8) + 1;
            device.devnum = ((hash >> 8) % 127) + 1;

            /* Use device description as product name - convert from Unicode to UTF-8 */
            if (friendly_name_w[0]) {
                wide_to_utf8(friendly_name_w, device.product, sizeof(device.product));
            } else if (device_desc_w[0]) {
                wide_to_utf8(device_desc_w, device.product, sizeof(device.product));
            }

            /* Default values */
            device.speed = USB_SPEED_HIGH;
            device.num_interfaces = 1;
            device.configuration_value = 1;
            device.num_configurations = 1;

            /* Call callback */
            if (callback) {
                callback(&device, user_data);
            }

            device_count++;
        }

        index++;
    }

    SetupDiDestroyDeviceInfoList(dev_info);

    LOG_INFO("Found %d USB devices", device_count);
    return ERR_SUCCESS;
}

/* Callback for counting devices */
static void count_device_callback(const usb_device_t *device, void *user_data) {
    (void)device;
    int *count = (int *)user_data;
    (*count)++;
}

int usb_get_device_count(void) {
    int count = 0;
    usb_enumerate_devices(count_device_callback, &count);
    return count;
}

/* Device find callback data */
typedef struct {
    const char *busid;
    usb_device_t *device;
    bool found;
} find_device_data_t;

static void find_device_callback(const usb_device_t *device, void *user_data) {
    find_device_data_t *data = (find_device_data_t *)user_data;
    if (!data->found && strcmp(device->busid, data->busid) == 0) {
        memcpy(data->device, device, sizeof(usb_device_t));
        data->found = true;
    }
}

error_code_t usb_find_device(const char *busid, usb_device_t *device) {
    find_device_data_t data = {
        .busid = busid,
        .device = device,
        .found = false
    };

    error_code_t err = usb_enumerate_devices(find_device_callback, &data);
    if (err != ERR_SUCCESS) {
        return err;
    }

    return data.found ? ERR_SUCCESS : ERR_USB_NOT_FOUND;
}

/* VID/PID find callback data */
typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    usb_device_t *device;
    bool found;
} find_vidpid_data_t;

static void find_vidpid_callback(const usb_device_t *device, void *user_data) {
    find_vidpid_data_t *data = (find_vidpid_data_t *)user_data;
    if (!data->found &&
        device->vendor_id == data->vendor_id &&
        device->product_id == data->product_id) {
        memcpy(data->device, device, sizeof(usb_device_t));
        data->found = true;
    }
}

error_code_t usb_find_device_by_vid_pid(uint16_t vendor_id, uint16_t product_id,
                                         usb_device_t *device) {
    find_vidpid_data_t data = {
        .vendor_id = vendor_id,
        .product_id = product_id,
        .device = device,
        .found = false
    };

    error_code_t err = usb_enumerate_devices(find_vidpid_callback, &data);
    if (err != ERR_SUCCESS) {
        return err;
    }

    return data.found ? ERR_SUCCESS : ERR_USB_NOT_FOUND;
}

/* ----- Device Operations ----- */

error_code_t usb_open_device(usb_device_t *device) {
    if (device == NULL || device->path[0] == '\0') {
        return ERR_INVALID_PARAM;
    }

    if (device->is_open) {
        return ERR_SUCCESS;
    }

    LOG_DEBUG("Opening USB device: %s", device->busid);

    /* Try to open device file */
    HANDLE file_handle = CreateFileA(device->path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, NULL);

    if (file_handle == INVALID_HANDLE_VALUE) {
        /* Device may not be accessible via WinUSB (e.g., mass storage, HID)
         * Mark as open in virtual mode - GUI will work but no real USB forwarding */
        LOG_WARN("Cannot open device directly (may be controlled by system driver): %s", device->busid);
        device->handle = NULL;
        device->is_open = true;
        device->is_virtual = true;  /* Mark as virtual mode */
        LOG_INFO("Device %s opened in virtual mode (VID=%04X PID=%04X)",
            device->busid, device->vendor_id, device->product_id);
        return ERR_SUCCESS;
    }

    /* Try to initialize WinUSB */
    WINUSB_INTERFACE_HANDLE winusb_handle;
    if (!WinUsb_Initialize(file_handle, &winusb_handle)) {
        LOG_WARN("WinUsb_Initialize failed - using virtual mode for: %s", device->busid);
        CloseHandle(file_handle);
        device->handle = NULL;
        device->is_open = true;
        device->is_virtual = true;
        LOG_INFO("Device %s opened in virtual mode (VID=%04X PID=%04X)",
            device->busid, device->vendor_id, device->product_id);
        return ERR_SUCCESS;
    }

    /* Store handles - full WinUSB mode */
    device->handle = file_handle;
    device->interface_handles[0] = winusb_handle;
    device->is_open = true;
    device->is_virtual = false;

    LOG_INFO("Opened USB device: %s (VID=%04X PID=%04X) - WinUSB mode",
        device->busid, device->vendor_id, device->product_id);

    return ERR_SUCCESS;
}

void usb_close_device(usb_device_t *device) {
    if (device == NULL || !device->is_open) {
        return;
    }

    LOG_DEBUG("Closing USB device: %s", device->busid);

    /* Release interface if claimed */
    if (device->is_claimed) {
        usb_release_interface(device, device->claimed_interface);
    }

    /* Only close handles if not in virtual mode */
    if (!device->is_virtual) {
        /* Close WinUSB handles */
        for (int i = 0; i < USB_MAX_INTERFACES; i++) {
            if (device->interface_handles[i]) {
                WinUsb_Free(device->interface_handles[i]);
                device->interface_handles[i] = NULL;
            }
        }

        /* Close file handle */
        if (device->handle) {
            CloseHandle(device->handle);
            device->handle = NULL;
        }
    }

    device->is_open = false;
    device->is_virtual = false;
    LOG_INFO("Closed USB device: %s", device->busid);
}

error_code_t usb_reset_device(usb_device_t *device) {
    if (device == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    LOG_DEBUG("Resetting USB device: %s", device->busid);

    /* WinUSB doesn't have a direct reset function */
    /* Close and reopen as workaround */
    char path[USBIP_PATH_MAX];
    strncpy(path, device->path, sizeof(path));

    usb_close_device(device);
    Sleep(100);

    return usb_open_device(device);
}

error_code_t usb_get_device_descriptor(usb_device_t *device, usb_device_descriptor_t *desc) {
    if (device == NULL || desc == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    WINUSB_INTERFACE_HANDLE winusb = device->interface_handles[0];
    ULONG transferred;

    if (!WinUsb_GetDescriptor(winusb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
            (PUCHAR)desc, sizeof(*desc), &transferred)) {
        LOG_WIN32_ERROR("Failed to get device descriptor");
        return ERR_USB_DESCRIPTOR_ERROR;
    }

    return ERR_SUCCESS;
}

error_code_t usb_get_config_descriptor(usb_device_t *device, uint8_t config_index,
                                        uint8_t *buffer, size_t buffer_size, size_t *actual_size) {
    if (device == NULL || buffer == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    WINUSB_INTERFACE_HANDLE winusb = device->interface_handles[0];
    ULONG transferred;

    if (!WinUsb_GetDescriptor(winusb, USB_CONFIGURATION_DESCRIPTOR_TYPE, config_index, 0,
            buffer, (ULONG)buffer_size, &transferred)) {
        LOG_WIN32_ERROR("Failed to get configuration descriptor");
        return ERR_USB_DESCRIPTOR_ERROR;
    }

    if (actual_size) {
        *actual_size = transferred;
    }

    return ERR_SUCCESS;
}

error_code_t usb_get_string_descriptor(usb_device_t *device, uint8_t index,
                                        uint16_t lang_id, char *buffer, size_t buffer_size) {
    if (device == NULL || buffer == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    WINUSB_INTERFACE_HANDLE winusb = device->interface_handles[0];
    uint8_t str_buffer[256];
    ULONG transferred;

    if (!WinUsb_GetDescriptor(winusb, USB_STRING_DESCRIPTOR_TYPE, index, lang_id,
            str_buffer, sizeof(str_buffer), &transferred)) {
        return ERR_USB_DESCRIPTOR_ERROR;
    }

    /* Convert Unicode to ASCII */
    USB_STRING_DESCRIPTOR *str_desc = (USB_STRING_DESCRIPTOR*)str_buffer;
    int chars = (str_desc->bLength - 2) / 2;
    int copy_chars = (chars < (int)buffer_size - 1) ? chars : (int)buffer_size - 1;

    for (int i = 0; i < copy_chars; i++) {
        buffer[i] = (char)str_desc->bString[i];
    }
    buffer[copy_chars] = '\0';

    return ERR_SUCCESS;
}

error_code_t usb_set_configuration(usb_device_t *device, uint8_t configuration) {
    /* WinUSB handles configuration automatically */
    (void)device;
    (void)configuration;
    return ERR_SUCCESS;
}

/* ----- Interface Operations ----- */

error_code_t usb_claim_interface(usb_device_t *device, uint8_t interface_num) {
    if (device == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    if (device->is_claimed) {
        if (device->claimed_interface == interface_num) {
            return ERR_SUCCESS;
        }
        return ERR_USB_BUSY;
    }

    LOG_DEBUG("Claiming interface %u on device %s", interface_num, device->busid);

    /* WinUSB claims interfaces automatically */
    device->is_claimed = true;
    device->claimed_interface = interface_num;

    return ERR_SUCCESS;
}

error_code_t usb_release_interface(usb_device_t *device, uint8_t interface_num) {
    if (device == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    if (!device->is_claimed || device->claimed_interface != interface_num) {
        return ERR_SUCCESS;
    }

    LOG_DEBUG("Releasing interface %u on device %s", interface_num, device->busid);

    device->is_claimed = false;
    device->claimed_interface = -1;

    return ERR_SUCCESS;
}

error_code_t usb_set_interface(usb_device_t *device, uint8_t interface_num, uint8_t alt_setting) {
    if (device == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    WINUSB_INTERFACE_HANDLE winusb = device->interface_handles[interface_num];
    if (winusb == NULL) {
        winusb = device->interface_handles[0];
    }

    if (!WinUsb_SetCurrentAlternateSetting(winusb, alt_setting)) {
        LOG_WIN32_ERROR("Failed to set alternate setting");
        return ERR_USB_IO_FAILED;
    }

    return ERR_SUCCESS;
}

/* ----- Transfer Operations ----- */

error_code_t usb_control_transfer(usb_device_t *device,
                                   uint8_t request_type, uint8_t request,
                                   uint16_t value, uint16_t index,
                                   uint8_t *data, uint16_t length,
                                   uint32_t timeout_ms, uint32_t *actual_length) {
    if (device == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    WINUSB_INTERFACE_HANDLE winusb = device->interface_handles[0];
    WINUSB_SETUP_PACKET setup;
    ULONG transferred = 0;

    setup.RequestType = request_type;
    setup.Request = request;
    setup.Value = value;
    setup.Index = index;
    setup.Length = length;

    /* Set timeout */
    ULONG timeout = timeout_ms;
    WinUsb_SetPipePolicy(winusb, 0, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);

    if (!WinUsb_ControlTransfer(winusb, setup, data, length, &transferred, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_SEM_TIMEOUT) {
            return ERR_TIMEOUT;
        }
        LOG_WIN32_ERROR("Control transfer failed");
        return ERR_USB_IO_FAILED;
    }

    if (actual_length) {
        *actual_length = transferred;
    }

    return ERR_SUCCESS;
}

error_code_t usb_bulk_transfer(usb_device_t *device, uint8_t endpoint,
                                uint8_t *data, uint32_t length,
                                uint32_t timeout_ms, uint32_t *actual_length) {
    if (device == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    WINUSB_INTERFACE_HANDLE winusb = device->interface_handles[0];
    ULONG transferred = 0;
    BOOL result;

    /* Set timeout */
    ULONG timeout = timeout_ms;
    WinUsb_SetPipePolicy(winusb, endpoint, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);

    if (endpoint & USB_DIR_IN) {
        result = WinUsb_ReadPipe(winusb, endpoint, data, length, &transferred, NULL);
    } else {
        result = WinUsb_WritePipe(winusb, endpoint, data, length, &transferred, NULL);
    }

    if (!result) {
        DWORD err = GetLastError();
        if (err == ERROR_SEM_TIMEOUT) {
            if (actual_length) *actual_length = transferred;
            return ERR_TIMEOUT;
        }
        LOG_WIN32_ERROR("Bulk transfer failed");
        return ERR_USB_IO_FAILED;
    }

    if (actual_length) {
        *actual_length = transferred;
    }

    return ERR_SUCCESS;
}

error_code_t usb_interrupt_transfer(usb_device_t *device, uint8_t endpoint,
                                     uint8_t *data, uint32_t length,
                                     uint32_t timeout_ms, uint32_t *actual_length) {
    /* WinUSB handles interrupt transfers same as bulk */
    return usb_bulk_transfer(device, endpoint, data, length, timeout_ms, actual_length);
}

/* ----- URB Operations ----- */

error_code_t usb_submit_urb(usb_device_t *device, usb_urb_t *urb) {
    /* For now, implement synchronous version */
    if (device == NULL || urb == NULL || !device->is_open) {
        return ERR_INVALID_PARAM;
    }

    error_code_t result;
    uint32_t actual_length = 0;

    if (urb->endpoint == 0) {
        /* Control transfer */
        usb_setup_packet_t *setup = (usb_setup_packet_t *)urb->setup;
        result = usb_control_transfer(device,
            setup->bmRequestType, setup->bRequest,
            setup->wValue, setup->wIndex,
            urb->buffer, (uint16_t)urb->buffer_length,
            URB_TIMEOUT_MS, &actual_length);
    } else if (urb->type == USB_ENDPOINT_XFER_BULK) {
        result = usb_bulk_transfer(device, urb->endpoint,
            urb->buffer, urb->buffer_length,
            URB_TIMEOUT_MS, &actual_length);
    } else {
        result = usb_interrupt_transfer(device, urb->endpoint,
            urb->buffer, urb->buffer_length,
            URB_TIMEOUT_MS, &actual_length);
    }

    urb->actual_length = actual_length;
    urb->status = (result == ERR_SUCCESS) ? 0 : -1;
    urb->completed = true;

    return result;
}

error_code_t usb_cancel_urb(usb_device_t *device, usb_urb_t *urb) {
    if (device == NULL || urb == NULL) {
        return ERR_INVALID_PARAM;
    }

    WINUSB_INTERFACE_HANDLE winusb = device->interface_handles[0];
    WinUsb_AbortPipe(winusb, urb->endpoint);

    urb->cancelled = true;
    urb->completed = true;
    urb->status = -ECANCELED;

    return ERR_SUCCESS;
}

error_code_t usb_reap_urb(usb_device_t *device, usb_urb_t **urb, uint32_t timeout_ms) {
    /* Not implemented for synchronous operation */
    (void)device;
    (void)urb;
    (void)timeout_ms;
    return ERR_NOT_SUPPORTED;
}

/* ----- Hotplug Support ----- */

error_code_t usb_register_hotplug(usb_hotplug_callback_t callback, void *user_data) {
    g_usb_host.hotplug_callback = callback;
    g_usb_host.hotplug_user_data = user_data;
    /* Note: Full hotplug support requires RegisterDeviceNotification */
    LOG_INFO("Hotplug callback registered");
    return ERR_SUCCESS;
}

void usb_unregister_hotplug(void) {
    g_usb_host.hotplug_callback = NULL;
    g_usb_host.hotplug_user_data = NULL;
}

/* ----- Utility Functions ----- */

void usb_device_to_usbip(const usb_device_t *device, usbip_usb_device_t *usbip_dev) {
    memset(usbip_dev, 0, sizeof(*usbip_dev));

    strncpy(usbip_dev->path, device->path, sizeof(usbip_dev->path) - 1);
    strncpy(usbip_dev->busid, device->busid, sizeof(usbip_dev->busid) - 1);
    usbip_dev->busnum = device->busnum;
    usbip_dev->devnum = device->devnum;
    usbip_dev->speed = device->speed;
    usbip_dev->idVendor = device->vendor_id;
    usbip_dev->idProduct = device->product_id;
    usbip_dev->bcdDevice = device->bcd_device;
    usbip_dev->bDeviceClass = device->device_class;
    usbip_dev->bDeviceSubClass = device->device_subclass;
    usbip_dev->bDeviceProtocol = device->device_protocol;
    usbip_dev->bConfigurationValue = device->configuration_value;
    usbip_dev->bNumConfigurations = device->num_configurations;
    usbip_dev->bNumInterfaces = device->num_interfaces;
}

void usb_interface_to_usbip(const usb_interface_info_t *iface, usbip_usb_interface_t *usbip_iface) {
    usbip_iface->bInterfaceClass = iface->class_code;
    usbip_iface->bInterfaceSubClass = iface->subclass_code;
    usbip_iface->bInterfaceProtocol = iface->protocol;
    usbip_iface->padding = 0;
}

void usb_print_device_info(const usb_device_t *device) {
    LOG_INFO("USB Device: %s", device->busid);
    LOG_INFO("  Path: %s", device->path);
    LOG_INFO("  VID/PID: %04X:%04X", device->vendor_id, device->product_id);
    LOG_INFO("  Class: %02X/%02X/%02X", device->device_class,
        device->device_subclass, device->device_protocol);
    LOG_INFO("  Speed: %s", usb_speed_string(device->speed));
    LOG_INFO("  Manufacturer: %s", device->manufacturer);
    LOG_INFO("  Product: %s", device->product);
    LOG_INFO("  Serial: %s", device->serial);
    LOG_INFO("  Interfaces: %u", device->num_interfaces);
}

uint32_t usb_get_speed(uint32_t win_speed) {
    return get_usb_speed((USB_DEVICE_SPEED)win_speed);
}
