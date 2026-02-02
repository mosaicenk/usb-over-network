/*
 * USB Over Network - Client Manager
 * Windows-only implementation
 *
 * Management of connected clients
 */

#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include "../common/types.h"
#include "../common/error.h"
#include "../common/network.h"
#include "device_list.h"
#include "urb_handler.h"

/* Client states */
typedef enum client_state {
    CLIENT_STATE_CONNECTED = 0,     /* Just connected, browsing */
    CLIENT_STATE_ATTACHED,          /* Device attached, forwarding URBs */
    CLIENT_STATE_DISCONNECTING,     /* Disconnecting */
    CLIENT_STATE_DISCONNECTED       /* Disconnected */
} client_state_t;

/* Client connection structure */
typedef struct client_connection {
    uint32_t id;                    /* Unique client ID */
    socket_t socket;                /* Client socket */
    char ip_address[64];            /* Client IP address */
    uint16_t port;                  /* Client port */
    client_state_t state;           /* Current state */

    /* Attached device info */
    char attached_busid[USBIP_BUSID_MAX];   /* Bus ID of attached device */
    usb_device_t *device;                    /* Pointer to USB device */
    urb_handler_t *urb_handler;              /* URB handler for this device */

    /* Threading */
    thread_t recv_thread;           /* Receive thread */
    thread_t send_thread;           /* Send thread */
    bool running;                   /* Threads running flag */

    /* Statistics */
    uint64_t connect_time;          /* Connection timestamp */
    uint64_t bytes_sent;            /* Bytes sent to client */
    uint64_t bytes_received;        /* Bytes received from client */
    uint64_t urbs_processed;        /* Number of URBs processed */

    /* Internal */
    struct client_connection *next; /* Next client in list */
} client_connection_t;

/* Client manager structure */
typedef struct client_manager {
    client_connection_t *clients;   /* Linked list of clients */
    int client_count;               /* Number of connected clients */
    uint32_t next_client_id;        /* Next client ID to assign */
    device_list_t *device_list;     /* Reference to device list */
    mutex_t mutex;                  /* Thread safety */
    bool initialized;               /* Initialization flag */
} client_manager_t;

/* ----- Initialization ----- */

/* Initialize client manager */
error_code_t client_manager_init(client_manager_t *manager, device_list_t *device_list);

/* Cleanup client manager */
void client_manager_cleanup(client_manager_t *manager);

/* ----- Client Management ----- */

/* Add new client connection */
error_code_t client_manager_add(client_manager_t *manager, socket_t socket,
                                 const char *ip_address, uint16_t port,
                                 client_connection_t **client);

/* Remove client connection */
error_code_t client_manager_remove(client_manager_t *manager, client_connection_t *client);

/* Remove client by ID */
error_code_t client_manager_remove_by_id(client_manager_t *manager, uint32_t client_id);

/* Find client by ID */
client_connection_t* client_manager_find(client_manager_t *manager, uint32_t client_id);

/* Find client by socket */
client_connection_t* client_manager_find_by_socket(client_manager_t *manager, socket_t socket);

/* Find client by attached device */
client_connection_t* client_manager_find_by_device(client_manager_t *manager, const char *busid);

/* Get client count */
int client_manager_count(client_manager_t *manager);

/* ----- Client State ----- */

/* Set client state */
void client_set_state(client_connection_t *client, client_state_t state);

/* Get client state string */
const char* client_state_string(client_state_t state);

/* ----- Device Attachment ----- */

/* Attach device to client */
error_code_t client_attach_device(client_manager_t *manager, client_connection_t *client,
                                   const char *busid);

/* Detach device from client */
error_code_t client_detach_device(client_manager_t *manager, client_connection_t *client);

/* ----- Client Communication ----- */

/* Start client communication threads */
error_code_t client_start_threads(client_connection_t *client);

/* Stop client communication threads */
void client_stop_threads(client_connection_t *client);

/* Handle client request (device list, import, etc.) */
error_code_t client_handle_request(client_manager_t *manager, client_connection_t *client);

/* Send device list to client */
error_code_t client_send_device_list(client_manager_t *manager, client_connection_t *client);

/* Handle import request from client */
error_code_t client_handle_import(client_manager_t *manager, client_connection_t *client,
                                   const char *busid);

/* ----- URB Forwarding ----- */

/* Forward URB from client to device */
error_code_t client_forward_urb(client_connection_t *client, const usbip_header_t *header,
                                 const uint8_t *data, uint32_t data_len);

/* Send URB completion to client */
error_code_t client_send_urb_completion(client_connection_t *client, urb_entry_t *entry);

/* ----- Iteration ----- */

/* Callback for client iteration */
typedef bool (*client_callback_t)(client_connection_t *client, void *user_data);

/* Iterate over all clients */
void client_manager_foreach(client_manager_t *manager, client_callback_t callback, void *user_data);

/* ----- Utility ----- */

/* Print client info */
void client_print_info(const client_connection_t *client);

/* Print all clients */
void client_manager_print(client_manager_t *manager);

/* Disconnect all clients */
void client_manager_disconnect_all(client_manager_t *manager);

/* Disconnect clients using specific device */
void client_manager_disconnect_device_users(client_manager_t *manager, const char *busid);

#endif /* CLIENT_MANAGER_H */
