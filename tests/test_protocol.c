/*
 * USB Over Network - Protocol Serialization Tests
 * Pure logic: verifies USB/IP byte-order helpers round-trip.
 */

#include "../common/types.h"
#include "../common/protocol.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); g_failures++; } } while (0)

static void test_header_basic_roundtrip(void) {
    usbip_header_basic_t h = { .version = USBIP_PROTO_VERSION,
                               .command = OP_REP_DEVLIST,
                               .status  = USBIP_ST_OK };
    usbip_header_basic_t expected = h;
    usbip_pack_header_basic(&h);
    CHECK(h.version != expected.version || h.command != expected.command, "pack did not swap bytes");
    usbip_unpack_header_basic(&h);
    CHECK(h.version == expected.version, "version mismatch");
    CHECK(h.command == expected.command, "command mismatch");
    CHECK(h.status  == expected.status,  "status mismatch");
}

static void test_device_roundtrip(void) {
    usbip_usb_device_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.busnum = 1; dev.devnum = 7;
    dev.speed = USB_SPEED_HIGH;
    dev.idVendor = 0x046D; dev.idProduct = 0xC534;
    dev.bcdDevice = 0x0110;
    strncpy(dev.path,  "1-7", sizeof(dev.path) - 1);
    strncpy(dev.busid, "1-7", sizeof(dev.busid) - 1);
    usbip_usb_device_t expected = dev;
    usbip_pack_device(&dev);
    CHECK(dev.busnum != expected.busnum, "busnum not swapped");
    CHECK(strcmp(dev.path, expected.path) == 0, "path must not be swapped");
    usbip_unpack_device(&dev);
    CHECK(dev.busnum    == expected.busnum,    "busnum mismatch");
    CHECK(dev.idVendor  == expected.idVendor,  "idVendor mismatch");
    CHECK(dev.idProduct == expected.idProduct, "idProduct mismatch");
    CHECK(dev.bcdDevice == expected.bcdDevice, "bcdDevice mismatch");
}

static void test_urb_submit_roundtrip(void) {
    usbip_header_t h;
    memset(&h, 0, sizeof(h));
    h.common.command = USBIP_CMD_SUBMIT;
    h.common.seqnum = 42;
    h.common.devid = usbip_make_devid(1, 2);
    h.common.direction = USBIP_DIR_IN;
    h.common.ep = 0x81;
    h.u.cmd_submit.transfer_buffer_length = 64;
    h.u.cmd_submit.setup[0] = 0x80;
    usbip_header_t expected = h;
    usbip_pack_header(&h, USBIP_CMD_SUBMIT);
    CHECK(h.common.seqnum != expected.common.seqnum, "seqnum not swapped");
    usbip_unpack_header(&h);
    CHECK(h.common.seqnum == expected.common.seqnum, "seqnum mismatch");
    CHECK(h.common.devid == expected.common.devid, "devid mismatch");
    CHECK(h.u.cmd_submit.transfer_buffer_length == expected.u.cmd_submit.transfer_buffer_length, "len mismatch");
    CHECK(h.u.cmd_submit.setup[0] == 0x80, "setup byte must not be swapped");
}

static void test_validation(void) {
    CHECK(usbip_validate_version(USBIP_PROTO_VERSION), "valid version rejected");
    CHECK(!usbip_validate_version(0x0001), "invalid version accepted");
    CHECK(usbip_validate_command(OP_REQ_DEVLIST), "OP_REQ_DEVLIST rejected");
    CHECK(!usbip_validate_command(0xFFFF), "unknown op accepted");
    CHECK(usbip_validate_urb_command(USBIP_CMD_SUBMIT), "CMD_SUBMIT rejected");
    CHECK(!usbip_validate_urb_command(0), "zero urb command accepted");
}

static void test_devid_helpers(void) {
    uint32_t id = usbip_make_devid(3, 11);
    CHECK(usbip_devid_busnum(id) == 3,  "busnum extract wrong");
    CHECK(usbip_devid_devnum(id) == 11, "devnum extract wrong");
    CHECK(usbip_is_dir_in(USBIP_DIR_IN),  "dir IN not detected");
    CHECK(!usbip_is_dir_in(USBIP_DIR_OUT), "dir OUT misdetected");
    CHECK(usbip_is_control_ep(0), "ep 0 not control");
    CHECK(!usbip_is_control_ep(1), "ep 1 misdetected as control");
}

static void test_status_strings(void) {
    CHECK(strcmp(usbip_status_string(USBIP_ST_OK), "OK") == 0, "OK string wrong");
    CHECK(strcmp(usbip_status_string(USBIP_ST_NODEV), "No Device") == 0, "NODEV string wrong");
    CHECK(strcmp(usbip_urb_command_string(USBIP_RET_SUBMIT), "RET_SUBMIT") == 0, "RET_SUBMIT string wrong");
}

int main(void) {
    test_header_basic_roundtrip();
    test_device_roundtrip();
    test_urb_submit_roundtrip();
    test_validation();
    test_devid_helpers();
    test_status_strings();
    if (g_failures == 0) { printf("OK: all protocol tests passed\n"); return 0; }
    printf("FAILED: %d assertion(s) failed\n", g_failures);
    return 1;
}
