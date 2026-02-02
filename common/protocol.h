/*
 * USB Over Network - USB/IP Protocol Definitions
 * Windows-only implementation
 *
 * USB/IP protocol structures and serialization functions
 * Based on USB/IP protocol specification (https://www.kernel.org/doc/html/latest/usb/usbip_protocol.html)
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "types.h"
#include "usb_defs.h"
#include "config.h"
#include "error.h"

/* ----- Protocol Version ----- */
#define USBIP_PROTO_VERSION     0x0111

/* ----- Operation Codes ----- */

/* Requests from client to server */
#define OP_REQ_DEVLIST          0x8005      /* Request device list */
#define OP_REQ_IMPORT           0x8003      /* Request to attach device */

/* Replies from server to client */
#define OP_REP_DEVLIST          0x0005      /* Device list response */
#define OP_REP_IMPORT           0x0003      /* Import response */

/* URB transmission commands */
#define USBIP_CMD_SUBMIT        0x00000001  /* Submit URB request */
#define USBIP_CMD_UNLINK        0x00000002  /* Cancel URB request */
#define USBIP_RET_SUBMIT        0x00000003  /* URB completion response */
#define USBIP_RET_UNLINK        0x00000004  /* Unlink response */

/* ----- Status Codes ----- */
#define USBIP_ST_OK             0x00000000
#define USBIP_ST_NA             0x00000001  /* Not available */
#define USBIP_ST_DEV_BUSY       0x00000002  /* Device busy */
#define USBIP_ST_DEV_ERR        0x00000003  /* Device error */
#define USBIP_ST_NODEV          0x00000004  /* No such device */
#define USBIP_ST_ERROR          0x00000005  /* General error */

/* ----- Direction Flags ----- */
#define USBIP_DIR_OUT           0x00000000
#define USBIP_DIR_IN            0x00000001

/* ----- Protocol Structures ----- */

/* All structures are packed and use big-endian (network) byte order */

#pragma pack(push, 1)

/* Basic protocol header (8 bytes) */
typedef struct usbip_header_basic {
    uint16_t version;           /* Protocol version (0x0111) */
    uint16_t command;           /* Operation code */
    uint32_t status;            /* Status/return code */
} usbip_header_basic_t;

/* Device info structure (312 bytes) */
typedef struct usbip_usb_device {
    char     path[256];         /* Device path (e.g., "1-1") */
    char     busid[32];         /* Bus ID (e.g., "1-1") */
    uint32_t busnum;            /* Bus number */
    uint32_t devnum;            /* Device number */
    uint32_t speed;             /* USB speed */
    uint16_t idVendor;          /* Vendor ID */
    uint16_t idProduct;         /* Product ID */
    uint16_t bcdDevice;         /* Device release number */
    uint8_t  bDeviceClass;      /* Device class */
    uint8_t  bDeviceSubClass;   /* Device subclass */
    uint8_t  bDeviceProtocol;   /* Device protocol */
    uint8_t  bConfigurationValue; /* Current configuration */
    uint8_t  bNumConfigurations;/* Number of configurations */
    uint8_t  bNumInterfaces;    /* Number of interfaces */
} usbip_usb_device_t;

/* Interface info structure (4 bytes) */
typedef struct usbip_usb_interface {
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  padding;
} usbip_usb_interface_t;

/* Device list request (follows basic header) */
/* No additional fields */

/* Device list reply header */
typedef struct usbip_op_devlist_reply {
    usbip_header_basic_t header;
    uint32_t ndev;              /* Number of devices */
    /* Followed by ndev * (usbip_usb_device + interfaces) */
} usbip_op_devlist_reply_t;

/* Import request (follows basic header) */
typedef struct usbip_op_import_request {
    usbip_header_basic_t header;
    char busid[32];             /* Bus ID of device to import */
} usbip_op_import_request_t;

