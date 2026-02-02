/*
 * USB Over Network - USB Standard Definitions
 * Windows-only implementation
 *
 * USB specification constants, descriptor structures, and definitions
 */

#ifndef USB_DEFS_H
#define USB_DEFS_H

#include "types.h"

/* USB Specification Version */
#define USB_SPEC_VERSION_1_0        0x0100
#define USB_SPEC_VERSION_1_1        0x0110
#define USB_SPEC_VERSION_2_0        0x0200
#define USB_SPEC_VERSION_3_0        0x0300
#define USB_SPEC_VERSION_3_1        0x0310

/* USB Device Speeds */
#define USB_SPEED_UNKNOWN           0
#define USB_SPEED_LOW               1   /* 1.5 Mbps */
#define USB_SPEED_FULL              2   /* 12 Mbps */
#define USB_SPEED_HIGH              3   /* 480 Mbps */
#define USB_SPEED_WIRELESS          4   /* Wireless USB */
#define USB_SPEED_SUPER             5   /* 5 Gbps */
#define USB_SPEED_SUPER_PLUS        6   /* 10 Gbps */

/* USB Directions */
#define USB_DIR_OUT                 0x00
#define USB_DIR_IN                  0x80
#define USB_DIR_MASK                0x80

/* USB Request Types */
#define USB_TYPE_STANDARD           (0x00 << 5)
#define USB_TYPE_CLASS              (0x01 << 5)
#define USB_TYPE_VENDOR             (0x02 << 5)
#define USB_TYPE_RESERVED           (0x03 << 5)
#define USB_TYPE_MASK               (0x03 << 5)

/* USB Request Recipients */
#define USB_RECIP_DEVICE            0x00
#define USB_RECIP_INTERFACE         0x01
#define USB_RECIP_ENDPOINT          0x02
#define USB_RECIP_OTHER             0x03
#define USB_RECIP_MASK              0x1F

/* Standard USB Requests (bRequest) */
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C
#define USB_REQ_SET_SEL             0x30
#define USB_REQ_SET_ISOCH_DELAY     0x31

/* Descriptor Types */
#define USB_DT_DEVICE               0x01
#define USB_DT_CONFIG               0x02
#define USB_DT_STRING               0x03
#define USB_DT_INTERFACE            0x04
#define USB_DT_ENDPOINT             0x05
#define USB_DT_DEVICE_QUALIFIER     0x06
#define USB_DT_OTHER_SPEED_CONFIG   0x07
#define USB_DT_INTERFACE_POWER      0x08
#define USB_DT_OTG                  0x09
#define USB_DT_DEBUG                0x0A
#define USB_DT_INTERFACE_ASSOC      0x0B
#define USB_DT_BOS                  0x0F
#define USB_DT_DEVICE_CAPABILITY    0x10
#define USB_DT_SS_EP_COMPANION      0x30
#define USB_DT_HID                  0x21
#define USB_DT_HID_REPORT           0x22
#define USB_DT_HID_PHYSICAL         0x23
#define USB_DT_HUB                  0x29
#define USB_DT_SS_HUB               0x2A

/* Descriptor Sizes */
#define USB_DT_DEVICE_SIZE          18
#define USB_DT_CONFIG_SIZE          9
#define USB_DT_INTERFACE_SIZE       9
#define USB_DT_ENDPOINT_SIZE        7
#define USB_DT_ENDPOINT_AUDIO_SIZE  9
#define USB_DT_HUB_SIZE             9
#define USB_DT_SS_EP_COMPANION_SIZE 6

/* Endpoint Types */
#define USB_ENDPOINT_XFER_CONTROL   0x00
#define USB_ENDPOINT_XFER_ISOC      0x01
#define USB_ENDPOINT_XFER_BULK      0x02
#define USB_ENDPOINT_XFER_INT       0x03
#define USB_ENDPOINT_XFER_MASK      0x03

/* Endpoint Synchronization Types (for isochronous) */
#define USB_ENDPOINT_SYNC_NONE      (0 << 2)
#define USB_ENDPOINT_SYNC_ASYNC     (1 << 2)
#define USB_ENDPOINT_SYNC_ADAPTIVE  (2 << 2)
#define USB_ENDPOINT_SYNC_SYNC      (3 << 2)
#define USB_ENDPOINT_SYNC_MASK      (3 << 2)

