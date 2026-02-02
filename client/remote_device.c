/*
 * USB Over Network - Remote Device Implementation
 * Windows-only implementation
 */

#include "remote_device.h"
#include "../common/network.h"
#include "../common/log.h"
#include "../common/config.h"
#include <string.h>

/* ----- Helper Functions ----- */

static remote_device_t* create_remote_device(void) {
    remote_device_t *device = (remote_device_t *)calloc(1, sizeof(remote_device_t));
    if (device != NULL) {
        device->socket = INVALID_SOCKET_VAL;
        device->vhci_port = -1;
        device->state = REMOTE_DEV_STATE_DISCONNECTED;
        device->seqnum = 1;
    }
    return device;
}

static void free_remote_device(remote_device_t *device) {
    if (device == NULL) {
        return;
    }

    /* Stop forwarding */
    remote_device_stop_forwarding(device);

    /* Close socket */
    if (socket_is_valid(device->socket)) {
        socket_close(device->socket);
        device->socket = INVALID_SOCKET_VAL;
    }

    /* Detach from VHCI */
    if (device->vhci != NULL && device->vhci_port >= 0) {
        vhci_detach(device->vhci, device->vhci_port);
    }

    free(device);
}

/* ----- Initialization ----- */

error_code_t remote_device_list_init(remote_device_list_t *list) {
    if (list == NULL) {
        return ERR_INVALID_PARAM;
    }

    memset(list, 0, sizeof(remote_device_list_t));

    if (mutex_init(&list->mutex) != 0) {
        return ERR_MUTEX_INIT;
    }

    list->initialized = true;
    return ERR_SUCCESS;
}

void remote_device_list_cleanup(remote_device_list_t *list) {
    if (list == NULL || !list->initialized) {
        return;
    }

    remote_device_disconnect_all(list);

    mutex_destroy(&list->mutex);
    list->initialized = false;
}

/* ----- Device Operations ----- */

