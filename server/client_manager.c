/*
 * USB Over Network - Client Manager Implementation
 * Windows-only implementation
 */

#include "client_manager.h"
#include "../common/log.h"
#include "../common/protocol.h"
#include "../common/config.h"
#include <time.h>

/* ----- Helper Functions ----- */

static uint64_t get_timestamp(void) {
    return GetTickCount64();
}

static client_connection_t* create_client(uint32_t id, socket_t socket,
                                           const char *ip_address, uint16_t port) {
    client_connection_t *client = (client_connection_t *)calloc(1, sizeof(client_connection_t));
    if (client == NULL) {
        return NULL;
    }

    client->id = id;
    client->socket = socket;
    if (ip_address) {
        strncpy(client->ip_address, ip_address, sizeof(client->ip_address) - 1);
    }
    client->port = port;
    client->state = CLIENT_STATE_CONNECTED;
    client->connect_time = get_timestamp();

    return client;
}

static void free_client(client_connection_t *client) {
    if (client == NULL) {
        return;
    }

    /* Stop threads if running */
    client_stop_threads(client);

    /* Close socket */
    if (socket_is_valid(client->socket)) {
        socket_close(client->socket);
        client->socket = INVALID_SOCKET_VAL;
    }

    /* Cleanup URB handler */
    if (client->urb_handler != NULL) {
        urb_handler_cleanup(client->urb_handler);
        free(client->urb_handler);
        client->urb_handler = NULL;
    }

    /* Close USB device */
    if (client->device != NULL) {
        usb_close_device(client->device);
        client->device = NULL;
    }

    free(client);
}

/* ----- Initialization ----- */

error_code_t client_manager_init(client_manager_t *manager, device_list_t *device_list) {
    if (manager == NULL) {
        return ERR_INVALID_PARAM;
    }

    memset(manager, 0, sizeof(client_manager_t));
    manager->device_list = device_list;
    manager->next_client_id = 1;

    if (mutex_init(&manager->mutex) != 0) {
        LOG_ERROR("Failed to initialize client manager mutex");
        return ERR_MUTEX_INIT;
    }

    manager->initialized = true;
    LOG_DEBUG("Client manager initialized");

    return ERR_SUCCESS;
}

void client_manager_cleanup(client_manager_t *manager) {
    if (manager == NULL || !manager->initialized) {
        return;
    }

    /* Disconnect all clients */
    client_manager_disconnect_all(manager);

    mutex_destroy(&manager->mutex);
    manager->initialized = false;

    LOG_DEBUG("Client manager cleaned up");
}

/* ----- Client Management ----- */

error_code_t client_manager_add(client_manager_t *manager, socket_t socket,
                                 const char *ip_address, uint16_t port,
                                 client_connection_t **client) {
    if (manager == NULL || !manager->initialized) {
        return ERR_INVALID_PARAM;
    }

    if (manager->client_count >= MAX_CLIENTS) {
        LOG_WARN("Maximum clients reached");
        return ERR_BUSY;
    }

    mutex_lock(&manager->mutex);

    uint32_t id = manager->next_client_id++;
    client_connection_t *new_client = create_client(id, socket, ip_address, port);
    if (new_client == NULL) {
        mutex_unlock(&manager->mutex);
        return ERR_OUT_OF_MEMORY;
    }

    /* Add to list */
    new_client->next = manager->clients;
    manager->clients = new_client;
    manager->client_count++;

    if (client != NULL) {
        *client = new_client;
    }

    mutex_unlock(&manager->mutex);

    LOG_INFO("Client connected: ID=%u IP=%s:%u", id, ip_address, port);

    return ERR_SUCCESS;
}