/* Endpoint Usage Types (for isochronous) */
#define USB_ENDPOINT_USAGE_DATA     (0 << 4)
#define USB_ENDPOINT_USAGE_FEEDBACK (1 << 4)
#define USB_ENDPOINT_USAGE_IMPL_FB  (2 << 4)
#define USB_ENDPOINT_USAGE_MASK     (3 << 4)

/* Endpoint Number Mask */
#define USB_ENDPOINT_NUMBER_MASK    0x0F
#define USB_ENDPOINT_MAX            16

/* USB Device Classes */
#define USB_CLASS_PER_INTERFACE     0x00
#define USB_CLASS_AUDIO             0x01
#define USB_CLASS_COMM              0x02
#define USB_CLASS_HID               0x03
#define USB_CLASS_PHYSICAL          0x05
#define USB_CLASS_IMAGE             0x06
#define USB_CLASS_PRINTER           0x07
#define USB_CLASS_MASS_STORAGE      0x08
#define USB_CLASS_HUB               0x09
#define USB_CLASS_CDC_DATA          0x0A
#define USB_CLASS_SMART_CARD        0x0B
#define USB_CLASS_CONTENT_SECURITY  0x0D
#define USB_CLASS_VIDEO             0x0E
#define USB_CLASS_PERSONAL_HEALTH   0x0F
#define USB_CLASS_AUDIO_VIDEO       0x10
#define USB_CLASS_BILLBOARD         0x11
#define USB_CLASS_USB_TYPE_C_BRIDGE 0x12
#define USB_CLASS_DIAGNOSTIC        0xDC
#define USB_CLASS_WIRELESS          0xE0
#define USB_CLASS_MISC              0xEF
#define USB_CLASS_APP_SPECIFIC      0xFE
#define USB_CLASS_VENDOR_SPECIFIC   0xFF

/* USB Mass Storage Subclasses */
#define USB_SUBCLASS_SCSI           0x06
#define USB_SUBCLASS_RBC            0x01
#define USB_SUBCLASS_UFI            0x04
#define USB_SUBCLASS_SFF8070I       0x05

/* USB Mass Storage Protocols */
#define USB_PROTOCOL_CBI            0x00
#define USB_PROTOCOL_CB             0x01
#define USB_PROTOCOL_BBB            0x50  /* Bulk-Only */
#define USB_PROTOCOL_UAS            0x62

/* USB Feature Selectors */
#define USB_FEATURE_ENDPOINT_HALT   0x00
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP 0x01
#define USB_FEATURE_TEST_MODE       0x02

/* USB Pipe Types (Windows-style) */
#define USB_PIPE_TYPE_CONTROL       0x00
#define USB_PIPE_TYPE_ISOCHRONOUS   0x01
#define USB_PIPE_TYPE_BULK          0x02
#define USB_PIPE_TYPE_INTERRUPT     0x03

/* URB Transfer Flags */
#define URB_SHORT_NOT_OK            0x0001
#define URB_ISO_ASAP                0x0002
#define URB_NO_TRANSFER_DMA_MAP     0x0004
#define URB_ZERO_PACKET             0x0040
#define URB_NO_INTERRUPT            0x0080
#define URB_FREE_BUFFER             0x0100
#define URB_DIR_IN                  0x0200
#define URB_DIR_OUT                 0x0000
#define URB_DIR_MASK                0x0200

/* ----- USB Descriptor Structures ----- */

PACKED_BEGIN
/* USB Setup Packet (8 bytes) */
typedef struct usb_setup_packet {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} PACKED_ATTR usb_setup_packet_t;
PACKED_END

PACKED_BEGIN
/* USB Device Descriptor (18 bytes) */
typedef struct usb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} PACKED_ATTR usb_device_descriptor_t;
PACKED_END

PACKED_BEGIN
/* USB Configuration Descriptor (9 bytes) */
typedef struct usb_config_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} PACKED_ATTR usb_config_descriptor_t;
PACKED_END

