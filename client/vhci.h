/*
 * USB Over Network - Virtual Host Controller Interface
 * Windows-only implementation
 *
 * VHCI abstraction for attaching remote USB devices locally
 */

#ifndef VHCI_H
#define VHCI_H

#include "../common/types.h"
#include "../common/error.h"
#include "../common/protocol.h"

/* VHCI port states */
typedef enum vhci_port_state {
    VHCI_PORT_FREE = 0,         /* Port is available */
    VHCI_PORT_CONNECTING,       /* Port is being connected */
    VHCI_PORT_CONNECTED,        /* Device attached to port */
    VHCI_PORT_DISCONNECTING,    /* Port is being disconnected */
    VHCI_PORT_ERROR             /* Port is in error state */
} vhci_port_state_t;

/* VHCI port information */
typedef struct vhci_port_info {
    int port_number;            /* Port number */
    vhci_port_state_t state;    /* Current state */
    char busid[USBIP_BUSID_MAX];/* Remote device bus ID */
    char server_ip[64];         /* Server IP address */
    uint16_t server_port;       /* Server port */
    uint32_t devid;             /* Device ID */
    uint32_t speed;             /* USB speed */
} vhci_port_info_t;

/* VHCI driver context */
typedef struct vhci_context {
    int max_ports;              /* Maximum number of ports */
    vhci_port_info_t *ports;    /* Port array */
    bool initialized;           /* Initialization flag */
    void *driver_handle;        /* Driver handle (Windows) */
} vhci_context_t;

/* ----- Initialization ----- */

/* Initialize VHCI subsystem */
error_code_t vhci_init(vhci_context_t *ctx);

/* Cleanup VHCI subsystem */
void vhci_cleanup(vhci_context_t *ctx);

/* Check if VHCI driver is available */
bool vhci_is_available(void);

/* ----- Port Management ----- */

/* Get number of available ports */
int vhci_get_port_count(vhci_context_t *ctx);

/* Find a free port */
int vhci_find_free_port(vhci_context_t *ctx);

/* Get port information */
error_code_t vhci_get_port_info(vhci_context_t *ctx, int port, vhci_port_info_t *info);

/* Get all ports status */
error_code_t vhci_get_status(vhci_context_t *ctx);

/* ----- Device Attachment ----- */

/* Attach remote device to VHCI port */
error_code_t vhci_attach(vhci_context_t *ctx, int port, socket_t socket,
                          const usbip_usb_device_t *device,
                          const char *server_ip, uint16_t server_port);

/* Detach device from VHCI port */
error_code_t vhci_detach(vhci_context_t *ctx, int port);

/* Detach all devices */
void vhci_detach_all(vhci_context_t *ctx);

/* ----- Status & Utility ----- */

/* Get port state string */
const char* vhci_port_state_string(vhci_port_state_t state);

/* Print VHCI status */
void vhci_print_status(vhci_context_t *ctx);

/* Check if port is connected */
bool vhci_is_port_connected(vhci_context_t *ctx, int port);

#endif /* VHCI_H */
