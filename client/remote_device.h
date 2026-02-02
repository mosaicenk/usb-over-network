/*
 * USB Over Network - Remote Device Management
 * Windows-only implementation
 *
 * Management of remotely attached USB devices
 */

#ifndef REMOTE_DEVICE_H
#define REMOTE_DEVICE_H

#include "../common/types.h"
#include "../common/error.h"
#include "../common/protocol.h"
#include "vhci.h"

/* Remote device states */
typedef enum remote_device_state {
    REMOTE_DEV_STATE_DISCONNECTED = 0,
    REMOTE_DEV_STATE_CONNECTING,
    REMOTE_DEV_STATE_CONNECTED,
    REMOTE_DEV_STATE_ACTIVE,
    REMOTE_DEV_STATE_ERROR
} remote_device_state_t;

/* Remote device structure */
typedef struct remote_device {
    /* Connection info */
    char server_ip[64];
    uint16_t server_port;
    socket_t socket;

    /* Device info */
    usbip_usb_device_t device_info;
    char busid[USBIP_BUSID_MAX];

    /* VHCI attachment */
    vhci_context_t *vhci;
    int vhci_port;

    /* State */
    remote_device_state_t state;

    /* Threading */
    thread_t forward_thread;
    bool running;

    /* Statistics */
    uint64_t urbs_sent;
    uint64_t urbs_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;

    /* Sequence number */
    uint32_t seqnum;

    /* Internal */
    struct remote_device *next;
} remote_device_t;

/* Remote device list */
typedef struct remote_device_list {
    remote_device_t *head;
    int count;
    mutex_t mutex;
    bool initialized;
} remote_device_list_t;

/* ----- Initialization ----- */

/* Initialize remote device list */
error_code_t remote_device_list_init(remote_device_list_t *list);

/* Cleanup remote device list */
void remote_device_list_cleanup(remote_device_list_t *list);

/* ----- Device Operations ----- */

/* Connect to remote device */
error_code_t remote_device_connect(remote_device_list_t *list, vhci_context_t *vhci,
                                    const char *server_ip, uint16_t server_port,
                                    const char *busid, remote_device_t **device);

/* Disconnect remote device */
error_code_t remote_device_disconnect(remote_device_list_t *list, remote_device_t *device);

/* Disconnect device by VHCI port */
error_code_t remote_device_disconnect_port(remote_device_list_t *list, int port);

/* Disconnect all devices */
void remote_device_disconnect_all(remote_device_list_t *list);

/* ----- Device Lookup ----- */

/* Find device by VHCI port */
remote_device_t* remote_device_find_by_port(remote_device_list_t *list, int port);

/* Find device by bus ID */
remote_device_t* remote_device_find_by_busid(remote_device_list_t *list, const char *busid);

/* Get device count */
int remote_device_count(remote_device_list_t *list);

/* ----- URB Forwarding ----- */

/* Start URB forwarding */
error_code_t remote_device_start_forwarding(remote_device_t *device);

/* Stop URB forwarding */
void remote_device_stop_forwarding(remote_device_t *device);

/* ----- Status & Utility ----- */

/* Get device state string */
const char* remote_device_state_string(remote_device_state_t state);

/* Print device info */
void remote_device_print(const remote_device_t *device);

/* Print all devices */
void remote_device_list_print(remote_device_list_t *list);

/* ----- Server Communication ----- */

/* Request device list from server */
error_code_t remote_server_list_devices(const char *server_ip, uint16_t server_port,
                                         usbip_usb_device_t *devices, int *device_count,
                                         int max_devices);

/* Print server device list */
void remote_server_print_devices(const usbip_usb_device_t *devices, int device_count);

#endif /* REMOTE_DEVICE_H */