PACKED_BEGIN
/* USB Interface Descriptor (9 bytes) */
typedef struct usb_interface_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} PACKED_ATTR usb_interface_descriptor_t;
PACKED_END

PACKED_BEGIN
/* USB Endpoint Descriptor (7 bytes) */
typedef struct usb_endpoint_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} PACKED_ATTR usb_endpoint_descriptor_t;
PACKED_END

PACKED_BEGIN
/* USB String Descriptor Header */
typedef struct usb_string_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wString[1];  /* Variable length Unicode string */
} PACKED_ATTR usb_string_descriptor_t;
PACKED_END

/* ----- Helper Macros ----- */

/* Get endpoint number from address */
#define USB_EP_NUM(addr)        ((addr) & USB_ENDPOINT_NUMBER_MASK)

/* Get endpoint direction from address */
#define USB_EP_DIR(addr)        ((addr) & USB_DIR_MASK)

/* Check if endpoint is IN */
#define USB_EP_IS_IN(addr)      (((addr) & USB_DIR_MASK) == USB_DIR_IN)

/* Check if endpoint is OUT */
#define USB_EP_IS_OUT(addr)     (((addr) & USB_DIR_MASK) == USB_DIR_OUT)

/* Get transfer type from attributes */
#define USB_EP_TYPE(attr)       ((attr) & USB_ENDPOINT_XFER_MASK)

/* Build endpoint address */
#define USB_EP_ADDR(num, dir)   (((num) & 0x0F) | ((dir) & USB_DIR_MASK))

/* Get descriptor index from wValue */
#define USB_DESC_INDEX(wValue)  ((wValue) & 0xFF)

/* Get descriptor type from wValue */
#define USB_DESC_TYPE(wValue)   (((wValue) >> 8) & 0xFF)

/* Build wValue for GET_DESCRIPTOR */
#define USB_DESC_WVALUE(type, index)  (((type) << 8) | (index))

/* Maximum packet sizes by speed */
#define USB_MAX_PACKET_LOW      8
#define USB_MAX_PACKET_FULL     64
#define USB_MAX_PACKET_HIGH     512
#define USB_MAX_PACKET_SUPER    1024

/* USB Status Codes */
#define USB_STATUS_SUCCESS      0
#define USB_STATUS_PENDING      -1
#define USB_STATUS_ERROR        -2
#define USB_STATUS_STALL        -3
#define USB_STATUS_TIMEOUT      -4
#define USB_STATUS_CANCELLED    -5
#define USB_STATUS_NO_DEVICE    -6

/* Get USB speed name */
static INLINE const char* usb_speed_string(int speed) {
    switch (speed) {
        case USB_SPEED_LOW:         return "Low (1.5 Mbps)";
        case USB_SPEED_FULL:        return "Full (12 Mbps)";
        case USB_SPEED_HIGH:        return "High (480 Mbps)";
        case USB_SPEED_SUPER:       return "Super (5 Gbps)";
        case USB_SPEED_SUPER_PLUS:  return "Super+ (10 Gbps)";
        default:                    return "Unknown";
    }
}

/* Get USB class name */
static INLINE const char* usb_class_string(uint8_t class_code) {
    switch (class_code) {
        case USB_CLASS_PER_INTERFACE:   return "Per Interface";
        case USB_CLASS_AUDIO:           return "Audio";
        case USB_CLASS_COMM:            return "Communications";
        case USB_CLASS_HID:             return "HID";
        case USB_CLASS_PHYSICAL:        return "Physical";
        case USB_CLASS_IMAGE:           return "Image";
        case USB_CLASS_PRINTER:         return "Printer";
        case USB_CLASS_MASS_STORAGE:    return "Mass Storage";
        case USB_CLASS_HUB:             return "Hub";
        case USB_CLASS_CDC_DATA:        return "CDC Data";
        case USB_CLASS_VIDEO:           return "Video";
        case USB_CLASS_WIRELESS:        return "Wireless";
        case USB_CLASS_MISC:            return "Miscellaneous";
        case USB_CLASS_VENDOR_SPECIFIC: return "Vendor Specific";
        default:                        return "Unknown";
    }
}

#endif /* USB_DEFS_H */