error_code_t client_manager_remove(client_manager_t *manager, client_connection_t *client) {
    if (manager == NULL || client == NULL || !manager->initialized) {
        return ERR_INVALID_PARAM;
    }

    mutex_lock(&manager->mutex);

    client_connection_t *prev = NULL;
    client_connection_t *curr = manager->clients;

    while (curr != NULL) {
        if (curr == client) {
            /* Unlink from list */
            if (prev == NULL) {
                manager->clients = curr->next;
            } else {
                prev->next = curr->next;
            }
            manager->client_count--;

            /* Release device if attached */
            if (client->attached_busid[0] != '\0') {
                device_list_unexport(manager->device_list, client->attached_busid);
            }

            LOG_INFO("Client disconnected: ID=%u IP=%s", client->id, client->ip_address);

            mutex_unlock(&manager->mutex);

            free_client(client);
            return ERR_SUCCESS;
        }
        prev = curr;
        curr = curr->next;
    }

    mutex_unlock(&manager->mutex);
    return ERR_NOT_FOUND;
}

error_code_t client_manager_remove_by_id(client_manager_t *manager, uint32_t client_id) {
    client_connection_t *client = client_manager_find(manager, client_id);
    if (client == NULL) {
        return ERR_NOT_FOUND;
    }
    return client_manager_remove(manager, client);
}

client_connection_t* client_manager_find(client_manager_t *manager, uint32_t client_id) {
    if (manager == NULL || !manager->initialized) {
        return NULL;
    }

    mutex_lock(&manager->mutex);

    client_connection_t *client = manager->clients;
    while (client != NULL) {
        if (client->id == client_id) {
            mutex_unlock(&manager->mutex);
            return client;
        }
        client = client->next;
    }

    mutex_unlock(&manager->mutex);
    return NULL;
}

client_connection_t* client_manager_find_by_socket(client_manager_t *manager, socket_t socket) {
    if (manager == NULL || !manager->initialized) {
        return NULL;
    }

    mutex_lock(&manager->mutex);

    client_connection_t *client = manager->clients;
    while (client != NULL) {
        if (client->socket == socket) {
            mutex_unlock(&manager->mutex);
            return client;
        }
        client = client->next;
    }

    mutex_unlock(&manager->mutex);
    return NULL;
}

client_connection_t* client_manager_find_by_device(client_manager_t *manager, const char *busid) {
    if (manager == NULL || busid == NULL || !manager->initialized) {
        return NULL;
    }

    mutex_lock(&manager->mutex);

    client_connection_t *client = manager->clients;
    while (client != NULL) {
        if (strcmp(client->attached_busid, busid) == 0) {
            mutex_unlock(&manager->mutex);
            return client;
        }
        client = client->next;
    }

    mutex_unlock(&manager->mutex);
    return NULL;
}

int client_manager_count(client_manager_t *manager) {
    return (manager != NULL) ? manager->client_count : 0;
}

/* ----- Client State ----- */

void client_set_state(client_connection_t *client, client_state_t state) {
    if (client != NULL) {
        client->state = state;
        LOG_DEBUG("Client %u state: %s", client->id, client_state_string(state));
    }
}

const char* client_state_string(client_state_t state) {
    switch (state) {
        case CLIENT_STATE_CONNECTED:     return "Connected";
        case CLIENT_STATE_ATTACHED:      return "Attached";
        case CLIENT_STATE_DISCONNECTING: return "Disconnecting";
        case CLIENT_STATE_DISCONNECTED:  return "Disconnected";
        default:                         return "Unknown";
    }
}

/* ----- Device Attachment ----- */

