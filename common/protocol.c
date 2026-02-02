/*
 * USB Over Network - USB/IP Protocol Implementation
 * Windows-only implementation
 */

#include "protocol.h"
#include "network.h"
#include "log.h"
#include <string.h>

/* ----- Byte Order Conversion Helpers ----- */

static INLINE void swap16(uint16_t *val) {
    *val = htobe16(*val);
}

static INLINE void swap32(uint32_t *val) {
    *val = htobe32(*val);
}

static INLINE void unswap16(uint16_t *val) {
    *val = be16toh(*val);
}

static INLINE void unswap32(uint32_t *val) {
    *val = be32toh(*val);
}

/* ----- Serialization Functions ----- */

void usbip_pack_header_basic(usbip_header_basic_t *header) {
    swap16(&header->version);
    swap16(&header->command);
    swap32(&header->status);
}

void usbip_unpack_header_basic(usbip_header_basic_t *header) {
    unswap16(&header->version);
    unswap16(&header->command);
    unswap32(&header->status);
}

void usbip_pack_device(usbip_usb_device_t *dev) {
    swap32(&dev->busnum);
    swap32(&dev->devnum);
    swap32(&dev->speed);
    swap16(&dev->idVendor);
    swap16(&dev->idProduct);
    swap16(&dev->bcdDevice);
}

void usbip_unpack_device(usbip_usb_device_t *dev) {
    unswap32(&dev->busnum);
    unswap32(&dev->devnum);
    unswap32(&dev->speed);
    unswap16(&dev->idVendor);
    unswap16(&dev->idProduct);
    unswap16(&dev->bcdDevice);
}

void usbip_pack_header(usbip_header_t *header, uint32_t command) {
    /* Pack common header */
    swap32(&header->common.command);
    swap32(&header->common.seqnum);
    swap32(&header->common.devid);
    swap32(&header->common.direction);
    swap32(&header->common.ep);

    /* Pack command-specific fields */
    switch (command) {
        case USBIP_CMD_SUBMIT:
            swap32(&header->u.cmd_submit.transfer_flags);
            swap32(&header->u.cmd_submit.transfer_buffer_length);
            swap32(&header->u.cmd_submit.start_frame);
            swap32(&header->u.cmd_submit.number_of_packets);
            swap32(&header->u.cmd_submit.interval);
            break;

        case USBIP_RET_SUBMIT:
            swap32(&header->u.ret_submit.status);
            swap32(&header->u.ret_submit.actual_length);
            swap32(&header->u.ret_submit.start_frame);
            swap32(&header->u.ret_submit.number_of_packets);
            swap32(&header->u.ret_submit.error_count);
            break;

        case USBIP_CMD_UNLINK:
            swap32(&header->u.cmd_unlink.seqnum_to_unlink);
            break;

        case USBIP_RET_UNLINK:
            swap32(&header->u.ret_unlink.status);
            break;
    }
}

void usbip_unpack_header(usbip_header_t *header) {
    /* Unpack common header first to get command */
    unswap32(&header->common.command);
    unswap32(&header->common.seqnum);
    unswap32(&header->common.devid);
    unswap32(&header->common.direction);
    unswap32(&header->common.ep);

    /* Unpack command-specific fields */
    switch (header->common.command) {
        case USBIP_CMD_SUBMIT:
            unswap32(&header->u.cmd_submit.transfer_flags);
            unswap32(&header->u.cmd_submit.transfer_buffer_length);
            unswap32(&header->u.cmd_submit.start_frame);
            unswap32(&header->u.cmd_submit.number_of_packets);
            unswap32(&header->u.cmd_submit.interval);
            break;

        case USBIP_RET_SUBMIT:
            unswap32(&header->u.ret_submit.status);
            unswap32(&header->u.ret_submit.actual_length);
            unswap32(&header->u.ret_submit.start_frame);
            unswap32(&header->u.ret_submit.number_of_packets);
            unswap32(&header->u.ret_submit.error_count);
            break;

        case USBIP_CMD_UNLINK:
            unswap32(&header->u.cmd_unlink.seqnum_to_unlink);
            break;

        case USBIP_RET_UNLINK:
            unswap32(&header->u.ret_unlink.status);
            break;
    }
}

void usbip_pack_iso_packet(usbip_iso_packet_descriptor_t *iso) {
    swap32(&iso->offset);
    swap32(&iso->length);
    swap32(&iso->actual_length);
    swap32(&iso->status);
}

void usbip_unpack_iso_packet(usbip_iso_packet_descriptor_t *iso) {
    unswap32(&iso->offset);
    unswap32(&iso->length);
    unswap32(&iso->actual_length);
    unswap32(&iso->status);
}

/* ----- Protocol Message Functions ----- */

void usbip_create_devlist_request(usbip_header_basic_t *header) {
    memset(header, 0, sizeof(*header));
    header->version = USBIP_PROTO_VERSION;
    header->command = OP_REQ_DEVLIST;
    header->status = 0;
}

