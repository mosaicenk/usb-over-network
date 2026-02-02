/*
 * USB Over Network - USB Host Interface
 * Windows-only implementation
 *
 * Abstract interface for USB device enumeration and access
 */

#ifndef USB_HOST_H
#define USB_HOST_H

#include "../common/types.h"
#include "../common/error.h"
#include "../common/usb_defs.h"
#include "../common/protocol.h"

/* Maximum values */
#define USB_MAX_INTERFACES      32
#define USB_MAX_ENDPOINTS       32
#define USB_MAX_STRING_LEN      256

/* Endpoint information */
typedef struct usb_endpoint_info {
    uint8_t  address;
    uint8_t  attributes;
    uint16_t max_packet_size;
    uint8_t  interval;
} usb_endpoint_info_t;

/* Interface information */
typedef struct usb_interface_info {
    uint8_t  number;
    uint8_t  alt_setting;
    uint8_t  class_code;
    uint8_t  subclass_code;
    uint8_t  protocol;
    uint8_t  num_endpoints;
    usb_endpoint_info_t endpoints[USB_MAX_ENDPOINTS];
} usb_interface_info_t;

/* USB device handle */
typedef struct usb_device {
    /* Identification */
    char path[USBIP_PATH_MAX];
    char busid[USBIP_BUSID_MAX];
    uint32_t busnum;
    uint32_t devnum;
    uint32_t speed;

    /* Device descriptor info */
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t bcd_device;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  num_configurations;

    /* Current configuration */
    uint8_t  configuration_value;
    uint8_t  num_interfaces;

    /* String descriptors */
    char manufacturer[USB_MAX_STRING_LEN];
    char product[USB_MAX_STRING_LEN];
    char serial[USB_MAX_STRING_LEN];

    /* Interfaces */
    usb_interface_info_t interfaces[USB_MAX_INTERFACES];

    /* Windows-specific handle */
    void *handle;               /* WinUSB handle or device file handle */
    void *interface_handles[USB_MAX_INTERFACES];

    /* State */
    bool is_open;
    bool is_claimed;
    bool is_virtual;            /* True if opened in virtual mode (no WinUSB) */
    int  claimed_interface;
} usb_device_t;

/* URB (USB Request Block) structure */
typedef struct usb_urb {
    /* Identification */
    uint32_t seqnum;
    uint32_t devid;

    /* Transfer info */
    uint8_t  endpoint;
    uint8_t  direction;         /* 0=OUT, 1=IN */
    uint8_t  type;              /* Control, Bulk, Interrupt, Isochronous */
    uint32_t transfer_flags;

    /* Control transfer setup packet */
    uint8_t  setup[8];

    /* Data buffer */
    uint8_t *buffer;
    uint32_t buffer_length;
    uint32_t actual_length;

    /* Status */
    int32_t  status;

    /* Isochronous specific */
    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t interval;

    /* Internal state */
    void    *internal;          /* Platform-specific data */
    bool     completed;
    bool     cancelled;
} usb_urb_t;

/* Callback for device enumeration */
typedef void (*usb_enum_callback_t)(const usb_device_t *device, void *user_data);

/* Callback for hotplug events */
typedef void (*usb_hotplug_callback_t)(const usb_device_t *device, bool attached, void *user_data);

/* ----- Initialization ----- */

/* Initialize USB host subsystem */
error_code_t usb_host_init(void);

/* Cleanup USB host subsystem */
void usb_host_cleanup(void);

/* ----- Device Enumeration ----- */

/* Enumerate all USB devices */
error_code_t usb_enumerate_devices(usb_enum_callback_t callback, void *user_data);

/* Get device count */
int usb_get_device_count(void);

/* Find device by bus ID */
error_code_t usb_find_device(const char *busid, usb_device_t *device);

/* Find device by vendor/product ID */
error_code_t usb_find_device_by_vid_pid(uint16_t vendor_id, uint16_t product_id,
                                         usb_device_t *device);

/* ----- Device Operations ----- */

/* Open device */
error_code_t usb_open_device(usb_device_t *device);

/* Close device */
void usb_close_device(usb_device_t *device);

/* Reset device */
error_code_t usb_reset_device(usb_device_t *device);

/* Get device descriptor */
error_code_t usb_get_device_descriptor(usb_device_t *device, usb_device_descriptor_t *desc);

/* Get configuration descriptor */
error_code_t usb_get_config_descriptor(usb_device_t *device, uint8_t config_index,
                                        uint8_t *buffer, size_t buffer_size, size_t *actual_size);

/* Get string descriptor */
error_code_t usb_get_string_descriptor(usb_device_t *device, uint8_t index,
                                        uint16_t lang_id, char *buffer, size_t buffer_size);

/* Set configuration */
error_code_t usb_set_configuration(usb_device_t *device, uint8_t configuration);

/* ----- Interface Operations ----- */

/* Claim interface */
error_code_t usb_claim_interface(usb_device_t *device, uint8_t interface_num);

/* Release interface */
error_code_t usb_release_interface(usb_device_t *device, uint8_t interface_num);

/* Set alternate setting */
error_code_t usb_set_interface(usb_device_t *device, uint8_t interface_num, uint8_t alt_setting);

/* ----- Transfer Operations ----- */

/* Control transfer */
error_code_t usb_control_transfer(usb_device_t *device,
                                   uint8_t request_type, uint8_t request,
                                   uint16_t value, uint16_t index,
                                   uint8_t *data, uint16_t length,
                                   uint32_t timeout_ms, uint32_t *actual_length);

/* Bulk transfer */
error_code_t usb_bulk_transfer(usb_device_t *device, uint8_t endpoint,
                                uint8_t *data, uint32_t length,
                                uint32_t timeout_ms, uint32_t *actual_length);

/* Interrupt transfer */
error_code_t usb_interrupt_transfer(usb_device_t *device, uint8_t endpoint,
                                     uint8_t *data, uint32_t length,
                                     uint32_t timeout_ms, uint32_t *actual_length);

/* ----- URB Operations ----- */

/* Submit URB (asynchronous) */
error_code_t usb_submit_urb(usb_device_t *device, usb_urb_t *urb);

/* Cancel URB */
error_code_t usb_cancel_urb(usb_device_t *device, usb_urb_t *urb);

/* Reap URB (wait for completion) */
error_code_t usb_reap_urb(usb_device_t *device, usb_urb_t **urb, uint32_t timeout_ms);

/* ----- Hotplug Support ----- */

/* Register hotplug callback */
error_code_t usb_register_hotplug(usb_hotplug_callback_t callback, void *user_data);

/* Unregister hotplug callback */
void usb_unregister_hotplug(void);

/* ----- Utility Functions ----- */

/* Convert USB device to USB/IP device structure */
void usb_device_to_usbip(const usb_device_t *device, usbip_usb_device_t *usbip_dev);

/* Get interface info as USB/IP structure */
void usb_interface_to_usbip(const usb_interface_info_t *iface, usbip_usb_interface_t *usbip_iface);

/* Print device info for debugging */
void usb_print_device_info(const usb_device_t *device);

/* Get USB speed from Windows speed value */
uint32_t usb_get_speed(uint32_t win_speed);

#endif /* USB_HOST_H */