error_code_t client_attach_device(client_manager_t *manager, client_connection_t *client,
                                   const char *busid) {
    if (manager == NULL || client == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    LOG_INFO("Attaching device '%s' to client %u", busid, client->id);

    /* Find device in list */
    device_entry_t *entry = device_list_find(manager->device_list, busid);
    if (entry == NULL) {
        LOG_WARN("Device not found in list: '%s'", busid);
        /* Debug: list all devices */
        device_entry_t *e = manager->device_list->head;
        while (e) {
            LOG_DEBUG("  Available device: '%s' shared=%d state=%d",
                e->device.busid, e->is_shared, e->state);
            e = e->next;
        }
        return ERR_USB_NOT_FOUND;
    }

    /* Check if shared */
    if (!entry->is_shared) {
        LOG_WARN("Device not shared: '%s'", busid);
        return ERR_USB_NOT_FOUND;
    }

    /* Check if available */
    if (entry->state != DEVICE_STATE_AVAILABLE) {
        LOG_WARN("Device not available (state=%d): '%s'", entry->state, busid);
        return ERR_DEVICE_BUSY;
    }

    LOG_INFO("Device found and available, opening...");

    /* Open USB device */
    error_code_t err = usb_open_device(&entry->device);
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Failed to open device: %s (err=%d)", busid, err);
        return err;
    }

    LOG_INFO("Device opened (virtual=%d), claiming interface...", entry->device.is_virtual);

    /* Claim interface */
    err = usb_claim_interface(&entry->device, 0);
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Failed to claim interface: %s (err=%d)", busid, err);
        usb_close_device(&entry->device);
        return err;
    }

    LOG_INFO("Interface claimed, creating URB handler...");

    /* Create URB handler */
    client->urb_handler = (urb_handler_t *)calloc(1, sizeof(urb_handler_t));
    if (client->urb_handler == NULL) {
        usb_release_interface(&entry->device, 0);
        usb_close_device(&entry->device);
        return ERR_OUT_OF_MEMORY;
    }

    err = urb_handler_init(client->urb_handler, &entry->device);
    if (err != ERR_SUCCESS) {
        LOG_ERROR("URB handler init failed: %s (err=%d)", busid, err);
        free(client->urb_handler);
        client->urb_handler = NULL;
        usb_release_interface(&entry->device, 0);
        usb_close_device(&entry->device);
        return err;
    }

    LOG_INFO("URB handler initialized, starting...");

    /* Start URB handler */
    err = urb_handler_start(client->urb_handler);
    if (err != ERR_SUCCESS) {
        LOG_ERROR("URB handler start failed: %s (err=%d)", busid, err);
        urb_handler_cleanup(client->urb_handler);
        free(client->urb_handler);
        client->urb_handler = NULL;
        usb_release_interface(&entry->device, 0);
        usb_close_device(&entry->device);
        return err;
    }

    LOG_INFO("URB handler started successfully");

    /* Update client */
    client->device = &entry->device;
    strncpy(client->attached_busid, busid, sizeof(client->attached_busid) - 1);
    client_set_state(client, CLIENT_STATE_ATTACHED);

    /* Mark device as exported */
    device_list_export(manager->device_list, busid, client->id, client->ip_address);

    LOG_INFO("Device %s attached to client %u", busid, client->id);

    return ERR_SUCCESS;
}

error_code_t client_detach_device(client_manager_t *manager, client_connection_t *client) {
    if (manager == NULL || client == NULL) {
        return ERR_INVALID_PARAM;
    }

    if (client->attached_busid[0] == '\0') {
        return ERR_SUCCESS;  /* Already detached */
    }

    LOG_DEBUG("Detaching device %s from client %u", client->attached_busid, client->id);

    /* Stop URB handler */
    if (client->urb_handler != NULL) {
        urb_handler_stop(client->urb_handler);
        urb_handler_cleanup(client->urb_handler);
        free(client->urb_handler);
        client->urb_handler = NULL;
    }

    /* Release and close device */
    if (client->device != NULL) {
        usb_release_interface(client->device, 0);
        usb_close_device(client->device);
        client->device = NULL;
    }

    /* Update device list */
    device_list_unexport(manager->device_list, client->attached_busid);

    LOG_INFO("Device %s detached from client %u", client->attached_busid, client->id);

    client->attached_busid[0] = '\0';
    client_set_state(client, CLIENT_STATE_CONNECTED);

    return ERR_SUCCESS;
}

/* ----- Client Communication ----- */

error_code_t client_start_threads(client_connection_t *client) {
    /* Not implementing separate threads for now - using synchronous handling */
    (void)client;
    return ERR_SUCCESS;
}

void client_stop_threads(client_connection_t *client) {
    if (client != NULL) {
        client->running = false;
    }
}