/* Import reply */
typedef struct usbip_op_import_reply {
    usbip_header_basic_t header;
    usbip_usb_device_t device;  /* Device info (only if status == OK) */
} usbip_op_import_reply_t;

/* ----- URB Headers ----- */

/* Common URB header (48 bytes) */
typedef struct usbip_header_common {
    uint32_t command;           /* Command code */
    uint32_t seqnum;            /* Sequence number */
    uint32_t devid;             /* Device ID (busnum << 16 | devnum) */
    uint32_t direction;         /* 0=OUT, 1=IN */
    uint32_t ep;                /* Endpoint number */
} usbip_header_common_t;

/* URB Submit header (follows common header) */
typedef struct usbip_header_cmd_submit {
    uint32_t transfer_flags;
    uint32_t transfer_buffer_length;
    uint32_t start_frame;       /* For ISO transfers */
    uint32_t number_of_packets; /* For ISO transfers */
    uint32_t interval;          /* For interrupt/ISO */
    uint8_t  setup[8];          /* Setup packet for control transfers */
} usbip_header_cmd_submit_t;

/* URB Return header (follows common header) */
typedef struct usbip_header_ret_submit {
    uint32_t status;            /* URB status */
    uint32_t actual_length;     /* Actual transferred length */
    uint32_t start_frame;       /* For ISO transfers */
    uint32_t number_of_packets; /* For ISO transfers */
    uint32_t error_count;       /* For ISO transfers */
    uint8_t  setup[8];          /* Setup packet echo */
} usbip_header_ret_submit_t;

/* URB Unlink request */
typedef struct usbip_header_cmd_unlink {
    uint32_t seqnum_to_unlink;  /* Sequence number of URB to cancel */
    uint8_t  padding[24];       /* Padding to match submit header size */
} usbip_header_cmd_unlink_t;

/* URB Unlink response */
typedef struct usbip_header_ret_unlink {
    uint32_t status;            /* Unlink status */
    uint8_t  padding[24];       /* Padding to match submit header size */
} usbip_header_ret_unlink_t;

/* Complete URB header structure */
typedef struct usbip_header {
    usbip_header_common_t common;
    union {
        usbip_header_cmd_submit_t cmd_submit;
        usbip_header_ret_submit_t ret_submit;
        usbip_header_cmd_unlink_t cmd_unlink;
        usbip_header_ret_unlink_t ret_unlink;
    } u;
} usbip_header_t;

/* ISO packet descriptor */
typedef struct usbip_iso_packet_descriptor {
    uint32_t offset;
    uint32_t length;
    uint32_t actual_length;
    uint32_t status;
} usbip_iso_packet_descriptor_t;

#pragma pack(pop)

/* ----- Size Constants ----- */
#define USBIP_HEADER_BASIC_SIZE     sizeof(usbip_header_basic_t)
#define USBIP_USB_DEVICE_SIZE       sizeof(usbip_usb_device_t)
#define USBIP_USB_INTERFACE_SIZE    sizeof(usbip_usb_interface_t)
#define USBIP_HEADER_SIZE           sizeof(usbip_header_t)
#define USBIP_ISO_PACKET_SIZE       sizeof(usbip_iso_packet_descriptor_t)

/* ----- Serialization Functions ----- */

/* Pack basic header (host to network byte order) */
void usbip_pack_header_basic(usbip_header_basic_t *header);

/* Unpack basic header (network to host byte order) */
void usbip_unpack_header_basic(usbip_header_basic_t *header);

/* Pack device info */
void usbip_pack_device(usbip_usb_device_t *dev);

/* Unpack device info */
void usbip_unpack_device(usbip_usb_device_t *dev);

/* Pack URB header */
void usbip_pack_header(usbip_header_t *header, uint32_t command);

/* Unpack URB header */
void usbip_unpack_header(usbip_header_t *header);

/* Pack ISO packet descriptor */
void usbip_pack_iso_packet(usbip_iso_packet_descriptor_t *iso);