error_code_t remote_device_connect(remote_device_list_t *list, vhci_context_t *vhci,
                                    const char *server_ip, uint16_t server_port,
                                    const char *busid, remote_device_t **out_device) {
    if (list == NULL || server_ip == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    LOG_INFO("Connecting to device %s at %s:%u", busid, server_ip, server_port);

    /* Create device structure */
    remote_device_t *device = create_remote_device();
    if (device == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    strncpy(device->server_ip, server_ip, sizeof(device->server_ip) - 1);
    device->server_port = server_port;
    strncpy(device->busid, busid, sizeof(device->busid) - 1);
    device->vhci = vhci;
    device->state = REMOTE_DEV_STATE_CONNECTING;

    /* Connect to server */
    device->socket = tcp_client_connect_timeout(server_ip, server_port, CONNECTION_TIMEOUT_MS);
    if (!socket_is_valid(device->socket)) {
        LOG_ERROR("Failed to connect to server %s:%u", server_ip, server_port);
        free_remote_device(device);
        return ERR_SOCKET_CONNECT;
    }

    /* Send import request */
    usbip_op_import_request_t import_req;
    usbip_create_import_request(&import_req, busid);
    usbip_pack_header_basic(&import_req.header);

    if (net_send_all(device->socket, &import_req.header, sizeof(import_req.header)) != sizeof(import_req.header)) {
        LOG_ERROR("Failed to send import request");
        free_remote_device(device);
        return ERR_SOCKET_SEND;
    }

    if (net_send_all(device->socket, import_req.busid, sizeof(import_req.busid)) != sizeof(import_req.busid)) {
        LOG_ERROR("Failed to send bus ID");
        free_remote_device(device);
        return ERR_SOCKET_SEND;
    }

    /* Receive import response */
    usbip_header_basic_t reply_header;
    if (usbip_recv_header_basic(device->socket, &reply_header) != ERR_SUCCESS) {
        LOG_ERROR("Failed to receive import reply");
        free_remote_device(device);
        return ERR_SOCKET_RECV;
    }

    if (reply_header.status != USBIP_ST_OK) {
        LOG_ERROR("Import rejected: %s", usbip_status_string(reply_header.status));
        free_remote_device(device);
        return (reply_header.status == USBIP_ST_DEV_BUSY) ? ERR_DEVICE_BUSY : ERR_USB_NOT_FOUND;
    }

    /* Receive device info */
    if (usbip_recv_device(device->socket, &device->device_info) != ERR_SUCCESS) {
        LOG_ERROR("Failed to receive device info");
        free_remote_device(device);
        return ERR_SOCKET_RECV;
    }

    LOG_INFO("Import successful: %s (%04X:%04X)",
        device->device_info.busid,
        device->device_info.idVendor,
        device->device_info.idProduct);

    /* Find free VHCI port */
    if (vhci != NULL) {
        device->vhci_port = vhci_find_free_port(vhci);
        if (device->vhci_port < 0) {
            LOG_ERROR("No free VHCI ports available");
            free_remote_device(device);
            return ERR_VHCI_NO_FREE_PORT;
        }

        /* Attach to VHCI */
        error_code_t err = vhci_attach(vhci, device->vhci_port, device->socket,
                                        &device->device_info, server_ip, server_port);
        if (err != ERR_SUCCESS) {
            LOG_ERROR("Failed to attach to VHCI: %s", error_string(err));
            free_remote_device(device);
            return err;
        }
    }

    device->state = REMOTE_DEV_STATE_CONNECTED;

    /* Add to list */
    mutex_lock(&list->mutex);
    device->next = list->head;
    list->head = device;
    list->count++;
    mutex_unlock(&list->mutex);

    if (out_device != NULL) {
        *out_device = device;
    }

    LOG_INFO("Device attached to port %d", device->vhci_port);

    return ERR_SUCCESS;
}

error_code_t remote_device_disconnect(remote_device_list_t *list, remote_device_t *device) {
    if (list == NULL || device == NULL) {
        return ERR_INVALID_PARAM;
    }

    mutex_lock(&list->mutex);

    /* Find and remove from list */
    remote_device_t *prev = NULL;
    remote_device_t *curr = list->head;

    while (curr != NULL) {
        if (curr == device) {
            if (prev == NULL) {
                list->head = curr->next;
            } else {
                prev->next = curr->next;
            }
            list->count--;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    mutex_unlock(&list->mutex);

    if (curr != NULL) {
        LOG_INFO("Disconnecting device %s from port %d", device->busid, device->vhci_port);
        free_remote_device(device);
        return ERR_SUCCESS;
    }

    return ERR_NOT_FOUND;
}

error_code_t remote_device_disconnect_port(remote_device_list_t *list, int port) {
    remote_device_t *device = remote_device_find_by_port(list, port);
    if (device == NULL) {
        return ERR_NOT_FOUND;
    }
    return remote_device_disconnect(list, device);
}

void remote_device_disconnect_all(remote_device_list_t *list) {
    if (list == NULL || !list->initialized) {
        return;
    }

    while (list->head != NULL) {
        remote_device_disconnect(list, list->head);
    }
}

/* ----- Device Lookup ----- */

remote_device_t* remote_device_find_by_port(remote_device_list_t *list, int port) {
    if (list == NULL || !list->initialized) {
        return NULL;
    }

    mutex_lock(&list->mutex);

    remote_device_t *device = list->head;
    while (device != NULL) {
        if (device->vhci_port == port) {
            mutex_unlock(&list->mutex);
            return device;
        }
        device = device->next;
    }

    mutex_unlock(&list->mutex);
    return NULL;
}

remote_device_t* remote_device_find_by_busid(remote_device_list_t *list, const char *busid) {
    if (list == NULL || busid == NULL || !list->initialized) {
        return NULL;
    }

    mutex_lock(&list->mutex);

    remote_device_t *device = list->head;
    while (device != NULL) {
        if (strcmp(device->busid, busid) == 0) {
            mutex_unlock(&list->mutex);
            return device;
        }
        device = device->next;
    }

    mutex_unlock(&list->mutex);
    return NULL;
}

int remote_device_count(remote_device_list_t *list) {
    return (list != NULL) ? list->count : 0;
}

/* ----- URB Forwarding ----- */

static DWORD WINAPI forward_thread_func(LPVOID param) {
    remote_device_t *device = (remote_device_t *)param;

    LOG_DEBUG("Forward thread started for device %s", device->busid);

    device->state = REMOTE_DEV_STATE_ACTIVE;

    while (device->running) {
        /* In a full implementation, this thread would:
         * 1. Receive URBs from VHCI driver
         * 2. Send them to the server
         * 3. Receive responses from server
         * 4. Pass them back to VHCI driver
         *
         * For now, we just maintain the connection
         */
        Sleep(100);
    }

    LOG_DEBUG("Forward thread stopped for device %s", device->busid);
    return 0;
}

error_code_t remote_device_start_forwarding(remote_device_t *device) {
    if (device == NULL || device->running) {
        return ERR_INVALID_PARAM;
    }

    device->running = true;

    if (thread_create(&device->forward_thread, forward_thread_func, device) != 0) {
        device->running = false;
        return ERR_THREAD_CREATE;
    }

    return ERR_SUCCESS;
}

void remote_device_stop_forwarding(remote_device_t *device) {
    if (device == NULL || !device->running) {
        return;
    }

    device->running = false;
    thread_join(device->forward_thread);
    CloseHandle(device->forward_thread);
}

/* ----- Status & Utility ----- */

const char* remote_device_state_string(remote_device_state_t state) {
    switch (state) {
        case REMOTE_DEV_STATE_DISCONNECTED: return "Disconnected";
        case REMOTE_DEV_STATE_CONNECTING:   return "Connecting";
        case REMOTE_DEV_STATE_CONNECTED:    return "Connected";
        case REMOTE_DEV_STATE_ACTIVE:       return "Active";
        case REMOTE_DEV_STATE_ERROR:        return "Error";
        default:                            return "Unknown";
    }
}

void remote_device_print(const remote_device_t *device) {
    if (device == NULL) {
        return;
    }

    printf("  Port %d: %s\n", device->vhci_port, device->busid);
    printf("    VID:PID: %04X:%04X\n",
        device->device_info.idVendor, device->device_info.idProduct);
    printf("    Server: %s:%u\n", device->server_ip, device->server_port);
    printf("    State: %s\n", remote_device_state_string(device->state));
    printf("    URBs: sent=%llu recv=%llu\n", device->urbs_sent, device->urbs_received);
}

void remote_device_list_print(remote_device_list_t *list) {
    if (list == NULL || !list->initialized) {
        printf("Remote device list not initialized\n");
        return;
    }

    printf("\nAttached Devices (%d):\n", list->count);
    printf("---------------------\n");

    if (list->count == 0) {
        printf("  No devices attached\n");
    } else {
        mutex_lock(&list->mutex);
        remote_device_t *device = list->head;
        while (device != NULL) {
            remote_device_print(device);
            device = device->next;
        }
        mutex_unlock(&list->mutex);
    }
    printf("\n");
}

/* ----- Server Communication ----- */

error_code_t remote_server_list_devices(const char *server_ip, uint16_t server_port,
                                         usbip_usb_device_t *devices, int *device_count,
                                         int max_devices) {
    if (server_ip == NULL || devices == NULL || device_count == NULL) {
        return ERR_INVALID_PARAM;
    }

    *device_count = 0;

    /* Connect to server */
    socket_t sock = tcp_client_connect_timeout(server_ip, server_port, CONNECTION_TIMEOUT_MS);
    if (!socket_is_valid(sock)) {
        LOG_ERROR("Failed to connect to server %s:%u", server_ip, server_port);
        return ERR_SOCKET_CONNECT;
    }

    /* Send device list request */
    usbip_header_basic_t request;
    usbip_create_devlist_request(&request);
    usbip_pack_header_basic(&request);

    if (net_send_all(sock, &request, sizeof(request)) != sizeof(request)) {
        socket_close(sock);
        return ERR_SOCKET_SEND;
    }

    /* Receive response header */
    usbip_header_basic_t reply_header;
    if (usbip_recv_header_basic(sock, &reply_header) != ERR_SUCCESS) {
        socket_close(sock);
        return ERR_SOCKET_RECV;
    }

    if (reply_header.status != USBIP_ST_OK) {
        socket_close(sock);
        return ERR_PROTOCOL_INVALID;
    }

    /* Receive device count */
    uint32_t ndev_net;
    if (net_recv_all(sock, &ndev_net, sizeof(ndev_net)) != sizeof(ndev_net)) {
        socket_close(sock);
        return ERR_SOCKET_RECV;
    }
    uint32_t ndev = be32toh(ndev_net);

    LOG_DEBUG("Server has %u devices", ndev);

    /* Receive each device */
    for (uint32_t i = 0; i < ndev && *device_count < max_devices; i++) {
        usbip_usb_device_t *dev = &devices[*device_count];

        if (usbip_recv_device(sock, dev) != ERR_SUCCESS) {
            LOG_WARN("Failed to receive device %u", i);
            break;
        }

        /* Skip interface info for now */
        for (uint8_t j = 0; j < dev->bNumInterfaces; j++) {
            usbip_usb_interface_t iface;
            if (usbip_recv_interface(sock, &iface) != ERR_SUCCESS) {
                break;
            }
        }

        (*device_count)++;
    }

    socket_close(sock);

    LOG_INFO("Received %d device(s) from server", *device_count);
    return ERR_SUCCESS;
}

void remote_server_print_devices(const usbip_usb_device_t *devices, int device_count) {
    if (devices == NULL || device_count == 0) {
        printf("No devices available.\n");
        return;
    }

    printf("\nAvailable devices on server:\n");
    printf("----------------------------\n");

    for (int i = 0; i < device_count; i++) {
        const usbip_usb_device_t *dev = &devices[i];
        printf("[%d] %s: %04X:%04X\n", i + 1, dev->busid, dev->idVendor, dev->idProduct);
        printf("    Path: %s\n", dev->path);
        printf("    Class: %s (%02X/%02X/%02X)\n",
            usb_class_string(dev->bDeviceClass),
            dev->bDeviceClass, dev->bDeviceSubClass, dev->bDeviceProtocol);
        printf("    Speed: %s\n", usb_speed_string(dev->speed));
        printf("    Interfaces: %u\n", dev->bNumInterfaces);
        printf("\n");
    }
}