void usbip_create_devlist_reply(usbip_op_devlist_reply_t *reply, uint32_t ndev) {
    memset(reply, 0, sizeof(*reply));
    reply->header.version = USBIP_PROTO_VERSION;
    reply->header.command = OP_REP_DEVLIST;
    reply->header.status = USBIP_ST_OK;
    reply->ndev = ndev;
}

void usbip_create_import_request(usbip_op_import_request_t *req, const char *busid) {
    memset(req, 0, sizeof(*req));
    req->header.version = USBIP_PROTO_VERSION;
    req->header.command = OP_REQ_IMPORT;
    req->header.status = 0;
    if (busid != NULL) {
        strncpy(req->busid, busid, sizeof(req->busid) - 1);
    }
}

void usbip_create_import_reply(usbip_op_import_reply_t *reply, uint32_t status,
                                const usbip_usb_device_t *device) {
    memset(reply, 0, sizeof(*reply));
    reply->header.version = USBIP_PROTO_VERSION;
    reply->header.command = OP_REP_IMPORT;
    reply->header.status = status;
    if (status == USBIP_ST_OK && device != NULL) {
        memcpy(&reply->device, device, sizeof(reply->device));
    }
}

void usbip_create_urb_submit(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                             uint32_t direction, uint32_t ep, uint32_t transfer_flags,
                             uint32_t transfer_buffer_length, const uint8_t *setup) {
    memset(header, 0, sizeof(*header));
    header->common.command = USBIP_CMD_SUBMIT;
    header->common.seqnum = seqnum;
    header->common.devid = devid;
    header->common.direction = direction;
    header->common.ep = ep;
    header->u.cmd_submit.transfer_flags = transfer_flags;
    header->u.cmd_submit.transfer_buffer_length = transfer_buffer_length;
    if (setup != NULL) {
        memcpy(header->u.cmd_submit.setup, setup, 8);
    }
}

void usbip_create_urb_return(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                             uint32_t direction, uint32_t ep, uint32_t status,
                             uint32_t actual_length) {
    memset(header, 0, sizeof(*header));
    header->common.command = USBIP_RET_SUBMIT;
    header->common.seqnum = seqnum;
    header->common.devid = devid;
    header->common.direction = direction;
    header->common.ep = ep;
    header->u.ret_submit.status = status;
    header->u.ret_submit.actual_length = actual_length;
}

void usbip_create_urb_unlink(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                             uint32_t seqnum_to_unlink) {
    memset(header, 0, sizeof(*header));
    header->common.command = USBIP_CMD_UNLINK;
    header->common.seqnum = seqnum;
    header->common.devid = devid;
    header->u.cmd_unlink.seqnum_to_unlink = seqnum_to_unlink;
}

void usbip_create_urb_unlink_ret(usbip_header_t *header, uint32_t seqnum, uint32_t devid,
                                  uint32_t status) {
    memset(header, 0, sizeof(*header));
    header->common.command = USBIP_RET_UNLINK;
    header->common.seqnum = seqnum;
    header->common.devid = devid;
    header->u.ret_unlink.status = status;
}

/* ----- Protocol I/O Functions ----- */

error_code_t usbip_send_header_basic(socket_t fd, const usbip_header_basic_t *header) {
    usbip_header_basic_t packed;
    memcpy(&packed, header, sizeof(packed));
    usbip_pack_header_basic(&packed);

    ssize_t sent = net_send_all(fd, &packed, sizeof(packed));
    if (sent != sizeof(packed)) {
        LOG_ERROR("Failed to send basic header");
        return ERR_SOCKET_SEND;
    }

    DLOG_DEBUG("Sent basic header: version=0x%04X cmd=0x%04X status=%u",
        header->version, header->command, header->status);
    return ERR_SUCCESS;
}

error_code_t usbip_recv_header_basic(socket_t fd, usbip_header_basic_t *header) {
    ssize_t received = net_recv_all(fd, header, sizeof(*header));
    if (received != sizeof(*header)) {
        LOG_ERROR("Failed to receive basic header");
        return ERR_SOCKET_RECV;
    }

    usbip_unpack_header_basic(header);

    DLOG_DEBUG("Received basic header: version=0x%04X cmd=0x%04X status=%u",
        header->version, header->command, header->status);
    return ERR_SUCCESS;
}

error_code_t usbip_send_device(socket_t fd, const usbip_usb_device_t *device) {
    usbip_usb_device_t packed;
    memcpy(&packed, device, sizeof(packed));
    usbip_pack_device(&packed);

    ssize_t sent = net_send_all(fd, &packed, sizeof(packed));
    if (sent != sizeof(packed)) {
        return ERR_SOCKET_SEND;
    }
    return ERR_SUCCESS;
}

error_code_t usbip_recv_device(socket_t fd, usbip_usb_device_t *device) {
    ssize_t received = net_recv_all(fd, device, sizeof(*device));
    if (received != sizeof(*device)) {
        return ERR_SOCKET_RECV;
    }

    usbip_unpack_device(device);
    return ERR_SUCCESS;
}