error_code_t client_handle_request(client_manager_t *manager, client_connection_t *client) {
    if (manager == NULL || client == NULL) {
        return ERR_INVALID_PARAM;
    }

    usbip_header_basic_t header;
    error_code_t err = usbip_recv_header_basic(client->socket, &header);
    if (err != ERR_SUCCESS) {
        return err;
    }

    LOG_DEBUG("Client %u request: cmd=0x%04X", client->id, header.command);

    switch (header.command) {
        case OP_REQ_DEVLIST:
            return client_send_device_list(manager, client);

        case OP_REQ_IMPORT: {
            char busid[USBIP_BUSID_MAX];
            ssize_t received = net_recv_all(client->socket, busid, sizeof(busid));
            if (received != sizeof(busid)) {
                return ERR_SOCKET_RECV;
            }
            busid[sizeof(busid) - 1] = '\0';
            return client_handle_import(manager, client, busid);
        }

        default:
            LOG_WARN("Unknown command from client %u: 0x%04X", client->id, header.command);
            return ERR_PROTOCOL_UNSUPPORTED;
    }
}

/* Callback for sending device list */
typedef struct {
    socket_t socket;
    int count;
    error_code_t error;
} send_devlist_data_t;

static bool send_device_callback(device_entry_t *entry, void *user_data) {
    send_devlist_data_t *data = (send_devlist_data_t *)user_data;

    usbip_usb_device_t usbip_dev;
    usb_device_to_usbip(&entry->device, &usbip_dev);

    if (usbip_send_device(data->socket, &usbip_dev) != ERR_SUCCESS) {
        data->error = ERR_SOCKET_SEND;
        return false;
    }

    /* Send interface info */
    for (int i = 0; i < entry->device.num_interfaces && i < USB_MAX_INTERFACES; i++) {
        usbip_usb_interface_t usbip_iface;
        usb_interface_to_usbip(&entry->device.interfaces[i], &usbip_iface);

        if (usbip_send_interface(data->socket, &usbip_iface) != ERR_SUCCESS) {
            data->error = ERR_SOCKET_SEND;
            return false;
        }
    }

    data->count++;
    return true;
}

error_code_t client_send_device_list(client_manager_t *manager, client_connection_t *client) {
    if (manager == NULL || client == NULL) {
        return ERR_INVALID_PARAM;
    }

    LOG_DEBUG("Sending device list to client %u", client->id);

    /* Count shared devices */
    int device_count = 0;
    device_list_lock(manager->device_list);

    device_entry_t *entry = manager->device_list->head;
    while (entry != NULL) {
        if (entry->is_shared && entry->state == DEVICE_STATE_AVAILABLE) {
            device_count++;
        }
        entry = entry->next;
    }

    device_list_unlock(manager->device_list);

    /* Send reply header */
    usbip_op_devlist_reply_t reply;
    usbip_create_devlist_reply(&reply, device_count);
    usbip_pack_header_basic(&reply.header);

    uint32_t ndev_net = htobe32(reply.ndev);

    if (net_send_all(client->socket, &reply.header, sizeof(reply.header)) != sizeof(reply.header)) {
        return ERR_SOCKET_SEND;
    }

    if (net_send_all(client->socket, &ndev_net, sizeof(ndev_net)) != sizeof(ndev_net)) {
        return ERR_SOCKET_SEND;
    }

    /* Send each device */
    send_devlist_data_t data = {
        .socket = client->socket,
        .count = 0,
        .error = ERR_SUCCESS
    };

    device_list_foreach_shared(manager->device_list, send_device_callback, &data);

    if (data.error != ERR_SUCCESS) {
        return data.error;
    }

    LOG_INFO("Sent %d devices to client %u", data.count, client->id);

    return ERR_SUCCESS;
}