/* Unpack ISO packet descriptor */
void usbip_unpack_iso_packet(usbip_iso_packet_descriptor_t *iso);

/* ----- Protocol Message Functions ----- */

/* Create device list request */
void usbip_create_devlist_request(usbip_header_basic_t *header);

/* Create device list reply header */
void usbip_create_devlist_reply(usbip_op_devlist_reply_t *reply, uint32_t ndev);

/* Create import request */
void usbip_create_import_request(usbip_op_import_request_t *req, const char *busid);

/* Create import reply */
void usbip_create_import_reply(usbip_op_import_reply_t *reply, uint32_t status,
                                const usbip_usb_device_t *device);

/* Create URB submit header */
void usbip_create_urb_submit(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                             uint32_t direction, uint32_t ep, uint32_t transfer_flags,
                             uint32_t transfer_buffer_length, const uint8_t *setup);

/* Create URB return header */
void usbip_create_urb_return(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                             uint32_t direction, uint32_t ep, uint32_t status,
                             uint32_t actual_length);

/* Create URB unlink request */
void usbip_create_urb_unlink(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                             uint32_t seqnum_to_unlink);

/* Create URB unlink response */
void usbip_create_urb_unlink_ret(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                                  uint32_t status);

/* ----- Protocol I/O Functions ----- */

/* Send basic header */
error_code_t usbip_send_header_basic(socket_t fd, const usbip_header_basic_t *header);

/* Receive basic header */
error_code_t usbip_recv_header_basic(socket_t fd, usbip_header_basic_t *header);

/* Send device info */
error_code_t usbip_send_device(socket_t fd, const usbip_usb_device_t *device);

/* Receive device info */
error_code_t usbip_recv_device(socket_t fd, usbip_usb_device_t *device);

/* Send interface info */
error_code_t usbip_send_interface(socket_t fd, const usbip_usb_interface_t *iface);

/* Receive interface info */
error_code_t usbip_recv_interface(socket_t fd, usbip_usb_interface_t *iface);

/* Send URB header */
error_code_t usbip_send_urb_header(socket_t fd, const usbip_header_t *header);

/* Receive URB header */
error_code_t usbip_recv_urb_header(socket_t fd, usbip_header_t *header);

/* Send URB data */
error_code_t usbip_send_urb_data(socket_t fd, const void *data, uint32_t length);

/* Receive URB data */
error_code_t usbip_recv_urb_data(socket_t fd, void *data, uint32_t length);

/* ----- Validation Functions ----- */

/* Validate protocol version */
bool usbip_validate_version(uint16_t version);

/* Validate command code */
bool usbip_validate_command(uint16_t command);

/* Validate URB command */
bool usbip_validate_urb_command(uint32_t command);

/* ----- Utility Functions ----- */

/* Get command name string */
const char* usbip_command_string(uint16_t command);

/* Get URB command name string */
const char* usbip_urb_command_string(uint32_t command);

/* Get status string */
const char* usbip_status_string(uint32_t status);

/* Build device ID from bus/dev numbers */
static INLINE uint32_t usbip_make_devid(uint32_t busnum, uint32_t devnum) {
    return (busnum << 16) | (devnum & 0xFFFF);
}

/* Extract bus number from device ID */
static INLINE uint32_t usbip_devid_busnum(uint32_t devid) {
    return (devid >> 16) & 0xFFFF;
}

/* Extract device number from device ID */
static INLINE uint32_t usbip_devid_devnum(uint32_t devid) {
    return devid & 0xFFFF;
}

/* Check if direction is IN */
static INLINE bool usbip_is_dir_in(uint32_t direction) {
    return direction == USBIP_DIR_IN;
}

/* Check if this is a control endpoint */
static INLINE bool usbip_is_control_ep(uint32_t ep) {
    return ep == 0;
}

#endif /* PROTOCOL_H */