error_code_t usbip_send_interface(socket_t fd, const usbip_usb_interface_t *iface) {
    ssize_t sent = net_send_all(fd, iface, sizeof(*iface));
    if (sent != sizeof(*iface)) {
        return ERR_SOCKET_SEND;
    }
    return ERR_SUCCESS;
}

error_code_t usbip_recv_interface(socket_t fd, usbip_usb_interface_t *iface) {
    ssize_t received = net_recv_all(fd, iface, sizeof(*iface));
    if (received != sizeof(*iface)) {
        return ERR_SOCKET_RECV;
    }
    return ERR_SUCCESS;
}

error_code_t usbip_send_urb_header(socket_t fd, const usbip_header_t *header) {
    usbip_header_t packed;
    memcpy(&packed, header, sizeof(packed));
    usbip_pack_header(&packed, header->common.command);

    ssize_t sent = net_send_all(fd, &packed, sizeof(packed));
    if (sent != sizeof(packed)) {
        LOG_ERROR("Failed to send URB header");
        return ERR_SOCKET_SEND;
    }

    DLOG_DEBUG("Sent URB header: cmd=%s seq=%u devid=0x%08X ep=%u len=%u",
        usbip_urb_command_string(header->common.command),
        header->common.seqnum, header->common.devid, header->common.ep,
        header->common.command == USBIP_CMD_SUBMIT ?
            header->u.cmd_submit.transfer_buffer_length :
            header->u.ret_submit.actual_length);

    return ERR_SUCCESS;
}

error_code_t usbip_recv_urb_header(socket_t fd, usbip_header_t *header) {
    ssize_t received = net_recv_all(fd, header, sizeof(*header));
    if (received != sizeof(*header)) {
        LOG_ERROR("Failed to receive URB header");
        return ERR_SOCKET_RECV;
    }

    usbip_unpack_header(header);

    DLOG_DEBUG("Received URB header: cmd=%s seq=%u devid=0x%08X ep=%u",
        usbip_urb_command_string(header->common.command),
        header->common.seqnum, header->common.devid, header->common.ep);

    return ERR_SUCCESS;
}

error_code_t usbip_send_urb_data(socket_t fd, const void *data, uint32_t length) {
    if (length == 0) {
        return ERR_SUCCESS;
    }

    ssize_t sent = net_send_all(fd, data, length);
    if (sent != (ssize_t)length) {
        LOG_ERROR("Failed to send URB data: sent=%zd expected=%u", sent, length);
        return ERR_SOCKET_SEND;
    }

    DLOG_DEBUG("Sent URB data: %u bytes", length);
    return ERR_SUCCESS;
}

error_code_t usbip_recv_urb_data(socket_t fd, void *data, uint32_t length) {
    if (length == 0) {
        return ERR_SUCCESS;
    }

    ssize_t received = net_recv_all(fd, data, length);
    if (received != (ssize_t)length) {
        LOG_ERROR("Failed to receive URB data: received=%zd expected=%u", received, length);
        return ERR_SOCKET_RECV;
    }

    DLOG_DEBUG("Received URB data: %u bytes", length);
    return ERR_SUCCESS;
}

/* ----- Validation Functions ----- */

bool usbip_validate_version(uint16_t version) {
    return version == USBIP_PROTO_VERSION;
}

bool usbip_validate_command(uint16_t command) {
    switch (command) {
        case OP_REQ_DEVLIST:
        case OP_REP_DEVLIST:
        case OP_REQ_IMPORT:
        case OP_REP_IMPORT:
            return true;
        default:
            return false;
    }
}

bool usbip_validate_urb_command(uint32_t command) {
    switch (command) {
        case USBIP_CMD_SUBMIT:
        case USBIP_CMD_UNLINK:
        case USBIP_RET_SUBMIT:
        case USBIP_RET_UNLINK:
            return true;
        default:
            return false;
    }
}

/* ----- Utility Functions ----- */

const char* usbip_command_string(uint16_t command) {
    switch (command) {
        case OP_REQ_DEVLIST: return "OP_REQ_DEVLIST";
        case OP_REP_DEVLIST: return "OP_REP_DEVLIST";
        case OP_REQ_IMPORT:  return "OP_REQ_IMPORT";
        case OP_REP_IMPORT:  return "OP_REP_IMPORT";
        default:             return "UNKNOWN";
    }
}

const char* usbip_urb_command_string(uint32_t command) {
    switch (command) {
        case USBIP_CMD_SUBMIT: return "CMD_SUBMIT";
        case USBIP_CMD_UNLINK: return "CMD_UNLINK";
        case USBIP_RET_SUBMIT: return "RET_SUBMIT";
        case USBIP_RET_UNLINK: return "RET_UNLINK";
        default:               return "UNKNOWN";
    }
}

const char* usbip_status_string(uint32_t status) {
    switch (status) {
        case USBIP_ST_OK:       return "OK";
        case USBIP_ST_NA:       return "Not Available";
        case USBIP_ST_DEV_BUSY: return "Device Busy";
        case USBIP_ST_DEV_ERR:  return "Device Error";
        case USBIP_ST_NODEV:    return "No Device";
        case USBIP_ST_ERROR:    return "Error";
        default:                return "Unknown";
    }
}