error_code_t client_handle_import(client_manager_t *manager, client_connection_t *client,
                                   const char *busid) {
    if (manager == NULL || client == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    LOG_INFO("Client %u requesting device: %s", client->id, busid);

    /* Try to attach device */
    error_code_t err = client_attach_device(manager, client, busid);

    /* Send reply */
    usbip_op_import_reply_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.header.version = USBIP_PROTO_VERSION;
    reply.header.command = OP_REP_IMPORT;

    if (err == ERR_SUCCESS) {
        reply.header.status = USBIP_ST_OK;
        usb_device_to_usbip(client->device, &reply.device);
    } else {
        reply.header.status = (err == ERR_DEVICE_BUSY) ? USBIP_ST_DEV_BUSY : USBIP_ST_NA;
    }

    usbip_pack_header_basic(&reply.header);

    /* Send header */
    if (net_send_all(client->socket, &reply.header, sizeof(reply.header)) != sizeof(reply.header)) {
        return ERR_SOCKET_SEND;
    }

    /* Send device info if successful */
    if (err == ERR_SUCCESS) {
        usbip_pack_device(&reply.device);
        if (net_send_all(client->socket, &reply.device, sizeof(reply.device)) != sizeof(reply.device)) {
            client_detach_device(manager, client);
            return ERR_SOCKET_SEND;
        }
    }

    return err;
}

/* ----- URB Forwarding ----- */

error_code_t client_forward_urb(client_connection_t *client, const usbip_header_t *header,
                                 const uint8_t *data, uint32_t data_len) {
    if (client == NULL || header == NULL || client->urb_handler == NULL) {
        return ERR_INVALID_PARAM;
    }

    client->urbs_processed++;

    return urb_handler_submit(client->urb_handler, header, data, data_len);
}

error_code_t client_send_urb_completion(client_connection_t *client, urb_entry_t *entry) {
    if (client == NULL || entry == NULL) {
        return ERR_INVALID_PARAM;
    }

    usbip_header_t ret_header;
    urb_handler_create_return(entry, &ret_header);

    error_code_t err = usbip_send_urb_header(client->socket, &ret_header);
    if (err != ERR_SUCCESS) {
        return err;
    }

    /* Send data for IN transfers */
    if (entry->urb.direction == USBIP_DIR_IN && entry->urb.actual_length > 0) {
        err = usbip_send_urb_data(client->socket, entry->urb.buffer, entry->urb.actual_length);
        if (err != ERR_SUCCESS) {
            return err;
        }
        client->bytes_sent += entry->urb.actual_length;
    }

    return ERR_SUCCESS;
}

/* ----- Iteration ----- */

void client_manager_foreach(client_manager_t *manager, client_callback_t callback, void *user_data) {
    if (manager == NULL || callback == NULL || !manager->initialized) {
        return;
    }

    mutex_lock(&manager->mutex);

    client_connection_t *client = manager->clients;
    while (client != NULL) {
        client_connection_t *next = client->next;
        if (!callback(client, user_data)) {
            break;
        }
        client = next;
    }

    mutex_unlock(&manager->mutex);
}

/* ----- Utility ----- */

void client_print_info(const client_connection_t *client) {
    if (client == NULL) {
        return;
    }

    LOG_INFO("Client %u:", client->id);
    LOG_INFO("  Address: %s:%u", client->ip_address, client->port);
    LOG_INFO("  State: %s", client_state_string(client->state));
    if (client->attached_busid[0] != '\0') {
        LOG_INFO("  Device: %s", client->attached_busid);
    }
    LOG_INFO("  URBs: %llu", client->urbs_processed);
    LOG_INFO("  Sent: %llu bytes", client->bytes_sent);
    LOG_INFO("  Received: %llu bytes", client->bytes_received);
}

void client_manager_print(client_manager_t *manager) {
    if (manager == NULL || !manager->initialized) {
        return;
    }

    LOG_INFO("Client Manager (%d clients):", manager->client_count);
    LOG_INFO("----------------------------------------");

    mutex_lock(&manager->mutex);

    client_connection_t *client = manager->clients;
    while (client != NULL) {
        client_print_info(client);
        client = client->next;
    }

    LOG_INFO("----------------------------------------");

    mutex_unlock(&manager->mutex);
}

void client_manager_disconnect_all(client_manager_t *manager) {
    if (manager == NULL || !manager->initialized) {
        return;
    }

    LOG_INFO("Disconnecting all clients");

    while (manager->clients != NULL) {
        client_manager_remove(manager, manager->clients);
    }
}

void client_manager_disconnect_device_users(client_manager_t *manager, const char *busid) {
    if (manager == NULL || busid == NULL || !manager->initialized) {
        return;
    }

    client_connection_t *client = client_manager_find_by_device(manager, busid);
    if (client != NULL) {
        LOG_INFO("Disconnecting client %u using device %s", client->id, busid);
        client_manager_remove(manager, client);
    }
}
